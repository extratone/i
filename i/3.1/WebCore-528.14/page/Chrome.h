/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Chrome_h
#define Chrome_h

#include "FileChooser.h"
#include "FocusDirection.h"
#include "HostWindow.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

#ifndef __OBJC__
class WAKView;
#else
@class WAKView;
#endif

#if PLATFORM(MAC)
#ifndef __OBJC__
class NSView;
#endif
#endif

namespace WebCore {

    class ChromeClient;
    class ContextMenu;
    class FloatRect;
    class Frame;
    class Geolocation;
    class HitTestResult;
    class IntRect;
    class Page;
    class String;

    struct FrameLoadRequest;
    struct WindowFeatures;
    
    class Chrome : public HostWindow {
    public:
        Chrome(Page*, ChromeClient*);
        ~Chrome();

        ChromeClient* client() { return m_client; }

        // HostWindow methods.
        virtual void repaint(const IntRect&, bool contentChanged, bool immediate = false, bool repaintContentOnly = false);
        virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect);
        virtual IntPoint screenToWindow(const IntPoint&) const;
        virtual IntRect windowToScreen(const IntRect&) const;
        virtual PlatformWidget platformWindow() const;
        virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const;

        void contentsSizeChanged(Frame*, const IntSize&) const;

        void setWindowRect(const FloatRect&) const;
        FloatRect windowRect() const;

        FloatRect pageRect() const;
        
        float scaleFactor();

        void focus(bool userGesture) const;
        void unfocus() const;

        bool canTakeFocus(FocusDirection) const;
        void takeFocus(FocusDirection) const;

        Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const bool) const;
        void show() const;

        bool canRunModal() const;
        bool canRunModalNow() const;
        void runModal() const;

        void setToolbarsVisible(bool) const;
        bool toolbarsVisible() const;
        
        void setStatusbarVisible(bool) const;
        bool statusbarVisible() const;
        
        void setScrollbarsVisible(bool) const;
        bool scrollbarsVisible() const;
        
        void setMenubarVisible(bool) const;
        bool menubarVisible() const;
        
        void setResizable(bool) const;

        bool canRunBeforeUnloadConfirmPanel();
        bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);

        void closeWindowSoon();

        void runJavaScriptAlert(Frame*, const String&);
        bool runJavaScriptConfirm(Frame*, const String&);
        bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result);
        void setStatusbarText(Frame*, const String&);
        bool shouldInterruptJavaScript();

        IntRect windowResizerRect() const;

        void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

        void setToolTip(const HitTestResult&);

        void print(Frame*);

        void enableSuddenTermination();
        void disableSuddenTermination();

        bool requestGeolocationPermissionForFrame(Frame*, Geolocation*);
            
        void runOpenPanel(Frame*, PassRefPtr<FileChooser>);

#if PLATFORM(MAC)
        void focusNSView(NSView*);
#endif

    private:
        Page* m_page;
        ChromeClient* m_client;
    };
}

#endif // Chrome_h
