/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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

#include "config.h"
#include "PluginView.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSDOMWindow.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "FocusController.h"
#include "PlatformMouseEvent.h"
#if PLATFORM(WIN_OS) && !PLATFORM(WX) && ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginMessageThrottlerWin.h"
#endif
#include "PluginPackage.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "PluginMainThreadScheduler.h"
#include "PluginPackage.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "c_instance.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "Settings.h"
#include "runtime.h"
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>
#include <wtf/ASCIICType.h>

using JSC::ExecState;
using JSC::JSLock;
using JSC::JSObject;
using JSC::JSValuePtr;
using JSC::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

static int s_callingPlugin;

static String scriptStringIfJavaScriptURL(const KURL& url)
{
    if (!url.protocolIs("javascript"))
        return String();

    // This returns an unescaped string
    return decodeURLEscapeSequences(url.string().substring(11));
}

PluginView* PluginView::s_currentPluginView = 0;

void PluginView::popPopupsStateTimerFired(Timer<PluginView>*)
{
    popPopupsEnabledState();
}

IntRect PluginView::windowClipRect() const
{
    // Start by clipping to our bounds.
    IntRect clipRect(m_windowRect);
    
    // Take our element and get the clip rect from the enclosing layer and frame view.
    RenderLayer* layer = m_element->renderer()->enclosingLayer();
    FrameView* parentView = m_element->document()->view();
    clipRect.intersect(parentView->windowClipRectForLayer(layer, true));

    return clipRect;
}

void PluginView::setFrameRect(const IntRect& rect)
{
    if (m_element->document()->printing())
        return;

    if (rect != frameRect())
        Widget::setFrameRect(rect);

    updatePluginWidget();

#if PLATFORM(WIN_OS)
    // On Windows, always call plugin to change geometry.
    setNPWindowRect(rect);
#elif XP_UNIX
    // On Unix, only call plugin if it's full-page.
    if (m_mode == NP_FULL)
        setNPWindowRect(rect);
#endif
}

void PluginView::frameRectsChanged()
{
    updatePluginWidget();
}

void PluginView::handleEvent(Event* event)
{
    if (!m_plugin || m_isWindowed)
        return;

    if (event->isMouseEvent())
        handleMouseEvent(static_cast<MouseEvent*>(event));
    else if (event->isKeyboardEvent())
        handleKeyboardEvent(static_cast<KeyboardEvent*>(event));
}

bool PluginView::start()
{
    if (m_isStarted)
        return false;

    PluginMainThreadScheduler::scheduler().registerPlugin(m_instance);

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    NPError npErr;
    {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        npErr = m_plugin->pluginFuncs()->newp((NPMIMEType)m_mimeType.data(), m_instance, m_mode, m_paramCount, m_paramNames, m_paramValues, NULL);
        setCallingPlugin(false);
        LOG_NPERROR(npErr);
        PluginView::setCurrentPluginView(0);
    }

    if (npErr != NPERR_NO_ERROR)
        return false;

    m_isStarted = true;

    if (!m_url.isEmpty() && !m_loadManually) {
        FrameLoadRequest frameLoadRequest;
        frameLoadRequest.resourceRequest().setHTTPMethod("GET");
        frameLoadRequest.resourceRequest().setURL(m_url);
        load(frameLoadRequest, false, 0);
    }

    return true;
}

void PluginView::setCurrentPluginView(PluginView* pluginView)
{
    s_currentPluginView = pluginView;
}

PluginView* PluginView::currentPluginView()
{
    return s_currentPluginView;
}

static char* createUTF8String(const String& str)
{
    CString cstr = str.utf8();
    char* result = reinterpret_cast<char*>(fastMalloc(cstr.length() + 1));

    strncpy(result, cstr.data(), cstr.length() + 1);

    return result;
}

static bool getString(ScriptController* proxy, JSValuePtr result, String& string)
{
    if (!proxy || !result || result.isUndefined())
        return false;
    JSLock lock(false);

    ExecState* exec = proxy->globalObject()->globalExec();
    UString ustring = result.toString(exec);
    exec->clearException();

    string = ustring;
    return true;
}

void PluginView::performRequest(PluginRequest* request)
{
    // don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed unless the loads are in the same frame as the plugin.
    const String& targetFrameName = request->frameLoadRequest().frameName();
    if (m_parentFrame->loader()->documentLoader() != m_parentFrame->loader()->activeDocumentLoader() &&
        (targetFrameName.isNull() || m_parentFrame->tree()->find(targetFrameName) != m_parentFrame))
        return;

    KURL requestURL = request->frameLoadRequest().resourceRequest().url();
    String jsString = scriptStringIfJavaScriptURL(requestURL);

    if (jsString.isNull()) {
        // if this is not a targeted request, create a stream for it. otherwise,
        // just pass it off to the loader
        if (targetFrameName.isEmpty()) {
            RefPtr<PluginStream> stream = PluginStream::create(this, m_parentFrame, request->frameLoadRequest().resourceRequest(), request->sendNotification(), request->notifyData(), plugin()->pluginFuncs(), instance(), m_plugin->quirks());
            m_streams.add(stream);
            stream->start();
        } else {
            m_parentFrame->loader()->load(request->frameLoadRequest().resourceRequest(), targetFrameName, false);
      
            // FIXME: <rdar://problem/4807469> This should be sent when the document has finished loading
            if (request->sendNotification()) {
                PluginView::setCurrentPluginView(this);
                JSC::JSLock::DropAllLocks dropAllLocks(false);
                setCallingPlugin(true);
                m_plugin->pluginFuncs()->urlnotify(m_instance, requestURL.string().utf8().data(), NPRES_DONE, request->notifyData());
                setCallingPlugin(false);
                PluginView::setCurrentPluginView(0);
            }
        }
        return;
    }

    // Targeted JavaScript requests are only allowed on the frame that contains the JavaScript plugin
    // and this has been made sure in ::load.
    ASSERT(targetFrameName.isEmpty() || m_parentFrame->tree()->find(targetFrameName) == m_parentFrame);
    
    // Executing a script can cause the plugin view to be destroyed, so we keep a reference to the parent frame.
    RefPtr<Frame> parentFrame = m_parentFrame;
    JSValuePtr result = m_parentFrame->loader()->executeScript(jsString, request->shouldAllowPopups()).jsValue();

    if (targetFrameName.isNull()) {
        String resultString;

        CString cstr;
        if (getString(parentFrame->script(), result, resultString))
            cstr = resultString.utf8();

        RefPtr<PluginStream> stream = PluginStream::create(this, m_parentFrame, request->frameLoadRequest().resourceRequest(), request->sendNotification(), request->notifyData(), plugin()->pluginFuncs(), instance(), m_plugin->quirks());
        m_streams.add(stream);
        stream->sendJavaScriptStream(requestURL, cstr);
    }
}

void PluginView::requestTimerFired(Timer<PluginView>* timer)
{
    ASSERT(timer == &m_requestTimer);
    ASSERT(m_requests.size() > 0);
    ASSERT(!m_isJavaScriptPaused);

    PluginRequest* request = m_requests[0];
    m_requests.remove(0);
    
    // Schedule a new request before calling performRequest since the call to
    // performRequest can cause the plugin view to be deleted.
    if (m_requests.size() > 0)
        m_requestTimer.startOneShot(0);

    performRequest(request);
    delete request;
}

void PluginView::scheduleRequest(PluginRequest* request)
{
    m_requests.append(request);

    if (!m_isJavaScriptPaused)
        m_requestTimer.startOneShot(0);
}

NPError PluginView::load(const FrameLoadRequest& frameLoadRequest, bool sendNotification, void* notifyData)
{
    ASSERT(frameLoadRequest.resourceRequest().httpMethod() == "GET" || frameLoadRequest.resourceRequest().httpMethod() == "POST");

    KURL url = frameLoadRequest.resourceRequest().url();
    
    if (url.isEmpty())
        return NPERR_INVALID_URL;

    // Don't allow requests to be made when the document loader is stopping all loaders.
    if (m_parentFrame->loader()->documentLoader()->isStopping())
        return NPERR_GENERIC_ERROR;

    const String& targetFrameName = frameLoadRequest.frameName();
    String jsString = scriptStringIfJavaScriptURL(url);

    if (!jsString.isNull()) {
        Settings* settings = m_parentFrame->settings();

        // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
        if (!settings || !settings->isJavaScriptEnabled())
            return NPERR_GENERIC_ERROR;
        
        // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
        if (!targetFrameName.isNull() && m_parentFrame->tree()->find(targetFrameName) != m_parentFrame)
            return NPERR_INVALID_PARAM;
    } else if (!FrameLoader::canLoad(url, String(), m_parentFrame->document())) {
            return NPERR_GENERIC_ERROR;
    }

    PluginRequest* request = new PluginRequest(frameLoadRequest, sendNotification, notifyData, arePopupsAllowed());
    scheduleRequest(request);

    return NPERR_NO_ERROR;
}

static KURL makeURL(const KURL& baseURL, const char* relativeURLString)
{
    String urlString = relativeURLString;

    // Strip return characters.
    urlString.replace('\n', "");
    urlString.replace('\r', "");

    return KURL(baseURL, urlString);
}

NPError PluginView::getURLNotify(const char* url, const char* target, void* notifyData)
{
    FrameLoadRequest frameLoadRequest;

    frameLoadRequest.setFrameName(target);
    frameLoadRequest.resourceRequest().setHTTPMethod("GET");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_baseURL, url));

    return load(frameLoadRequest, true, notifyData);
}

NPError PluginView::getURL(const char* url, const char* target)
{
    FrameLoadRequest frameLoadRequest;

    frameLoadRequest.setFrameName(target);
    frameLoadRequest.resourceRequest().setHTTPMethod("GET");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_baseURL, url));

    return load(frameLoadRequest, false, 0);
}

NPError PluginView::postURLNotify(const char* url, const char* target, uint32 len, const char* buf, NPBool file, void* notifyData)
{
    return handlePost(url, target, len, buf, file, notifyData, true, true);
}

NPError PluginView::postURL(const char* url, const char* target, uint32 len, const char* buf, NPBool file)
{
    // As documented, only allow headers to be specified via NPP_PostURL when using a file.
    return handlePost(url, target, len, buf, file, 0, false, file);
}

NPError PluginView::newStream(NPMIMEType type, const char* target, NPStream** stream)
{
    notImplemented();
    // Unsupported
    return NPERR_GENERIC_ERROR;
}

int32 PluginView::write(NPStream* stream, int32 len, void* buffer)
{
    notImplemented();
    // Unsupported
    return -1;
}

NPError PluginView::destroyStream(NPStream* stream, NPReason reason)
{
    PluginStream* browserStream = static_cast<PluginStream*>(stream->ndata);

    if (!stream || PluginStream::ownerForStream(stream) != m_instance)
        return NPERR_INVALID_INSTANCE_ERROR;

    browserStream->cancelAndDestroyStream(reason);
    return NPERR_NO_ERROR;
}

void PluginView::status(const char* message)
{
    if (Page* page = m_parentFrame->page())
        page->chrome()->setStatusbarText(m_parentFrame, String(message));
}

NPError PluginView::setValue(NPPVariable variable, void* value)
{
    switch (variable) {
    case NPPVpluginWindowBool:
        m_isWindowed = value;
        return NPERR_NO_ERROR;
    case NPPVpluginTransparentBool:
        m_isTransparent = value;
        return NPERR_NO_ERROR;
#if defined(XP_MACOSX)
    case NPPVpluginDrawingModel:
        return NPERR_NO_ERROR;
    case NPPVpluginEventModel:
        return NPERR_NO_ERROR;
#endif
    default:
        notImplemented();
        return NPERR_GENERIC_ERROR;
    }
}

void PluginView::invalidateTimerFired(Timer<PluginView>* timer)
{
    ASSERT(timer == &m_invalidateTimer);

    for (unsigned i = 0; i < m_invalidRects.size(); i++)
        invalidateRect(m_invalidRects[i]);
    m_invalidRects.clear();
}


void PluginView::pushPopupsEnabledState(bool state)
{
    m_popupStateStack.append(state);
}
 
void PluginView::popPopupsEnabledState()
{
    m_popupStateStack.removeLast();
}

bool PluginView::arePopupsAllowed() const
{
    if (!m_popupStateStack.isEmpty())
        return m_popupStateStack.last();

    return false;
}

void PluginView::setJavaScriptPaused(bool paused)
{
    if (m_isJavaScriptPaused == paused)
        return;
    m_isJavaScriptPaused = paused;

    if (m_isJavaScriptPaused)
        m_requestTimer.stop();
    else if (!m_requests.isEmpty())
        m_requestTimer.startOneShot(0);
}

PassRefPtr<JSC::Bindings::Instance> PluginView::bindingInstance()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* object = 0;

    if (!m_plugin || !m_plugin->pluginFuncs()->getvalue)
        return 0;

    NPError npErr;
    {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        npErr = m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginScriptableNPObject, &object);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (npErr != NPERR_NO_ERROR || !object)
        return 0;

    RefPtr<JSC::Bindings::RootObject> root = m_parentFrame->script()->createRootObject(this);
    RefPtr<JSC::Bindings::Instance> instance = JSC::Bindings::CInstance::create(object, root.release());

    _NPN_ReleaseObject(object);

    return instance.release();
#else
    return 0;
#endif
}

void PluginView::disconnectStream(PluginStream* stream)
{
    ASSERT(m_streams.contains(stream));

    m_streams.remove(stream);
}

void PluginView::setParameters(const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    ASSERT(paramNames.size() == paramValues.size());

    unsigned size = paramNames.size();
    unsigned paramCount = 0;

    m_paramNames = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));
    m_paramValues = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));

    for (unsigned i = 0; i < size; i++) {
        if (m_plugin->quirks().contains(PluginQuirkRemoveWindowlessVideoParam) && equalIgnoringCase(paramNames[i], "windowlessvideo"))
            continue;

        m_paramNames[paramCount] = createUTF8String(paramNames[i]);
        m_paramValues[paramCount] = createUTF8String(paramValues[i]);

        paramCount++;
    }

    m_paramCount = paramCount;
}

PluginView::PluginView(Frame* parentFrame, const IntSize& size, PluginPackage* plugin, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
    : m_parentFrame(parentFrame)
    , m_plugin(plugin)
    , m_element(element)
    , m_isStarted(false)
    , m_url(url)
    , m_baseURL(m_parentFrame->loader()->completeURL(m_parentFrame->document()->baseURL().string()))
    , m_status(PluginStatusLoadedSuccessfully)
    , m_requestTimer(this, &PluginView::requestTimerFired)
    , m_invalidateTimer(this, &PluginView::invalidateTimerFired)
    , m_popPopupsStateTimer(this, &PluginView::popPopupsStateTimerFired)
    , m_paramNames(0)
    , m_paramValues(0)
#if defined(XP_MACOSX)
    , m_isWindowed(false)
#else
    , m_isWindowed(true)
#endif
    , m_isTransparent(false)
    , m_haveInitialized(false)
#if PLATFORM(GTK) || defined(Q_WS_X11)
    , m_needsXEmbed(false)
#endif
#if PLATFORM(QT)
    , m_isNPAPIPlugin(false)
#endif
#if PLATFORM(WIN_OS) && !PLATFORM(WX) && ENABLE(NETSCAPE_PLUGIN_API)
    , m_pluginWndProc(0)
    , m_lastMessage(0)
    , m_isCallingPluginWndProc(false)
    , m_wmPrintHDC(0)
#endif
#if (PLATFORM(QT) && PLATFORM(WIN_OS)) || defined(XP_MACOSX)
    , m_window(0)
#endif
    , m_loadManually(loadManually)
    , m_manualStream(0)
    , m_isJavaScriptPaused(false)
{
    if (!m_plugin) {
        m_status = PluginStatusCanNotFindPlugin;
        return;
    }

    m_instance = &m_instanceStruct;
    m_instance->ndata = this;
    m_instance->pdata = 0;

    m_mimeType = mimeType.utf8();

    setParameters(paramNames, paramValues);

#ifdef XP_UNIX
    m_npWindow.ws_info = 0;
#endif

    m_mode = m_loadManually ? NP_FULL : NP_EMBED;

    resize(size);
}

void PluginView::didReceiveResponse(const ResourceResponse& response)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(!m_manualStream);

    m_manualStream = PluginStream::create(this, m_parentFrame, m_parentFrame->loader()->activeDocumentLoader()->request(), false, 0, plugin()->pluginFuncs(), instance(), m_plugin->quirks());
    m_manualStream->setLoadManually(true);

    m_manualStream->didReceiveResponse(0, response);
}

void PluginView::didReceiveData(const char* data, int length)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(m_manualStream);
    
    m_manualStream->didReceiveData(0, data, length);
}

void PluginView::didFinishLoading()
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didFinishLoading(0);
}

void PluginView::didFail(const ResourceError& error)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didFail(0, error);
}

void PluginView::setCallingPlugin(bool b) const
{
    if (!m_plugin->quirks().contains(PluginQuirkHasModalMessageLoop))
        return;

    if (b)
        ++s_callingPlugin;
    else
        --s_callingPlugin;

    ASSERT(s_callingPlugin >= 0);
}

bool PluginView::isCallingPlugin()
{
    return s_callingPlugin > 0;
}

PluginView* PluginView::create(Frame* parentFrame, const IntSize& size, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    // if we fail to find a plugin for this MIME type, findPlugin will search for
    // a plugin by the file extension and update the MIME type, so pass a mutable String
    String mimeTypeCopy = mimeType;
    PluginPackage* plugin = PluginDatabase::installedPlugins()->findPlugin(url, mimeTypeCopy);

    // No plugin was found, try refreshing the database and searching again
    if (!plugin && PluginDatabase::installedPlugins()->refresh()) {
        mimeTypeCopy = mimeType;
        plugin = PluginDatabase::installedPlugins()->findPlugin(url, mimeTypeCopy);
    }

    return new PluginView(parentFrame, size, plugin, element, url, paramNames, paramValues, mimeTypeCopy, loadManually);
}

void PluginView::freeStringArray(char** stringArray, int length)
{
    if (!stringArray)
        return;

    for (int i = 0; i < length; i++)
        fastFree(stringArray[i]);

    fastFree(stringArray);
}

static inline bool startsWithBlankLine(const Vector<char>& buffer)
{
    return buffer.size() > 0 && buffer[0] == '\n';
}

static inline int locationAfterFirstBlankLine(const Vector<char>& buffer)
{
    const char* bytes = buffer.data();
    unsigned length = buffer.size();

    for (unsigned i = 0; i < length - 4; i++) {
        // Support for Acrobat. It sends "\n\n".
        if (bytes[i] == '\n' && bytes[i + 1] == '\n')
            return i + 2;
        
        // Returns the position after 2 CRLF's or 1 CRLF if it is the first line.
        if (bytes[i] == '\r' && bytes[i + 1] == '\n') {
            i += 2;
            if (i == 2)
                return i;
            else if (bytes[i] == '\n')
                // Support for Director. It sends "\r\n\n" (3880387).
                return i + 1;
            else if (bytes[i] == '\r' && bytes[i + 1] == '\n')
                // Support for Flash. It sends "\r\n\r\n" (3758113).
                return i + 2;
        }
    }

    return -1;
}

static inline const char* findEOL(const char* bytes, unsigned length)
{
    // According to the HTTP specification EOL is defined as
    // a CRLF pair. Unfortunately, some servers will use LF
    // instead. Worse yet, some servers will use a combination
    // of both (e.g. <header>CRLFLF<body>), so findEOL needs
    // to be more forgiving. It will now accept CRLF, LF or
    // CR.
    //
    // It returns NULL if EOLF is not found or it will return
    // a pointer to the first terminating character.
    for (unsigned i = 0; i < length; i++) {
        if (bytes[i] == '\n')
            return bytes + i;
        if (bytes[i] == '\r') {
            // Check to see if spanning buffer bounds
            // (CRLF is across reads). If so, wait for
            // next read.
            if (i + 1 == length)
                break;

            return bytes + i;
        }
    }

    return 0;
}

static inline String capitalizeRFC822HeaderFieldName(const String& name)
{
    bool capitalizeCharacter = true;
    String result;

    for (unsigned i = 0; i < name.length(); i++) {
        UChar c;

        if (capitalizeCharacter && name[i] >= 'a' && name[i] <= 'z')
            c = toASCIIUpper(name[i]);
        else if (!capitalizeCharacter && name[i] >= 'A' && name[i] <= 'Z')
            c = toASCIILower(name[i]);
        else
            c = name[i];

        if (name[i] == '-')
            capitalizeCharacter = true;
        else
            capitalizeCharacter = false;

        result.append(c);
    }

    return result;
}

static inline HTTPHeaderMap parseRFC822HeaderFields(const Vector<char>& buffer, unsigned length)
{
    const char* bytes = buffer.data();
    const char* eol;
    String lastKey;
    HTTPHeaderMap headerFields;

    // Loop ove rlines until we're past the header, or we can't find any more end-of-lines
    while ((eol = findEOL(bytes, length))) {
        const char* line = bytes;
        int lineLength = eol - bytes;
        
        // Move bytes to the character after the terminator as returned by findEOL.
        bytes = eol + 1;
        if ((*eol == '\r') && (*bytes == '\n'))
            bytes++; // Safe since findEOL won't return a spanning CRLF.

        length -= (bytes - line);
        if (lineLength == 0)
            // Blank line; we're at the end of the header
            break;
        else if (*line == ' ' || *line == '\t') {
            // Continuation of the previous header
            if (lastKey.isNull()) {
                // malformed header; ignore it and continue
                continue;
            } else {
                // Merge the continuation of the previous header
                String currentValue = headerFields.get(lastKey);
                String newValue(line, lineLength);

                headerFields.set(lastKey, currentValue + newValue);
            }
        } else {
            // Brand new header
            const char* colon;
            for (colon = line; *colon != ':' && colon != eol; colon++) {
                // empty loop
            }
            if (colon == eol) 
                // malformed header; ignore it and continue
                continue;
            else {
                lastKey = capitalizeRFC822HeaderFieldName(String(line, colon - line));
                String value;

                for (colon++; colon != eol; colon++) {
                    if (*colon != ' ' && *colon != '\t')
                        break;
                }
                if (colon == eol)
                    value = "";
                else
                    value = String(colon, eol - colon);

                String oldValue = headerFields.get(lastKey);
                if (!oldValue.isNull()) {
                    String tmp = oldValue;
                    tmp += ", ";
                    tmp += value;
                    value = tmp;
                }

                headerFields.set(lastKey, value);
            }
        }
    }

    return headerFields;
}

NPError PluginView::handlePost(const char* url, const char* target, uint32 len, const char* buf, bool file, void* notifyData, bool sendNotification, bool allowHeaders)
{
    if (!url || !len || !buf)
        return NPERR_INVALID_PARAM;

    FrameLoadRequest frameLoadRequest;

    HTTPHeaderMap headerFields;
    Vector<char> buffer;
    
    if (file) {
        NPError readResult = handlePostReadFile(buffer, len, buf);
        if(readResult != NPERR_NO_ERROR)
            return readResult;
    } else {
        buffer.resize(len);
        memcpy(buffer.data(), buf, len);
    }

    const char* postData = buffer.data();
    int postDataLength = buffer.size();

    if (allowHeaders) {
        if (startsWithBlankLine(buffer)) {
            postData++;
            postDataLength--;
        } else {
            int location = locationAfterFirstBlankLine(buffer);
            if (location != -1) {
                // If the blank line is somewhere in the middle of the buffer, everything before is the header
                headerFields = parseRFC822HeaderFields(buffer, location);
                unsigned dataLength = buffer.size() - location;

                // Sometimes plugins like to set Content-Length themselves when they post,
                // but WebFoundation does not like that. So we will remove the header
                // and instead truncate the data to the requested length.
                String contentLength = headerFields.get("Content-Length");

                if (!contentLength.isNull())
                    dataLength = min(contentLength.toInt(), (int)dataLength);
                headerFields.remove("Content-Length");

                postData += location;
                postDataLength = dataLength;
            }
        }
    }

    frameLoadRequest.resourceRequest().setHTTPMethod("POST");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_baseURL, url));
    frameLoadRequest.resourceRequest().addHTTPHeaderFields(headerFields);
    frameLoadRequest.resourceRequest().setHTTPBody(FormData::create(postData, postDataLength));
    frameLoadRequest.setFrameName(target);

    return load(frameLoadRequest, sendNotification, notifyData);
}

void PluginView::invalidateWindowlessPluginRect(const IntRect& rect)
{
    if (!isVisible())
        return;
    
    if (!m_element->renderer())
        return;
    RenderBox* renderer = toRenderBox(m_element->renderer());
    
    IntRect dirtyRect = rect;
    dirtyRect.move(renderer->borderLeft() + renderer->paddingLeft(), renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(dirtyRect);
}

void PluginView::paintMissingPluginIcon(GraphicsContext* context, const IntRect& rect)
{
    static RefPtr<Image> nullPluginImage;
    if (!nullPluginImage)
        nullPluginImage = Image::loadPlatformResource("nullPlugin");

    IntRect imageRect(frameRect().x(), frameRect().y(), nullPluginImage->width(), nullPluginImage->height());

    int xOffset = (frameRect().width() - imageRect.width()) / 2;
    int yOffset = (frameRect().height() - imageRect.height()) / 2;

    imageRect.move(xOffset, yOffset);

    if (!rect.intersects(imageRect))
        return;

    context->save();
    context->clip(windowClipRect());
    context->drawImage(nullPluginImage.get(), imageRect.location());
    context->restore();
}

} // namespace WebCore
