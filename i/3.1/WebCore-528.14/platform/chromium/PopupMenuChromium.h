/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PopupMenuChromium_h
#define PopupMenuChromium_h

#include "config.h"
#include "PopupMenuClient.h"

#include "FramelessScrollView.h"
#include "IntRect.h"

namespace WebCore {

    class FrameView;
    class PopupListBox;

    // FIXME: Our FramelessScrollView classes should probably implement HostWindow!

    // This class holds a PopupListBox (see cpp file).  Its sole purpose is to be
    // able to draw a border around its child.  All its paint/event handling is
    // just forwarded to the child listBox (with the appropriate transforms).
    // NOTE: this class is exposed so it can be instantiated direcly for the
    // autofill popup.  We cannot use the Popup class directly in that case as the
    // autofill popup should not be focused when shown and we want to forward the
    // key events to it (through handleKeyEvent).

    class PopupContainer : public FramelessScrollView, public RefCounted<PopupContainer>  {
    public:
        static PassRefPtr<PopupContainer> create(PopupMenuClient*, bool focusOnShow);

        // Whether a key event should be sent to this popup.
        virtual bool isInterestedInEventForKey(int keyCode);

        // FramelessScrollView
        virtual void paint(GraphicsContext*, const IntRect&);
        virtual void hide();
        virtual bool handleMouseDownEvent(const PlatformMouseEvent&);
        virtual bool handleMouseMoveEvent(const PlatformMouseEvent&);
        virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&);
        virtual bool handleWheelEvent(const PlatformWheelEvent&);
        virtual bool handleKeyEvent(const PlatformKeyboardEvent&);

        // PopupContainer methods

        // Show the popup
        void showPopup(FrameView*);

        // Show the popup in the specified rect for the specified frame.
        // Note: this code was somehow arbitrarily factored-out of the Popup class
        // so WebViewImpl can create a PopupContainer.
        void show(const IntRect&, FrameView*, int index);

        // Hide the popup.  Do not call this directly: use client->hidePopup().
        void hidePopup();

        // Compute size of widget and children.
        void layout();

        // Sets whether the PopupMenuClient should be told to change its text when a
        // new item is selected (by using the arrow keys).  Default is true.
        void setTextOnIndexChange(bool);

        // Sets whether the selection should be accepted when the popup menu is
        // closed (through ESC being pressed or the focus going away).  Default
        // is true.  Note that when TAB is pressed, the selection is always
        // accepted regardless of this setting.
        void setAcceptOnAbandon(bool);

        // Sets whether we should move the selection to the first/last item
        // when the user presses down/up arrow keys and the last/first item is
        // selected.  Default is false, causing the first/last item to stay
        // selected.
        void setLoopSelectionNavigation(bool);

        PopupListBox* listBox() const { return m_listBox.get(); }

        // Refresh the popup values from the PopupMenuClient.
        void refresh();

    private:
        friend class WTF::RefCounted<PopupContainer>;

        PopupContainer(PopupMenuClient*, bool focusOnShow);
        ~PopupContainer();

        // Paint the border.
        void paintBorder(GraphicsContext*, const IntRect&);

        RefPtr<PopupListBox> m_listBox;

        // Whether the window showing this popup should be focused when shown.
        bool m_focusOnShow;
    };

} // namespace WebCore

#endif
