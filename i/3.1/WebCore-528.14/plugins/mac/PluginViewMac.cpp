/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LP64__

#include "config.h"
#include "PluginView.h"

#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include "wtf/RetainPtr.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "Image.h"
#include "JSDOMBinding.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "npruntime_impl.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "PluginMainThreadScheduler.h"
#include "RenderLayer.h"
#include "runtime.h"
#include "runtime_root.h"
#include "ScriptController.h"
#include "Settings.h"

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::JSValue;
using JSC::UString;

#if PLATFORM(QT)
#include <QWidget>
#include <QKeyEvent>
QT_BEGIN_NAMESPACE
extern Q_GUI_EXPORT OSWindowRef qt_mac_window_for(const QWidget *w);
QT_END_NAMESPACE
#endif

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

static int modifiersForEvent(UIEventWithKeyState *event);

static inline WindowRef nativeWindowFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget)
        return static_cast<WindowRef>(qt_mac_window_for(widget));
#endif
    return 0;
}

static inline CGContextRef cgHandleFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget)
        return (CGContextRef)widget->macCGHandle();
#endif
    return 0;
}

static inline IntPoint topLevelOffsetFor(PlatformWidget widget)
{
#if PLATFORM(QT)
    if (widget) {
        PlatformWidget topLevel = widget->window();
        return widget->mapTo(topLevel, QPoint(0, 0)) + topLevel->geometry().topLeft() - topLevel->pos();
    }
#endif
    return IntPoint();
}

// --------------- Lifetime management -----------------

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    setPlatformPluginWidget(m_parentFrame->view()->hostWindow()->platformWindow());

    m_npCgContext.window = 0;
    m_npCgContext.context = 0;
    m_npWindow.window = (void*)&m_npCgContext;
    m_npWindow.type = NPWindowTypeWindow;
    m_npWindow.x = 0;
    m_npWindow.y = 0;
    m_npWindow.width = 0;
    m_npWindow.height = 0;
    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = 0;
    m_npWindow.clipRect.bottom = 0;

    setIsNPAPIPlugin(true);

    show();

    m_status = PluginStatusLoadedSuccessfully;

    // TODO: Implement null timer throttling depending on plugin activation
    m_nullEventTimer.set(new Timer<PluginView>(this, &PluginView::nullEventTimerFired));
    m_nullEventTimer->startRepeating(0.02);
}

PluginView::~PluginView()
{
    stop();

    deleteAllValues(m_requests);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    m_parentFrame->script()->cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !(m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin)))
        m_plugin->unload();

    m_window = 0;
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    JSC::JSLock::DropAllLocks dropAllLocks(false);

    PluginMainThreadScheduler::scheduler().unregisterPlugin(m_instance);

    // Destroy the plugin
    PluginView::setCurrentPluginView(this);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->destroy(m_instance, 0);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    m_instance->pdata = 0;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    LOG(Plugin, "PluginView::getValueStatic(%d)", variable);

    switch (variable) {
    case NPNVToolkit:
        *((uint32 *)value) = 0;
        return NPERR_NO_ERROR;

    case NPNVjavascriptEnabledBool:
        *((uint32 *)value) = true;
        return NPERR_NO_ERROR;

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    LOG(Plugin, "PluginView::getValue(%d)", variable);

    switch (variable) {
    case NPNVWindowNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* windowScriptObject = m_parentFrame->script()->windowScriptNPObject();

        // Return value is expected to be retained, as described in
        // <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (windowScriptObject)
            _NPN_RetainObject(windowScriptObject);

        void** v = (void**)value;
        *v = windowScriptObject;

        return NPERR_NO_ERROR;
    }

    case NPNVPluginElementNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* pluginScriptObject = 0;

        if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
            pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

        // Return value is expected to be retained, as described in
        // <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (pluginScriptObject)
            _NPN_RetainObject(pluginScriptObject);

        void** v = (void**)value;
        *v = pluginScriptObject;

        return NPERR_NO_ERROR;
    }

    case NPNVsupportsCoreGraphicsBool:
        *((uint32 *)value) = true;
        return NPERR_NO_ERROR;

    default:
        return getValueStatic(variable, value);
    }

}
void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

// -------------- Geometry and painting ----------------

void PluginView::show()
{
    LOG(Plugin, "PluginView::show()");

    setSelfVisible(true);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(true);

    Widget::show();
}

void PluginView::hide()
{
    LOG(Plugin, "PluginView::hide()");

    setSelfVisible(false);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(false);

    Widget::hide();
}

void PluginView::setFocus()
{
    LOG(Plugin, "PluginView::setFocus()");

    if (platformPluginWidget())
       platformPluginWidget()->setFocus(Qt::OtherFocusReason);
   else
       Widget::setFocus();

    // TODO: Also handle and pass on blur events (focus lost)

    EventRecord record;
    record.what = getFocusEvent;
    record.message = 0;
    record.when = TickCount();
    record.where = globalMousePosForPlugin();
    record.modifiers = GetCurrentKeyModifiers();

    if (!dispatchNPEvent(record))
        LOG(Events, "PluginView::setFocus(): Get-focus event not accepted");
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (isSelfVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(visible);
}

void PluginView::setNPWindowRect(const IntRect&)
{
    setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    CGContextRef newContextRef = cgHandleFor(platformPluginWidget());
    if (!newContextRef)
        return;

    WindowRef newWindowRef = nativeWindowFor(platformPluginWidget());
    if (!newWindowRef)
        return;

    m_npWindow.window = (void*)&m_npCgContext;
    m_npCgContext.window = newWindowRef;
    m_npCgContext.context = newContextRef;

    m_npWindow.x = m_windowRect.x();
    m_npWindow.y = m_windowRect.y();
    m_npWindow.width = m_windowRect.width();
    m_npWindow.height = m_windowRect.height();

    // TODO: (also clip against scrollbars, etc.)
    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = m_windowRect.width();
    m_npWindow.clipRect.bottom = m_windowRect.height();

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(false);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::updatePluginWidget()
{
    if (!parent())
       return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (platformPluginWidget() && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect))
        setNPWindowIfNeeded();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

    EventRecord event;
    event.what = updateEvt;
    event.message = (long unsigned int)m_npCgContext.window;
    event.when = TickCount();
    event.where.h = 0;
    event.where.v = 0;
    event.modifiers = GetCurrentKeyModifiers();

    CGContextRef cg = m_npCgContext.context;
    CGContextSaveGState(cg);
    IntPoint offset = frameRect().location();
    CGContextTranslateCTM(cg, offset.x(), offset.y());

    if (!dispatchNPEvent(event))
        LOG(Events, "PluginView::paint(): Paint event not accepted");

    CGContextRestoreGState(cg);
}

void PluginView::invalidateRect(const IntRect& rect)
{
    if (platformPluginWidget()) {
        // TODO: optimize
        platformPluginWidget()->update();
        return;
    }
}

void PluginView::invalidateRect(NPRect* rect)
{
    // TODO: optimize
    invalidate();
}

void PluginView::invalidateRegion(NPRegion region)
{
    // TODO: optimize
    invalidate();
}

void PluginView::forceRedraw()
{
    notImplemented();
}


// ----------------- Event handling --------------------

void PluginView::handleMouseEvent(MouseEvent* event)
{
    EventRecord record;

    if (event->type() == eventNames().mousemoveEvent) {
        // Mouse movement is handled by null timer events
        return;
    } else if (event->type() == eventNames().mouseoverEvent) {
        record.what = adjustCursorEvent;
    } else if (event->type() == eventNames().mouseoutEvent) {
        record.what = adjustCursorEvent;
    } else if (event->type() == eventNames().mousedownEvent) {
        record.what = mouseDown;
        // The plugin needs focus to receive keyboard events
        if (Page* page = m_parentFrame->page())
            page->focusController()->setFocusedFrame(m_parentFrame);
        m_parentFrame->document()->setFocusedNode(m_element);
    } else if (event->type() == eventNames().mouseupEvent) {
        record.what = mouseUp;
    } else {
        return;
    }

    record.where = globalMousePosForPlugin();
    record.modifiers = modifiersForEvent(event);

    if (!event->buttonDown())
        record.modifiers |= btnState;

    if (event->button() == 2)
        record.modifiers |= controlKey;

    if (!dispatchNPEvent(record)) {
        if (record.what == adjustCursorEvent)
            return; // Signals that the plugin wants a normal cursor

        LOG(Events, "PluginView::handleMouseEvent(): Mouse event type %d at %d,%d not accepted",
                record.what, record.where.h, record.where.v);
    } else {
        event->setDefaultHandled();
    }
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    LOG(Plugin, "PluginView::handleKeyboardEvent() ----------------- ");

    LOG(Plugin, "PV::hKE(): KE.keyCode: 0x%02X, KE.charCode: %d",
            event->keyCode(), event->charCode());

    EventRecord record;

    if (event->type() == eventNames().keydownEvent) {
        // This event is the result of a PlatformKeyboardEvent::KeyDown which
        // was disambiguated into a PlatformKeyboardEvent::RawKeyDown. Since
        // we don't have access to the text here, we return, and wait for the
        // corresponding event based on PlatformKeyboardEvent::Char.
        return;
    } else if (event->type() == eventNames().keypressEvent) {
        // Which would be this one. This event was disambiguated from the same
        // PlatformKeyboardEvent::KeyDown, but to a PlatformKeyboardEvent::Char,
        // which retains the text from the original event. So, we can safely pass
        // on the event as a key-down event to the plugin.
        record.what = keyDown;
    } else if (event->type() == eventNames().keyupEvent) {
        // PlatformKeyboardEvent::KeyUp events always have the text, so nothing
        // fancy here.
        record.what = keyUp;
    } else {
        return;
    }

    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    int keyCode = platformEvent->nativeVirtualKeyCode();

    const String text = platformEvent->text();
    if (text.length() < 1) {
        event->setDefaultHandled();
        return;
    }

    WTF::RetainPtr<CFStringRef> cfText(WTF::AdoptCF, text.createCFString());

    LOG(Plugin, "PV::hKE(): PKE.text: %s, PKE.unmodifiedText: %s, PKE.keyIdentifier: %s",
            text.ascii().data(), platformEvent->unmodifiedText().ascii().data(),
            platformEvent->keyIdentifier().ascii().data());

    char charCodes[2] = { 0, 0 };
    if (!CFStringGetCString(cfText.get(), charCodes, 2, CFStringGetSystemEncoding())) {
        LOG_ERROR("Could not resolve character code using system encoding.");
        event->setDefaultHandled();
        return;
    }

    record.where = globalMousePosForPlugin();
    record.modifiers = modifiersForEvent(event);
    record.message = ((keyCode & 0xFF) << 8) | (charCodes[0] & 0xFF);
    record.when = TickCount();

    LOG(Plugin, "PV::hKE(): record.modifiers: %d", record.modifiers);

    LOG(Plugin, "PV::hKE(): PKE.qtEvent()->nativeVirtualKey: 0x%02X, charCode: %d",
               keyCode, int(uchar(charCodes[0])));

    if (!dispatchNPEvent(record))
        LOG(Events, "PluginView::handleKeyboardEvent(): Keyboard event type %d not accepted", record.what);
    else
        event->setDefaultHandled();
}

void PluginView::nullEventTimerFired(Timer<PluginView>*)
{
    EventRecord record;

    record.what = nullEvent;
    record.message = 0;
    record.when = TickCount();
    record.where = globalMousePosForPlugin();
    record.modifiers = GetCurrentKeyModifiers();
    if (!Button())
        record.modifiers |= btnState;

    if (!dispatchNPEvent(record))
        LOG(Events, "PluginView::nullEventTimerFired(): Null event not accepted");
}

static int modifiersForEvent(UIEventWithKeyState* event)
{
    int modifiers = 0;

    if (event->ctrlKey())
        modifiers |= controlKey;

    if (event->altKey())
        modifiers |= optionKey;

    if (event->metaKey())
        modifiers |= cmdKey;

    if (event->shiftKey())
        modifiers |= shiftKey;

     return modifiers;
}

static bool tigerOrBetter()
{
    static SInt32 systemVersion = 0;

    if (!systemVersion) {
        if (Gestalt(gestaltSystemVersion, &systemVersion) != noErr)
            return false;
    }

    return systemVersion >= 0x1040;
}

Point PluginView::globalMousePosForPlugin() const
{
    Point pos;
    GetGlobalMouse(&pos);

    IntPoint offset = topLevelOffsetFor(platformPluginWidget());
    pos.h -= offset.x();
    pos.v -= offset.y();

    float scaleFactor = tigerOrBetter() ? HIGetScaleFactor() : 1;

    pos.h = short(pos.h * scaleFactor);
    pos.v = short(pos.v * scaleFactor);

    return pos;
}

bool PluginView::dispatchNPEvent(NPEvent& event)
{
    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(false);
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
    return accepted;
}

// ------------------- Miscellaneous  ------------------

static const char* MozillaUserAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;

    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();

    return m_userAgent.data();
}

const char* PluginView::userAgentStatic()
{
    return MozillaUserAgent;
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    if (!fileExists(filename))
        return NPERR_FILE_NOT_FOUND;

    FILE* fileHandle = fopen((filename.utf8()).data(), "r");

    if (fileHandle == 0)
        return NPERR_FILE_NOT_FOUND;

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

} // namespace WebCore

#endif // !__LP64__
