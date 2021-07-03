/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Download.h"

#if PLATFORM(IOS)

#import "DataReference.h"
#import <WebCore/NotImplemented.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

#if USE(CFNETWORK)
#import <CFNetwork/CFURLDownload.h>
#endif

using namespace WebCore;

namespace WebKit {

#if USE(CFNETWORK)
// FIXME: If possible, we should consider moving some callbacks off the main thread or at least
// making them asynchonous calls.
static void dispatchOnMainThread(void(^block)())
{
    if (RunLoop::isMain()) {
        block();
        return;
    }

    dispatch_sync(dispatch_get_main_queue(), block);
}

static inline Download* toDownload(const void* clientInfo)
{
    return static_cast<Download*>(const_cast<void*>(clientInfo));
}

static void setUpDownloadClient(CFURLDownloadClient& client, Download& download)
{
    memset(&client, 0, sizeof(client));
    client.clientInfo = &download;

    client.didStart = [](CFURLDownloadRef, const void* clientInfo) {
        dispatchOnMainThread(^{
            toDownload(clientInfo)->didStart();
        });
    };

    client.willSendRequest = [](CFURLDownloadRef, CFURLRequestRef request, CFURLResponseRef, const void*) -> CFURLRequestRef {
        return static_cast<CFURLRequestRef>(CFRetain(request));
    };

    client.didReceiveResponse = [](CFURLDownloadRef, CFURLResponseRef response, const void* clientInfo) {
        dispatchOnMainThread(^{
            toDownload(clientInfo)->didReceiveResponse(response);
        });
    };

    client.didReceiveData = [](CFURLDownloadRef, CFIndex length, const void* clientInfo) {
        dispatchOnMainThread(^{
            toDownload(clientInfo)->didReceiveData(length);
        });
    };

    client.shouldDecodeDataOfMIMEType = [](CFURLDownloadRef, CFStringRef encodingType, const void* clientInfo) -> Boolean {
        __block BOOL returnValue = NO;
        dispatchOnMainThread(^{
            returnValue = toDownload(clientInfo)->shouldDecodeSourceDataOfMIMEType(encodingType);
        });
        return returnValue;
    };

    client.decideDestinationWithSuggestedObjectName = [](CFURLDownloadRef downloadRef, CFStringRef objectName, const void* clientInfo) {
        dispatchOnMainThread(^{
            bool allowOverwrite;
            String destination = toDownload(clientInfo)->decideDestinationWithSuggestedFilename(objectName, allowOverwrite);
            if (!destination.isNull())
                CFURLDownloadSetDestination(downloadRef, reinterpret_cast<CFURLRef>([NSURL fileURLWithPath:destination]), allowOverwrite);
        });
    };

    client.didCreateDestination = [](CFURLDownloadRef, CFURLRef path, const void* clientInfo) {
        dispatchOnMainThread(^{
            toDownload(clientInfo)->didCreateDestination(CFURLGetString(path));
        });
    };

    client.didFinish = [](CFURLDownloadRef, const void* clientInfo) {
        dispatchOnMainThread(^{
            toDownload(clientInfo)->didFinish();
        });
    };

    client.didFail = [](CFURLDownloadRef downloadRef, CFErrorRef error, const void* clientInfo) {
        dispatchOnMainThread(^{
            auto resumeData = adoptCF(CFURLDownloadCopyResumeData(downloadRef));
            toDownload(clientInfo)->didFail(error, IPC::DataReference(resumeData ? CFDataGetBytePtr(resumeData.get()) : nullptr, resumeData ? CFDataGetLength(resumeData.get()) : 0));
        });
    };
}
#endif // USE(CFNETWORK)

void Download::start()
{
    notImplemented();
}

void Download::resume(const IPC::DataReference&, const String&, const SandboxExtension::Handle&)
{
    notImplemented();
}

void Download::startWithHandle(ResourceHandle* handle, const ResourceResponse& response)
{
#if USE(CFNETWORK)
    CFURLDownloadClient client;
    setUpDownloadClient(client, *this);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    m_download = adoptCF(CFURLDownloadCreateAndStartWithLoadingConnection(NULL, handle->releaseConnectionForDownload().get(), m_request.cfURLRequest(UpdateHTTPBody), response.cfURLResponse(), &client));
#pragma clang diagnostic pop
#else
    notImplemented();
#endif
}

void Download::cancel()
{
    notImplemented();
}

void Download::platformInvalidate()
{
    notImplemented();
}

void Download::platformDidFinish()
{
    notImplemented();
}

void Download::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void Download::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void Download::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

void Download::receivedRequestToPerformDefaultHandling(const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

void Download::receivedChallengeRejection(const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

} // namespace WebKit

#endif // PLATFORM(IOS)
