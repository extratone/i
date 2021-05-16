/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Page.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "CSSStyleSelector.h"
#include "EditorClient.h"
#include "DOMWindow.h"
#include "DragController.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "Logging.h"
#include "NetworkStateNotifier.h"
#include "Navigator.h"
#include "PageGroup.h"
#include "PluginData.h"
#include "ProgressTracker.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Settings.h"
#include "StringHash.h"
#include "TextResourceDecoder.h"
#include "Widget.h"
#include "ScriptController.h"
#include <wtf/HashMap.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DOM_STORAGE)
#include "LocalStorage.h"
#include "SessionStorage.h"
#include "StorageArea.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptDebugServer.h"
#endif

#if ENABLE(WML)
#include "WMLPageState.h"
#endif

namespace WebCore {

static HashSet<Page*>* allPages;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter pageCounter("Page");
#endif

static void networkStateChanged()
{
    Vector<RefPtr<Frame> > frames;
    
    // Get all the frames of all the pages in all the page groups
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frames.append(frame);
    }

    AtomicString eventName = networkStateNotifier().onLine() ? eventNames().onlineEvent : eventNames().offlineEvent;
    
    for (unsigned i = 0; i < frames.size(); i++) {
        Document* document = frames[i]->document();
        
        if (!document)
            continue;

        // If the document does not have a body the event should be dispatched to the document
        EventTargetNode* eventTarget = document->body();
        if (!eventTarget)
            eventTarget = document;
        
        eventTarget->dispatchEventForType(eventName, false, false);
    }
}

Page::Page(ChromeClient* chromeClient, ContextMenuClient* contextMenuClient, EditorClient* editorClient, DragClient* dragClient, InspectorClient* inspectorClient)
    : m_chrome(new Chrome(this, chromeClient))
    , m_dragCaretController(new SelectionController(0, true))
    , m_focusController(new FocusController(this))
    , m_settings(new Settings(this))
    , m_progress(new ProgressTracker)
    , m_backForwardList(BackForwardList::create(this))
    , m_editorClient(editorClient)
    , m_frameCount(0)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_inLowQualityInterpolationMode(false)
    , m_cookieEnabled(true)
    , m_areMemoryCacheClientCallsEnabled(true)
    , m_mediaVolume(1)
    , m_javaScriptURLsAreAllowed(true)
    , m_didLoadUserStyleSheet(false)
    , m_userStyleSheetModificationTime(0)
    , m_group(0)
    , m_debugger(0)
    , m_pendingUnloadEventCount(0)
    , m_pendingBeforeUnloadEventCount(0)
    , m_customHTMLTokenizerTimeDelay(-1)
    , m_customHTMLTokenizerChunkSize(-1)
{
    UNUSED_PARAM(dragClient);
    UNUSED_PARAM(contextMenuClient);
    UNUSED_PARAM(inspectorClient);

    if (!allPages) {
        allPages = new HashSet<Page*>;
        
        networkStateNotifier().setNetworkStateChangedFunction(networkStateChanged);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    JavaScriptDebugServer::shared().pageCreated(this);
#endif

#ifndef NDEBUG
    pageCounter.increment();
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    setGroupName(String());
    allPages->remove(this);
    
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->pageDestroyed();

    m_editorClient->pageDestroyed();

    m_backForwardList->close();

#ifndef NDEBUG
    pageCounter.decrement();

    // Cancel keepAlive timers, to ensure we release all Frames before exiting.
    // It's safe to do this because we prohibit closing a Page while JavaScript
    // is executing.
    Frame::cancelAllKeepAlive();
#endif
}

void Page::setMainFrame(PassRefPtr<Frame> mainFrame)
{
    ASSERT(!m_mainFrame); // Should only be called during initialization
    m_mainFrame = mainFrame;
}

BackForwardList* Page::backForwardList()
{
    return m_backForwardList.get();
}

bool Page::goBack()
{
    HistoryItem* item = m_backForwardList->backItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeBack);
        return true;
    }
    return false;
}

bool Page::goForward()
{
    HistoryItem* item = m_backForwardList->forwardItem();
    
    if (item) {
        goToItem(item, FrameLoadTypeForward);
        return true;
    }
    return false;
}

void Page::goToItem(HistoryItem* item, FrameLoadType type)
{
    // Abort any current load if we're going to a history item
    m_mainFrame->loader()->stopAllLoaders();
    m_mainFrame->loader()->goToItem(item, type);
}

void Page::setGlobalHistoryItem(HistoryItem* item)
{
    m_globalHistoryItem = item;
}

void Page::setGroupName(const String& name)
{
    if (m_group && !m_group->name().isEmpty()) {
        ASSERT(m_group != m_singlePageGroup.get());
        ASSERT(!m_singlePageGroup);
        m_group->removePage(this);
    }

    if (name.isEmpty())
        m_group = 0;
    else {
        m_singlePageGroup.clear();
        m_group = PageGroup::pageGroup(name);
        m_group->addPage(this);
    }
}

const String& Page::groupName() const
{
    DEFINE_STATIC_LOCAL(String, nullString, ());
    return m_group ? m_group->name() : nullString;
}

void Page::initGroup()
{
    ASSERT(!m_singlePageGroup);
    ASSERT(!m_group);
    m_singlePageGroup.set(new PageGroup(this));
    m_group = m_singlePageGroup.get();
}

void Page::setNeedsReapplyStyles()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->setNeedsReapplyStyles();
}

void Page::refreshPlugins(bool reload)
{
    if (!allPages)
        return;

    PluginData::refresh();

    Vector<RefPtr<Frame> > framesNeedingReload;

    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        (*it)->m_pluginData = 0;

        if (reload) {
            for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
                if (frame->loader()->containsPlugins())
                    framesNeedingReload.append(frame);
            }
        }
    }

    for (size_t i = 0; i < framesNeedingReload.size(); ++i)
        framesNeedingReload[i]->loader()->reload();
}

PluginData* Page::pluginData() const
{
    if (!settings()->arePluginsEnabled())
        return 0;
    if (!m_pluginData)
        m_pluginData = PluginData::create(this);
    return m_pluginData.get();
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

bool Page::findString(const String& target, TextCaseSensitivity caseSensitivity, FindDirection direction, bool shouldWrap)
{
    if (target.isEmpty() || !mainFrame())
        return false;

    Frame* frame = focusController()->focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
        if (frame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, false, true)) {
            if (frame != startFrame)
                startFrame->selection()->clear();
            focusController()->setFocusedFrame(frame);
            return true;
        }
        frame = incrementFrame(frame, direction == FindDirectionForward, shouldWrap);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (shouldWrap && !startFrame->selection()->isNone()) {
        bool found = startFrame->findString(target, direction == FindDirectionForward, caseSensitivity == TextCaseSensitive, true, true);
        focusController()->setFocusedFrame(frame);
        return found;
    }

    return false;
}

unsigned int Page::markAllMatchesForText(const String& target, TextCaseSensitivity caseSensitivity, bool shouldHighlight, unsigned limit)
{
    if (target.isEmpty() || !mainFrame())
        return 0;

    unsigned matches = 0;

    Frame* frame = mainFrame();
    do {
        frame->setMarkedTextMatchesAreHighlighted(shouldHighlight);
        matches += frame->markAllMatchesForText(target, caseSensitivity == TextCaseSensitive, (limit == 0) ? 0 : (limit - matches));
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return matches;
}

void Page::unmarkAllTextMatches()
{
    if (!mainFrame())
        return;

    Frame* frame = mainFrame();
    do {
        if (Document* document = frame->document())
            document->removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, false);
    } while (frame);
}

const Selection& Page::selection() const
{
    return focusController()->focusedOrMainFrame()->selection()->selection();
}

void Page::setDefersLoading(bool defers)
{
    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->setDefersLoading(defers);
}

void Page::clearUndoRedoOperations()
{
    m_editorClient->clearUndoRedoOperations();
}

bool Page::inLowQualityImageInterpolationMode() const
{
    return m_inLowQualityInterpolationMode;
}

void Page::setInLowQualityImageInterpolationMode(bool mode)
{
    m_inLowQualityInterpolationMode = mode;
}

void Page::setMediaVolume(float volume)
{
    if (volume < 0 || volume > 1)
        return;

    if (m_mediaVolume == volume)
        return;

    m_mediaVolume = volume;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            frame->document()->mediaVolumeDidChange();
    }
}

void Page::didMoveOnscreen()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->view())
            frame->view()->didMoveOnscreen();
    }
}

void Page::willMoveOffscreen()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->view())
            frame->view()->willMoveOffscreen();
    }
}

void Page::userStyleSheetLocationChanged()
{
#if !FRAME_LOADS_USER_STYLESHEET
    // FIXME: We should provide a way to load other types of URLs than just
    // file: (e.g., http:, data:).
    if (m_settings->userStyleSheetLocation().isLocalFile())
        m_userStyleSheetPath = m_settings->userStyleSheetLocation().fileSystemPath();
    else
        m_userStyleSheetPath = String();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = 0;
#endif
}

const String& Page::userStyleSheet() const
{
    if (m_userStyleSheetPath.isEmpty()) {
        ASSERT(m_userStyleSheet.isEmpty());
        return m_userStyleSheet;
    }

    time_t modTime;
    if (!getFileModificationTime(m_userStyleSheetPath, modTime)) {
        // The stylesheet either doesn't exist, was just deleted, or is
        // otherwise unreadable. If we've read the stylesheet before, we should
        // throw away that data now as it no longer represents what's on disk.
        m_userStyleSheet = String();
        return m_userStyleSheet;
    }

    // If the stylesheet hasn't changed since the last time we read it, we can
    // just return the old data.
    if (m_didLoadUserStyleSheet && modTime <= m_userStyleSheetModificationTime)
        return m_userStyleSheet;

    m_didLoadUserStyleSheet = true;
    m_userStyleSheet = String();
    m_userStyleSheetModificationTime = modTime;

    // FIXME: It would be better to load this asynchronously to avoid blocking
    // the process, but we will first need to create an asynchronous loading
    // mechanism that is not tied to a particular Frame. We will also have to
    // determine what our behavior should be before the stylesheet is loaded
    // and what should happen when it finishes loading, especially with respect
    // to when the load event fires, when Document::close is called, and when
    // layout/paint are allowed to happen.
    RefPtr<SharedBuffer> data = SharedBuffer::createWithContentsOfFile(m_userStyleSheetPath);
    if (!data)
        return m_userStyleSheet;

    m_userStyleSheet = TextResourceDecoder::create("text/css")->decode(data->data(), data->size());

    return m_userStyleSheet;
}

void Page::removeAllVisitedLinks()
{
    if (!allPages)
        return;
    HashSet<PageGroup*> groups;
    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        if (PageGroup* group = (*it)->groupPtr())
            groups.add(group);
    }
    HashSet<PageGroup*>::iterator groupsEnd = groups.end();
    for (HashSet<PageGroup*>::iterator it = groups.begin(); it != groupsEnd; ++it)
        (*it)->removeVisitedLinks();
}

void Page::allVisitedStateChanged(PageGroup* group)
{
    ASSERT(group);
    ASSERT(allPages);
    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext()) {
            if (CSSStyleSelector* styleSelector = frame->document()->styleSelector())
                styleSelector->allVisitedStateChanged();
        }
    }
}

void Page::visitedStateChanged(PageGroup* group, LinkHash visitedLinkHash)
{
    ASSERT(group);
    ASSERT(allPages);
    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext()) {
            if (CSSStyleSelector* styleSelector = frame->document()->styleSelector())
                styleSelector->visitedStateChanged(visitedLinkHash);
        }
    }
}

void Page::setDebuggerForAllPages(JSC::Debugger* debugger)
{
    ASSERT(allPages);

    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        (*it)->setDebugger(debugger);
}

void Page::setDebugger(JSC::Debugger* debugger)
{
    if (m_debugger == debugger)
        return;

    m_debugger = debugger;

    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree()->traverseNext())
        frame->script()->attachDebugger(m_debugger);
}

#if ENABLE(DOM_STORAGE)
SessionStorage* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = SessionStorage::create(this);

    return m_sessionStorage.get();
}

void Page::setSessionStorage(PassRefPtr<SessionStorage> newStorage)
{
    ASSERT(newStorage->page() == this);
    m_sessionStorage = newStorage;
}
#endif
    
unsigned Page::pendingUnloadEventCount()
{
    return m_pendingUnloadEventCount;
}
    
void Page::changePendingUnloadEventCount(int delta) 
{
    if (!delta)
        return;
    ASSERT( (delta + (int)m_pendingUnloadEventCount) >= 0 );
    
    if (m_pendingUnloadEventCount == 0)
        m_chrome->disableSuddenTermination();
    else if ((m_pendingUnloadEventCount + delta) == 0)
        m_chrome->enableSuddenTermination();
    
    m_pendingUnloadEventCount += delta;
    return; 
}
    
unsigned Page::pendingBeforeUnloadEventCount()
{
    return m_pendingBeforeUnloadEventCount;
}
    
void Page::changePendingBeforeUnloadEventCount(int delta) 
{
    if (!delta)
        return;
    ASSERT( (delta + (int)m_pendingBeforeUnloadEventCount) >= 0 );
    
    if (m_pendingBeforeUnloadEventCount == 0)
        m_chrome->disableSuddenTermination();
    else if ((m_pendingBeforeUnloadEventCount + delta) == 0)
        m_chrome->enableSuddenTermination();
    
    m_pendingBeforeUnloadEventCount += delta;
    return; 
}

#if ENABLE(WML)
WMLPageState* Page::wmlPageState()
{
    if (!m_wmlPageState)    
        m_wmlPageState.set(new WMLPageState(this));
    return m_wmlPageState.get(); 
}
#endif

void Page::setCustomHTMLTokenizerTimeDelay(double customHTMLTokenizerTimeDelay)
{
    if (customHTMLTokenizerTimeDelay < 0) {
        m_customHTMLTokenizerTimeDelay = -1;
        return;
    }
    m_customHTMLTokenizerTimeDelay = customHTMLTokenizerTimeDelay;
}

void Page::setCustomHTMLTokenizerChunkSize(int customHTMLTokenizerChunkSize)
{
    if (customHTMLTokenizerChunkSize < 0) {
        m_customHTMLTokenizerChunkSize = -1;
        return;
    }
    m_customHTMLTokenizerChunkSize = customHTMLTokenizerChunkSize;
}

void Page::setMemoryCacheClientCallsEnabled(bool enabled)
{
    if (m_areMemoryCacheClientCallsEnabled == enabled)
        return;

    m_areMemoryCacheClientCallsEnabled = enabled;
    if (!enabled)
        return;

    for (RefPtr<Frame> frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->tellClientAboutPastMemoryCacheLoads();
}

void Page::setJavaScriptURLsAreAllowed(bool areAllowed)
{
    m_javaScriptURLsAreAllowed = areAllowed;
}

bool Page::javaScriptURLsAreAllowed() const
{
    return m_javaScriptURLsAreAllowed;
}

} // namespace WebCore
