/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Connection.h"

#include <memory>
#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>
#include <wtf/threads/BinarySemaphore.h>

namespace IPC {

struct WaitForMessageState {
    WaitForMessageState(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, unsigned waitForMessageFlags)
        : messageReceiverName(messageReceiverName)
        , messageName(messageName)
        , destinationID(destinationID)
        , waitForMessageFlags(waitForMessageFlags)
    {
    }

    StringReference messageReceiverName;
    StringReference messageName;
    uint64_t destinationID;

    unsigned waitForMessageFlags;
    bool messageWaitingInterrupted = false;

    std::unique_ptr<MessageDecoder> decoder;
};

class Connection::SyncMessageState {
public:
    static SyncMessageState& singleton();

    SyncMessageState();
    ~SyncMessageState() = delete;

    void wakeUpClientRunLoop()
    {
        m_waitForSyncReplySemaphore.signal();
    }

    bool wait(double absoluteTime)
    {
        return m_waitForSyncReplySemaphore.wait(absoluteTime);
    }

    // Returns true if this message will be handled on a client thread that is currently
    // waiting for a reply to a synchronous message.
    bool processIncomingMessage(Connection&, std::unique_ptr<MessageDecoder>&);

    // Dispatch pending sync messages. if allowedConnection is not null, will only dispatch messages
    // from that connection and put the other messages back in the queue.
    void dispatchMessages(Connection* allowedConnection);

private:
    void dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection&);

    BinarySemaphore m_waitForSyncReplySemaphore;

    // Protects m_didScheduleDispatchMessagesWorkSet and m_messagesToDispatchWhileWaitingForSyncReply.
    std::mutex m_mutex;

    // The set of connections for which we've scheduled a call to dispatchMessageAndResetDidScheduleDispatchMessagesForConnection.
    HashSet<RefPtr<Connection>> m_didScheduleDispatchMessagesWorkSet;

    struct ConnectionAndIncomingMessage {
        Ref<Connection> connection;
        std::unique_ptr<MessageDecoder> message;
    };
    Vector<ConnectionAndIncomingMessage> m_messagesToDispatchWhileWaitingForSyncReply;
};

class Connection::SecondaryThreadPendingSyncReply {
public:
    // The reply decoder, will be null if there was an error processing the sync message on the other side.
    std::unique_ptr<MessageDecoder> replyDecoder;

    BinarySemaphore semaphore;
};


Connection::SyncMessageState& Connection::SyncMessageState::singleton()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<SyncMessageState> syncMessageState;

    std::call_once(onceFlag, [] {
        syncMessageState.construct();
    });

    return syncMessageState;
}

Connection::SyncMessageState::SyncMessageState()
{
}

bool Connection::SyncMessageState::processIncomingMessage(Connection& connection, std::unique_ptr<MessageDecoder>& message)
{
    if (!message->shouldDispatchMessageWhenWaitingForSyncReply())
        return false;

    ConnectionAndIncomingMessage connectionAndIncomingMessage { connection, WTF::move(message) };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_didScheduleDispatchMessagesWorkSet.add(&connection).isNewEntry) {
            RefPtr<Connection> protectedConnection(&connection);
            RunLoop::main().dispatch([this, protectedConnection] {
                dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(*protectedConnection);
            });
        }

        m_messagesToDispatchWhileWaitingForSyncReply.append(WTF::move(connectionAndIncomingMessage));
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages(Connection* allowedConnection)
{
    ASSERT(RunLoop::isMain());

    Vector<ConnectionAndIncomingMessage> messagesToDispatchWhileWaitingForSyncReply;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messagesToDispatchWhileWaitingForSyncReply.swap(messagesToDispatchWhileWaitingForSyncReply);
    }

    Vector<ConnectionAndIncomingMessage> messagesToPutBack;

    for (size_t i = 0; i < messagesToDispatchWhileWaitingForSyncReply.size(); ++i) {
        ConnectionAndIncomingMessage& connectionAndIncomingMessage = messagesToDispatchWhileWaitingForSyncReply[i];

        if (allowedConnection && allowedConnection != connectionAndIncomingMessage.connection.ptr()) {
            // This incoming message belongs to another connection and we don't want to dispatch it now
            // so mark it to be put back in the message queue.
            messagesToPutBack.append(WTF::move(connectionAndIncomingMessage));
            continue;
        }

        connectionAndIncomingMessage.connection->dispatchMessage(WTF::move(connectionAndIncomingMessage.message));
    }

    if (!messagesToPutBack.isEmpty()) {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& message : messagesToPutBack)
            m_messagesToDispatchWhileWaitingForSyncReply.append(WTF::move(message));
    }
}

void Connection::SyncMessageState::dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection& connection)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ASSERT(m_didScheduleDispatchMessagesWorkSet.contains(&connection));
        m_didScheduleDispatchMessagesWorkSet.remove(&connection);
    }

    dispatchMessages(&connection);
}

Ref<Connection> Connection::createServerConnection(Identifier identifier, Client& client)
{
    return adoptRef(*new Connection(identifier, true, client));
}

Ref<Connection> Connection::createClientConnection(Identifier identifier, Client& client)
{
    return adoptRef(*new Connection(identifier, false, client));
}

Connection::Connection(Identifier identifier, bool isServer, Client& client)
    : m_client(&client)
    , m_isServer(isServer)
    , m_syncRequestID(0)
    , m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(false)
    , m_shouldExitOnSyncMessageSendFailure(false)
    , m_didCloseOnConnectionWorkQueueCallback(0)
    , m_isConnected(false)
    , m_connectionQueue(WorkQueue::create("com.apple.IPC.ReceiveQueue"))
    , m_inSendSyncCount(0)
    , m_inDispatchMessageCount(0)
    , m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount(0)
    , m_didReceiveInvalidMessage(false)
    , m_waitingForMessage(nullptr)
    , m_shouldWaitForSyncReplies(true)
{
    ASSERT(RunLoop::isMain());

    platformInitialize(identifier);

#if HAVE(QOS_CLASSES)
    ASSERT(pthread_main_np());
    m_mainThread = pthread_self();
#endif
}

Connection::~Connection()
{
    ASSERT(!isValid());
}

void Connection::setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(bool flag)
{
    ASSERT(!m_isConnected);

    m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage = flag;
}

void Connection::setShouldExitOnSyncMessageSendFailure(bool shouldExitOnSyncMessageSendFailure)
{
    ASSERT(!m_isConnected);

    m_shouldExitOnSyncMessageSendFailure = shouldExitOnSyncMessageSendFailure;
}

void Connection::addWorkQueueMessageReceiver(StringReference messageReceiverName, WorkQueue* workQueue, WorkQueueMessageReceiver* workQueueMessageReceiver)
{
    ASSERT(RunLoop::isMain());

    RefPtr<Connection> connection(this);
    m_connectionQueue->dispatch([connection, messageReceiverName, workQueue, workQueueMessageReceiver] {
        ASSERT(!connection->m_workQueueMessageReceivers.contains(messageReceiverName));

        connection->m_workQueueMessageReceivers.add(messageReceiverName, std::make_pair(workQueue, workQueueMessageReceiver));
    });
}

void Connection::removeWorkQueueMessageReceiver(StringReference messageReceiverName)
{
    ASSERT(RunLoop::isMain());

    RefPtr<Connection> connection(this);
    m_connectionQueue->dispatch([connection, messageReceiverName] {
        ASSERT(connection->m_workQueueMessageReceivers.contains(messageReceiverName));
        connection->m_workQueueMessageReceivers.remove(messageReceiverName);
    });
}

void Connection::dispatchWorkQueueMessageReceiverMessage(WorkQueueMessageReceiver& workQueueMessageReceiver, MessageDecoder& decoder)
{
    if (!decoder.isSyncMessage()) {
        workQueueMessageReceiver.didReceiveMessage(*this, decoder);
        return;
    }

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        // FIXME: Handle this.
        decoder.markInvalid();
        return;
    }

#if HAVE(DTRACE)
    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID, decoder.UUID());
#else
    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID);
#endif

    // Hand off both the decoder and encoder to the work queue message receiver.
    workQueueMessageReceiver.didReceiveSyncMessage(*this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder.isInvalid());

    if (replyEncoder)
        sendSyncReply(WTF::move(replyEncoder));
}

void Connection::setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback callback)
{
    ASSERT(!m_isConnected);

    m_didCloseOnConnectionWorkQueueCallback = callback;    
}

void Connection::invalidate()
{
    if (!isValid()) {
        // Someone already called invalidate().
        return;
    }
    
    m_client = nullptr;

    RefPtr<Connection> protectedThis(this);
    m_connectionQueue->dispatch([protectedThis] {
        protectedThis->platformInvalidate();
    });
}

void Connection::markCurrentlyDispatchedMessageAsInvalid()
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);

    m_didReceiveInvalidMessage = true;
}

std::unique_ptr<MessageEncoder> Connection::createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID)
{
    auto encoder = std::make_unique<MessageEncoder>(messageReceiverName, messageName, destinationID);
    encoder->setIsSyncMessage(true);

    // Encode the sync request ID.
    syncRequestID = ++m_syncRequestID;
    *encoder << syncRequestID;

    return encoder;
}

bool Connection::sendMessage(std::unique_ptr<MessageEncoder> encoder, unsigned messageSendFlags, bool alreadyRecordedMessage)
{
    if (!isValid())
        return false;

    if (messageSendFlags & DispatchMessageEvenWhenWaitingForSyncReply
        && (!m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage
            || m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(true);

#if HAVE(DTRACE)
    std::unique_ptr<MessageRecorder::MessageProcessingToken> token;
    if (!alreadyRecordedMessage)
        token = MessageRecorder::recordOutgoingMessage(*this, *encoder);
#else
    UNUSED_PARAM(alreadyRecordedMessage);
#endif

    {
        std::lock_guard<std::mutex> lock(m_outgoingMessagesMutex);
        m_outgoingMessages.append(WTF::move(encoder));
    }
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    RefPtr<Connection> protectedThis(this);
    m_connectionQueue->dispatch([protectedThis] {
        protectedThis->sendOutgoingMessages();
    });
    return true;
}

bool Connection::sendSyncReply(std::unique_ptr<MessageEncoder> encoder)
{
    return sendMessage(WTF::move(encoder));
}

std::unique_ptr<MessageDecoder> Connection::waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, std::chrono::milliseconds timeout, unsigned waitForMessageFlags)
{
    ASSERT(RunLoop::isMain());

    bool hasIncomingSynchronousMessage = false;

    // First, check if this message is already in the incoming messages queue.
    {
        std::lock_guard<std::mutex> lock(m_incomingMessagesMutex);

        for (auto it = m_incomingMessages.begin(), end = m_incomingMessages.end(); it != end; ++it) {
            std::unique_ptr<MessageDecoder>& message = *it;

            if (message->messageReceiverName() == messageReceiverName && message->messageName() == messageName && message->destinationID() == destinationID) {
                std::unique_ptr<MessageDecoder> returnedMessage = WTF::move(message);

                m_incomingMessages.remove(it);
                return returnedMessage;
            }

            if (message->isSyncMessage())
                hasIncomingSynchronousMessage = true;
        }
    }

    // Don't even start waiting if we have InterruptWaitingIfSyncMessageArrives and there's a sync message already in the queue.
    if (hasIncomingSynchronousMessage && waitForMessageFlags & InterruptWaitingIfSyncMessageArrives) {
        m_waitingForMessage = nullptr;
        return nullptr;
    }

    WaitForMessageState waitingForMessage(messageReceiverName, messageName, destinationID, waitForMessageFlags);

    {
        std::lock_guard<std::mutex> lock(m_waitForMessageMutex);

        // We don't support having multiple clients waiting for messages.
        ASSERT(!m_waitingForMessage);

        m_waitingForMessage = &waitingForMessage;
    }

    // Now wait for it to be set.
    while (true) {
        std::unique_lock<std::mutex> lock(m_waitForMessageMutex);

        if (m_waitingForMessage->decoder) {
            auto decoder = WTF::move(m_waitingForMessage->decoder);
            m_waitingForMessage = nullptr;
            return decoder;
        }

        // Now we wait.
        std::cv_status status = m_waitForMessageCondition.wait_for(lock, timeout);
        // We timed out, lost our connection, or a sync message came in with InterruptWaitingIfSyncMessageArrives, so stop waiting.
        if (status == std::cv_status::timeout || m_waitingForMessage->messageWaitingInterrupted)
            break;
    }

    m_waitingForMessage = nullptr;

    return nullptr;
}

std::unique_ptr<MessageDecoder> Connection::sendSyncMessage(uint64_t syncRequestID, std::unique_ptr<MessageEncoder> encoder, std::chrono::milliseconds timeout, unsigned syncSendFlags)
{
    if (!RunLoop::isMain()) {
        // No flags are supported for synchronous messages sent from secondary threads.
        ASSERT(!syncSendFlags);
        return sendSyncMessageFromSecondaryThread(syncRequestID, WTF::move(encoder), timeout);
    }

    if (!isValid()) {
        didFailToSendSyncMessage();
        return nullptr;
    }

    // Push the pending sync reply information on our stack.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies) {
            didFailToSendSyncMessage();
            return nullptr;
        }

        m_pendingSyncReplies.append(PendingSyncReply(syncRequestID));
    }

    ++m_inSendSyncCount;

#if HAVE(DTRACE)
    auto token = MessageRecorder::recordOutgoingMessage(*this, *encoder);
#endif

    // First send the message.
    sendMessage(WTF::move(encoder), DispatchMessageEvenWhenWaitingForSyncReply, true);

    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    Ref<Connection> protect(*this);
    std::unique_ptr<MessageDecoder> reply = waitForSyncReply(syncRequestID, timeout, syncSendFlags);

    --m_inSendSyncCount;

    // Finally, pop the pending sync reply information.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(m_pendingSyncReplies.last().syncRequestID == syncRequestID);
        m_pendingSyncReplies.removeLast();
    }

    if (!reply)
        didFailToSendSyncMessage();

    return reply;
}

std::unique_ptr<MessageDecoder> Connection::sendSyncMessageFromSecondaryThread(uint64_t syncRequestID, std::unique_ptr<MessageEncoder> encoder, std::chrono::milliseconds timeout)
{
    ASSERT(!RunLoop::isMain());

    if (!isValid())
        return nullptr;

    SecondaryThreadPendingSyncReply pendingReply;

    // Push the pending sync reply information on our stack.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies)
            return nullptr;

        ASSERT(!m_secondaryThreadPendingSyncReplyMap.contains(syncRequestID));
        m_secondaryThreadPendingSyncReplyMap.add(syncRequestID, &pendingReply);
    }

#if HAVE(DTRACE)
    auto token = MessageRecorder::recordOutgoingMessage(*this, *encoder);
#endif

    sendMessage(WTF::move(encoder), 0, true);

    pendingReply.semaphore.wait(currentTime() + (timeout.count() / 1000.0));

    // Finally, pop the pending sync reply information.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(m_secondaryThreadPendingSyncReplyMap.contains(syncRequestID));
        m_secondaryThreadPendingSyncReplyMap.remove(syncRequestID);
    }

    return WTF::move(pendingReply.replyDecoder);
}

std::unique_ptr<MessageDecoder> Connection::waitForSyncReply(uint64_t syncRequestID, std::chrono::milliseconds timeout, unsigned syncSendFlags)
{
    double absoluteTime = currentTime() + (timeout.count() / 1000.0);

    willSendSyncMessage(syncSendFlags);
    
    bool timedOut = false;
    while (!timedOut) {
        // First, check if we have any messages that we need to process.
        SyncMessageState::singleton().dispatchMessages(nullptr);
        
        {
            MutexLocker locker(m_syncReplyStateMutex);

            // Second, check if there is a sync reply at the top of the stack.
            ASSERT(!m_pendingSyncReplies.isEmpty());
            
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies.last();
            ASSERT_UNUSED(syncRequestID, pendingSyncReply.syncRequestID == syncRequestID);
            
            // We found the sync reply, or the connection was closed.
            if (pendingSyncReply.didReceiveReply || !m_shouldWaitForSyncReplies) {
                didReceiveSyncReply(syncSendFlags);
                return WTF::move(pendingSyncReply.replyDecoder);
            }
        }

        // Processing a sync message could cause the connection to be invalidated.
        // (If the handler ends up calling Connection::invalidate).
        // If that happens, we need to stop waiting, or we'll hang since we won't get
        // any more incoming messages.
        if (!isValid()) {
            didReceiveSyncReply(syncSendFlags);
            return nullptr;
        }

        // We didn't find a sync reply yet, keep waiting.
        // This allows the WebProcess to still serve clients while waiting for the message to return.
        // Notably, it can continue to process accessibility requests, which are on the main thread.
        if (syncSendFlags & SpinRunLoopWhileWaitingForReply) {
#if PLATFORM(COCOA)
            // FIXME: Although we run forever, any events incoming will cause us to drop out and exit out. This however doesn't
            // account for a timeout value passed in. Timeout is always NoTimeout in these cases, but that could change.
            RunLoop::current().runForDuration(1e10);
            timedOut = currentTime() >= absoluteTime;
#endif
        } else
            timedOut = !SyncMessageState::singleton().wait(absoluteTime);
        
    }
    
    didReceiveSyncReply(syncSendFlags);

    return nullptr;
}

void Connection::processIncomingSyncReply(std::unique_ptr<MessageDecoder> decoder)
{
    MutexLocker locker(m_syncReplyStateMutex);

    // Go through the stack of sync requests that have pending replies and see which one
    // this reply is for.
    for (size_t i = m_pendingSyncReplies.size(); i > 0; --i) {
        PendingSyncReply& pendingSyncReply = m_pendingSyncReplies[i - 1];

        if (pendingSyncReply.syncRequestID != decoder->destinationID())
            continue;

        ASSERT(!pendingSyncReply.replyDecoder);

        pendingSyncReply.replyDecoder = WTF::move(decoder);
        pendingSyncReply.didReceiveReply = true;

        // We got a reply to the last send message, wake up the client run loop so it can be processed.
        if (i == m_pendingSyncReplies.size())
            SyncMessageState::singleton().wakeUpClientRunLoop();

        return;
    }

    // If it's not a reply to any primary thread message, check if it is a reply to a secondary thread one.
    SecondaryThreadPendingSyncReplyMap::iterator secondaryThreadReplyMapItem = m_secondaryThreadPendingSyncReplyMap.find(decoder->destinationID());
    if (secondaryThreadReplyMapItem != m_secondaryThreadPendingSyncReplyMap.end()) {
        SecondaryThreadPendingSyncReply* reply = secondaryThreadReplyMapItem->value;
        ASSERT(!reply->replyDecoder);
        reply->replyDecoder = WTF::move(decoder);
        reply->semaphore.signal();
    }

    // If we get here, it means we got a reply for a message that wasn't in the sync request stack or map.
    // This can happen if the send timed out, so it's fine to ignore.
}

void Connection::processIncomingMessage(std::unique_ptr<MessageDecoder> message)
{
    ASSERT(!message->messageReceiverName().isEmpty());
    ASSERT(!message->messageName().isEmpty());

    if (message->messageReceiverName() == "IPC" && message->messageName() == "SyncMessageReply") {
        processIncomingSyncReply(WTF::move(message));
        return;
    }

    if (!m_workQueueMessageReceivers.isValidKey(message->messageReceiverName())) {
        RefPtr<Connection> protectedThis(this);
        StringReference messageReceiverName = message->messageReceiverName();
        StringCapture capturedMessageReceiverName(messageReceiverName.isEmpty() ? "<unknown message receiver>" : String(messageReceiverName.data(), messageReceiverName.size()));
        StringReference messageName = message->messageName();
        StringCapture capturedMessageName(messageName.isEmpty() ? "<unknown message>" : String(messageName.data(), messageName.size()));

        RunLoop::main().dispatch([protectedThis, capturedMessageReceiverName, capturedMessageName] {
            protectedThis->dispatchDidReceiveInvalidMessage(capturedMessageReceiverName.string().utf8(), capturedMessageName.string().utf8());
        });
        return;
    }

    auto it = m_workQueueMessageReceivers.find(message->messageReceiverName());
    if (it != m_workQueueMessageReceivers.end()) {
        RefPtr<Connection> protectedThis(this);
        RefPtr<WorkQueueMessageReceiver>& workQueueMessageReceiver = it->value.second;
        MessageDecoder* decoderPtr = message.release();
        it->value.first->dispatch([protectedThis, workQueueMessageReceiver, decoderPtr] {
            std::unique_ptr<MessageDecoder> decoder(decoderPtr);
            protectedThis->dispatchWorkQueueMessageReceiverMessage(*workQueueMessageReceiver, *decoder);
        });
        return;
    }

#if HAVE(QOS_CLASSES)
    if (message->isSyncMessage() && m_shouldBoostMainThreadOnSyncMessage) {
        pthread_override_t override = pthread_override_qos_class_start_np(m_mainThread, QOS_CLASS_USER_INTERACTIVE, 0);
        message->setQOSClassOverride(override);
    }
#endif

    if (message->isSyncMessage()) {
        std::lock_guard<std::mutex> lock(m_incomingSyncMessageCallbackMutex);

        for (auto& callback : m_incomingSyncMessageCallbacks.values())
            m_incomingSyncMessageCallbackQueue->dispatch(callback);

        m_incomingSyncMessageCallbacks.clear();
    }

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (SyncMessageState::singleton().processIncomingMessage(*this, message))
        return;

    // Check if we're waiting for this message.
    {
        std::lock_guard<std::mutex> lock(m_waitForMessageMutex);

        if (m_waitingForMessage && m_waitingForMessage->messageReceiverName == message->messageReceiverName() && m_waitingForMessage->messageName == message->messageName() && m_waitingForMessage->destinationID == message->destinationID()) {
            m_waitingForMessage->decoder = WTF::move(message);
            ASSERT(m_waitingForMessage->decoder);
            m_waitForMessageCondition.notify_one();
            return;
        }

        if (m_waitingForMessage && (m_waitingForMessage->waitForMessageFlags & InterruptWaitingIfSyncMessageArrives) && message->isSyncMessage()) {
            m_waitingForMessage->messageWaitingInterrupted = true;
            m_waitForMessageCondition.notify_one();
        }
    }

    enqueueIncomingMessage(WTF::move(message));
}

uint64_t Connection::installIncomingSyncMessageCallback(std::function<void ()> callback)
{
    std::lock_guard<std::mutex> lock(m_incomingSyncMessageCallbackMutex);

    m_nextIncomingSyncMessageCallbackID++;

    if (!m_incomingSyncMessageCallbackQueue)
        m_incomingSyncMessageCallbackQueue = WorkQueue::create("com.apple.WebKit.IPC.IncomingSyncMessageCallbackQueue");

    m_incomingSyncMessageCallbacks.add(m_nextIncomingSyncMessageCallbackID, callback);

    return m_nextIncomingSyncMessageCallbackID;
}

void Connection::uninstallIncomingSyncMessageCallback(uint64_t callbackID)
{
    std::lock_guard<std::mutex> lock(m_incomingSyncMessageCallbackMutex);
    m_incomingSyncMessageCallbacks.remove(callbackID);
}

bool Connection::hasIncomingSyncMessage()
{
    std::lock_guard<std::mutex> lock(m_incomingMessagesMutex);

    for (auto& message : m_incomingMessages) {
        if (message->isSyncMessage())
            return true;
    }
    
    return false;
}

void Connection::postConnectionDidCloseOnConnectionWorkQueue()
{
    RefPtr<Connection> connection(this);
    m_connectionQueue->dispatch([connection] {
        connection->connectionDidClose();
    });
}

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    platformInvalidate();

    {
        MutexLocker locker(m_syncReplyStateMutex);

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        if (!m_pendingSyncReplies.isEmpty())
            SyncMessageState::singleton().wakeUpClientRunLoop();

        for (SecondaryThreadPendingSyncReplyMap::iterator iter = m_secondaryThreadPendingSyncReplyMap.begin(); iter != m_secondaryThreadPendingSyncReplyMap.end(); ++iter)
            iter->value->semaphore.signal();
    }

    {
        std::lock_guard<std::mutex> lock(m_waitForMessageMutex);
        if (m_waitingForMessage)
            m_waitingForMessage->messageWaitingInterrupted = true;
    }
    m_waitForMessageCondition.notify_all();

    if (m_didCloseOnConnectionWorkQueueCallback)
        m_didCloseOnConnectionWorkQueueCallback(this);

    RefPtr<Connection> connection(this);
    RunLoop::main().dispatch([connection] {
        // If the connection has been explicitly invalidated before dispatchConnectionDidClose was called,
        // then the client will be null here.
        if (!connection->m_client)
            return;

        // Because we define a connection as being "valid" based on wheter it has a null client, we null out
        // the client before calling didClose here. Otherwise, sendSync will try to send a message to the connection and
        // will then wait indefinitely for a reply.
        Client* client = connection->m_client;
        connection->m_client = nullptr;

        client->didClose(*connection);
    });
}

bool Connection::canSendOutgoingMessages() const
{
    return m_isConnected && platformCanSendOutgoingMessages();
}

void Connection::sendOutgoingMessages()
{
    if (!canSendOutgoingMessages())
        return;

    while (true) {
        std::unique_ptr<MessageEncoder> message;

        {
            std::lock_guard<std::mutex> lock(m_outgoingMessagesMutex);
            if (m_outgoingMessages.isEmpty())
                break;
            message = m_outgoingMessages.takeFirst();
        }

        if (!sendOutgoingMessage(WTF::move(message)))
            break;
    }
}

void Connection::dispatchSyncMessage(MessageDecoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        decoder.markInvalid();
        return;
    }

#if HAVE(DTRACE)
    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID, decoder.UUID());
#else
    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID);
#endif

    // Hand off both the decoder and encoder to the client.
    m_client->didReceiveSyncMessage(*this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder.isInvalid());

    if (replyEncoder)
        sendSyncReply(WTF::move(replyEncoder));
}

void Connection::dispatchDidReceiveInvalidMessage(const CString& messageReceiverNameString, const CString& messageNameString)
{
    ASSERT(RunLoop::isMain());

    if (!m_client)
        return;

    m_client->didReceiveInvalidMessage(*this, StringReference(messageReceiverNameString.data(), messageReceiverNameString.length()), StringReference(messageNameString.data(), messageNameString.length()));
}

void Connection::didFailToSendSyncMessage()
{
    if (!m_shouldExitOnSyncMessageSendFailure)
        return;

    exit(0);
}

void Connection::enqueueIncomingMessage(std::unique_ptr<MessageDecoder> incomingMessage)
{
    {
        std::lock_guard<std::mutex> lock(m_incomingMessagesMutex);
        m_incomingMessages.append(WTF::move(incomingMessage));
    }

    RefPtr<Connection> protectedThis(this);
    RunLoop::main().dispatch([protectedThis] {
        protectedThis->dispatchOneMessage();
    });
}

void Connection::dispatchMessage(MessageDecoder& decoder)
{
    m_client->didReceiveMessage(*this, decoder);
}

void Connection::dispatchMessage(std::unique_ptr<MessageDecoder> message)
{
#if HAVE(DTRACE)
    MessageRecorder::recordIncomingMessage(*this, *message);
#endif

    if (!m_client)
        return;

    m_inDispatchMessageCount++;

    if (message->shouldDispatchMessageWhenWaitingForSyncReply())
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount++;

    bool oldDidReceiveInvalidMessage = m_didReceiveInvalidMessage;
    m_didReceiveInvalidMessage = false;

    if (message->isSyncMessage())
        dispatchSyncMessage(*message);
    else
        dispatchMessage(*message);

    m_didReceiveInvalidMessage |= message->isInvalid();
    m_inDispatchMessageCount--;

    // FIXME: For Delayed synchronous messages, we should not decrement the counter until we send a response.
    // Otherwise, we would deadlock if processing the message results in a sync message back after we exit this function.
    if (message->shouldDispatchMessageWhenWaitingForSyncReply())
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount--;

    if (m_didReceiveInvalidMessage && m_client)
        m_client->didReceiveInvalidMessage(*this, message->messageReceiverName(), message->messageName());

    m_didReceiveInvalidMessage = oldDidReceiveInvalidMessage;
}

void Connection::dispatchOneMessage()
{
    std::unique_ptr<MessageDecoder> message;

    {
        std::lock_guard<std::mutex> lock(m_incomingMessagesMutex);
        if (m_incomingMessages.isEmpty())
            return;

        message = m_incomingMessages.takeFirst();
    }

    dispatchMessage(WTF::move(message));
}

void Connection::wakeUpRunLoop()
{
    RunLoop::main().wakeUp();
}

} // namespace IPC
