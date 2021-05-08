/*
 * Copyright (C) 2006, 2007, 2008 Apple, Inc. All rights reserved.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include "FocusDirection.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "ScrollTypes.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#include "Console.h"
#include <wtf/HashMap.h>

#if PLATFORM(MAC)
#include "WebCoreKeyboardUIMode.h"
#endif

#define NSResponder WAKResponder
#ifndef __OBJC__
class WAKResponder;
#else
@class WAKResponder;
#endif

#ifndef __OBJC__
class NSMenu;
class NSResponder;
#endif

namespace WebCore {

    class AtomicString;
    class FileChooser;
    class FloatRect;
    class Frame;
    class Geolocation;
    class HTMLParserQuirks;
    class HitTestResult;
    class IntRect;
    class Node;
    class Page;
    class String;
    class Widget;
    
    struct FrameLoadRequest;
    struct ViewportArguments;
    struct WindowFeatures;

#if USE(ACCELERATED_COMPOSITING)
    class GraphicsLayer;
#endif

    class ChromeClient {
    public:
        virtual void chromeDestroyed() = 0;
        
        virtual void setWindowRect(const FloatRect&) = 0;
        virtual FloatRect windowRect() = 0;
        
        virtual FloatRect pageRect() = 0;
        
        virtual float scaleFactor() = 0;
    
        virtual void focus(bool userGesture) = 0;
        virtual void unfocus() = 0;

        virtual bool canTakeFocus(FocusDirection) = 0;
        virtual void takeFocus(FocusDirection) = 0;

        // The Frame pointer provides the ChromeClient with context about which
        // Frame wants to create the new Page.  Also, the newly created window
        // should not be shown to the user until the ChromeClient of the newly
        // created Page has its show method called.
        virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const bool userGesture) = 0;
        virtual void show() = 0;

        virtual bool canRunModal() = 0;
        virtual void runModal() = 0;

        virtual void setToolbarsVisible(bool) = 0;
        virtual bool toolbarsVisible() = 0;
        
        virtual void setStatusbarVisible(bool) = 0;
        virtual bool statusbarVisible() = 0;
        
        virtual void setScrollbarsVisible(bool) = 0;
        virtual bool scrollbarsVisible() = 0;
        
        virtual void setMenubarVisible(bool) = 0;
        virtual bool menubarVisible() = 0;

        virtual void setResizable(bool) = 0;
        
        virtual void addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID) = 0;

        virtual bool canRunBeforeUnloadConfirmPanel() = 0;
        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame) = 0;

        virtual void closeWindowSoon() = 0;
        
        virtual void runJavaScriptAlert(Frame*, const String&) = 0;
        virtual bool runJavaScriptConfirm(Frame*, const String&) = 0;
        virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) = 0;
        virtual void setStatusbarText(const String&) = 0;
        virtual bool shouldInterruptJavaScript() = 0;
        virtual bool tabsToLinks() const = 0;

        virtual IntRect windowResizerRect() const = 0;

        // Methods used by HostWindow.
        virtual void repaint(const IntRect&, bool contentChanged, bool immediate = false, bool repaintContentOnly = false) = 0;
        virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) = 0;
        virtual IntPoint screenToWindow(const IntPoint&) const = 0;
        virtual IntRect windowToScreen(const IntRect&) const = 0;
        virtual PlatformWidget platformWindow() const = 0;
        virtual void contentsSizeChanged(Frame*, const IntSize&) const = 0;
        virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const {} // Platforms other than Mac can implement this if it ever becomes necessary for them to do so.
        // End methods used by HostWindow.

        virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) = 0;

        virtual void setToolTip(const String&) = 0;

        virtual void print(Frame*) = 0;

        virtual void exceededDatabaseQuota(Frame*, const String& databaseName) = 0;

#if ENABLE(DASHBOARD_SUPPORT)
        virtual void dashboardRegionsChanged();
#endif

        virtual void populateVisitedLinks();

        virtual FloatRect customHighlightRect(Node*, const AtomicString& type, const FloatRect& lineRect);
        virtual void paintCustomHighlight(Node*, const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect,
            bool behindText, bool entireLine);
            
        virtual bool shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename);
        virtual String generateReplacementFile(const String& path);
        
        virtual void enableSuddenTermination();
        virtual void disableSuddenTermination();

        virtual bool paintCustomScrollbar(GraphicsContext*, const FloatRect&, ScrollbarControlSize, 
                                          ScrollbarControlState, ScrollbarPart pressedPart, bool vertical,
                                          float value, float proportion, ScrollbarControlPartMask);
        virtual bool paintCustomScrollCorner(GraphicsContext*, const FloatRect&);

#if ENABLE(TOUCH_EVENTS)
        virtual void eventRegionsChanged(const HashMap< RefPtr<Node>, unsigned>&) const = 0;
        virtual void didPreventDefaultForEvent() const = 0;
#endif
        virtual void didReceiveDocType(Frame*) const = 0;
        virtual void setNeedsScrollNotifications(Frame*, bool) const = 0;
        virtual void observedContentChange(Frame*) const = 0;
        virtual void clearContentChangeObservers(Frame*) const = 0;
        virtual void didReceiveViewportArguments(Frame*, const ViewportArguments&) const = 0;
        virtual void notifyRevealedSelectionByScrollingFrame(Frame*) const = 0;
        virtual bool isStopping() const = 0;
        virtual void didLayout() const = 0;

        // This is an synchronous call. The ChromeClient can display UI asking the user for permission
        // to use Geolococation. The ChromeClient must call Geolocation::setShouldClearCache() appropriately.
        virtual bool requestGeolocationPermissionForFrame(Frame*, Geolocation*) { return false; }
        
        virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) = 0;

        // Notification that the given form element has changed. This function
        // will be called frequently, so handling should be very fast.
        virtual void formStateDidChange(const Node*) = 0;

        virtual HTMLParserQuirks* createHTMLParserQuirks() = 0;

#if USE(ACCELERATED_COMPOSITING)
        // Pass 0 as the GraphicsLayer to detatch the root layer.
        virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) { }
        // Sets a flag to specify that the next time content is drawn to the window,
        // the changes appear on the screen in synchrony with updates to GraphicsLayers.
        virtual void setNeedsOneShotDrawingSynchronization() { }
        // Sets a flag to specify that the view needs to be updated, so we need
        // to do an eager layout before the drawing.
        virtual void scheduleViewUpdate() { }
#endif

#if PLATFORM(MAC)
        virtual KeyboardUIMode keyboardUIMode() { return KeyboardAccessDefault; }

        virtual NSResponder *firstResponder() { return 0; }
        virtual void makeFirstResponder(NSResponder *) { }

#endif

    protected:
        virtual ~ChromeClient() { }
    };

}

#endif // ChromeClient_h
