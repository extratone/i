/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "WebEditorClient.h"

#include "PlatformKeyboardEvent.h"
#include <WebCore/DataObjectGtk.h>
#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserver.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Pasteboard.h>
#include <WebCore/markup.h>
#include <wtf/glib/GRefPtr.h>

using namespace WebCore;

namespace WebKit {

bool WebEditorClient::executePendingEditorCommands(Frame* frame, const Vector<WTF::String>& pendingEditorCommands, bool allowTextInsertion)
{
    Vector<Editor::Command> commands;
    for (auto& commandString : pendingEditorCommands) {
        Editor::Command command = frame->editor().command(commandString.utf8().data());
        if (command.isTextInsertion() && !allowTextInsertion)
            return false;

        commands.append(WTF::move(command));
    }

    for (auto& command : commands) {
        if (!command.execute())
            return false;
    }

    return true;
}

void WebEditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (!platformEvent)
        return;

    // If this was an IME event don't do anything.
    if (platformEvent->handledByInputMethod())
        return;

    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document().frame();
    ASSERT(frame);

    const Vector<String> pendingEditorCommands = platformEvent->commands();
    if (!pendingEditorCommands.isEmpty()) {

        // During RawKeyDown events if an editor command will insert text, defer
        // the insertion until the keypress event. We want keydown to bubble up
        // through the DOM first.
        if (platformEvent->type() == PlatformEvent::RawKeyDown) {
            if (executePendingEditorCommands(frame, pendingEditorCommands, false))
                event->setDefaultHandled();

            return;
        }

        // Only allow text insertion commands if the current node is editable.
        if (executePendingEditorCommands(frame, pendingEditorCommands, frame->editor().canEdit())) {
            event->setDefaultHandled();
            return;
        }
    }

    // Don't allow text insertion for nodes that cannot edit.
    if (!frame->editor().canEdit())
        return;

    // This is just a normal text insertion, so wait to execute the insertion
    // until a keypress event happens. This will ensure that the insertion will not
    // be reflected in the contents of the field until the keyup DOM event.
    if (event->type() != eventNames().keypressEvent)
        return;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (event->charCode() < ' ')
        return;

    // Don't insert anything if a modifier is pressed
    if (platformEvent->ctrlKey() || platformEvent->altKey())
        return;

    if (frame->editor().insertText(platformEvent->text(), event))
        event->setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent* event)
{
    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (platformEvent && platformEvent->handledByInputMethod())
        event->setDefaultHandled();
}

#if PLATFORM(X11)
class EditorClientFrameDestructionObserver : FrameDestructionObserver {
public:
    EditorClientFrameDestructionObserver(Frame* frame, GClosure* closure)
        : FrameDestructionObserver(frame)
        , m_closure(closure)
    {
        g_closure_add_finalize_notifier(m_closure, this, destroyOnClosureFinalization);
    }

    void frameDestroyed()
    {
        g_closure_invalidate(m_closure);
        FrameDestructionObserver::frameDestroyed();
    }
private:
    GClosure* m_closure;

    static void destroyOnClosureFinalization(gpointer data, GClosure*)
    {
        // Calling delete void* will free the memory but won't invoke
        // the destructor, something that is a must for us.
        EditorClientFrameDestructionObserver* observer = static_cast<EditorClientFrameDestructionObserver*>(data);
        delete observer;
    }
};

static Frame* frameSettingClipboard;

static void collapseSelection(GtkClipboard*, Frame* frame)
{
    if (frameSettingClipboard && frameSettingClipboard == frame)
        return;

    // Collapse the selection without clearing it.
    ASSERT(frame);
    const VisibleSelection& selection = frame->selection().selection();
    frame->selection().setBase(selection.extent(), selection.affinity());
}
#endif

void WebEditorClient::updateGlobalSelection(Frame* frame)
{
#if PLATFORM(X11)
    if (!frame->selection().isRange())
        return;

    frameSettingClipboard = frame;
    GRefPtr<GClosure> callback = adoptGRef(g_cclosure_new(G_CALLBACK(collapseSelection), frame, nullptr));
    // This observer will be self-destroyed on closure finalization,
    // that will happen either after closure execution or after
    // closure invalidation.
    new EditorClientFrameDestructionObserver(frame, callback.get());
    g_closure_set_marshal(callback.get(), g_cclosure_marshal_VOID__VOID);

    RefPtr<Range> range = frame->selection().toNormalizedRange();
    PasteboardWebContent pasteboardContent;
    pasteboardContent.canSmartCopyOrDelete = false;
    pasteboardContent.text = range->text();
    pasteboardContent.markup = createMarkup(*range, nullptr, AnnotateForInterchange, false, ResolveNonLocalURLs);
    pasteboardContent.callback = callback;
    Pasteboard::createForGlobalSelection()->write(pasteboardContent);
    frameSettingClipboard = nullptr;
#endif
}

bool WebEditorClient::shouldShowUnicodeMenu()
{
    return true;
}

}
