/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "TypingCommand.h"

#include "BeforeTextInsertedEvent.h"
#include "BreakBlockquoteCommand.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "InsertLineBreakCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "SelectionController.h"
#include "VisiblePosition.h"
#include "htmlediting.h"
#include "visible_units.h"

namespace WebCore {

TypingCommand::TypingCommand(Document *document, ETypingCommand commandType, const String &textToInsert, bool selectInsertedText, TextGranularity granularity)
    : CompositeEditCommand(document), 
      m_commandType(commandType), 
      m_textToInsert(textToInsert), 
      m_openForMoreTyping(true), 
      m_applyEditing(false), 
      m_selectInsertedText(selectInsertedText),
      m_smartDelete(false),
      m_granularity(granularity)
{
}

void TypingCommand::deleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->deleteKeyPressed(granularity);
        return;
    }
    
    TypingCommand *typingCommand = new TypingCommand(document, DeleteKey, "", false, granularity);
    typingCommand->setSmartDelete(smartDelete);
    EditCommandPtr cmd(typingCommand);
    cmd.apply();
}

void TypingCommand::forwardDeleteKeyPressed(Document *document, bool smartDelete, TextGranularity granularity)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->forwardDeleteKeyPressed(granularity);
        return;
    }

    TypingCommand *typingCommand = new TypingCommand(document, ForwardDeleteKey, "", false, granularity);
    typingCommand->setSmartDelete(smartDelete);
    EditCommandPtr cmd(typingCommand);
    cmd.apply();
}

void TypingCommand::insertText(Document *document, const String &text, bool selectInsertedText)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    String newText = text;
    Node* startNode = frame->selection().start().node();
    
    if (startNode && startNode->rootEditableElement()) {        
        // Send khtmlBeforeTextInsertedEvent.  The event handler will update text if necessary.
        ExceptionCode ec = 0;
        RefPtr<BeforeTextInsertedEvent> evt = new BeforeTextInsertedEvent(text);
        startNode->rootEditableElement()->dispatchEvent(evt, ec, true);
        newText = evt->text();
    }
    
    if (newText.isEmpty())
        return;
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertText(newText, selectInsertedText);
        return;
    }

    EditCommandPtr cmd(new TypingCommand(document, InsertText, newText, selectInsertedText));
    cmd.apply();
}

void TypingCommand::insertLineBreak(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertLineBreak();
        return;
    }

    EditCommandPtr cmd(new TypingCommand(document, InsertLineBreak));
    cmd.apply();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparatorInQuotedContent();
        return;
    }

    EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparatorInQuotedContent));
    cmd.apply();
}

void TypingCommand::insertParagraphSeparator(Document *document)
{
    ASSERT(document);
    
    Frame *frame = document->frame();
    ASSERT(frame);
    
    EditCommandPtr lastEditCommand = frame->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand *>(lastEditCommand.get())->insertParagraphSeparator();
        return;
    }

    EditCommandPtr cmd(new TypingCommand(document, InsertParagraphSeparator));
    cmd.apply();
}

bool TypingCommand::isOpenForMoreTypingCommand(const EditCommandPtr &cmd)
{
    return cmd.isTypingCommand() &&
        static_cast<const TypingCommand *>(cmd.get())->openForMoreTyping();
}

void TypingCommand::closeTyping(const EditCommandPtr &cmd)
{
    if (isOpenForMoreTypingCommand(cmd))
        static_cast<TypingCommand *>(cmd.get())->closeTyping();
}

void TypingCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    switch (m_commandType) {
        case DeleteKey:
            deleteKeyPressed(m_granularity);
            return;
        case ForwardDeleteKey:
            forwardDeleteKeyPressed(m_granularity);
            return;
        case InsertLineBreak:
            insertLineBreak();
            return;
        case InsertParagraphSeparator:
            insertParagraphSeparator();
            return;
        case InsertParagraphSeparatorInQuotedContent:
            insertParagraphSeparatorInQuotedContent();
            return;
        case InsertText:
            insertText(m_textToInsert, m_selectInsertedText);
            return;
    }

    ASSERT_NOT_REACHED();
}

EditAction TypingCommand::editingAction() const
{
    return EditActionTyping;
}

void TypingCommand::markMisspellingsAfterTyping()
{
}

void TypingCommand::typingAddedToOpenCommand()
{
    markMisspellingsAfterTyping();
    // Do not apply editing to the frame on the first time through.
    // The frame will get told in the same way as all other commands.
    // But since this command stays open and is used for additional typing, 
    // we need to tell the frame here as other commands are added.
    if (m_applyEditing) {
        EditCommandPtr cmd(this);
        document()->frame()->appliedEditing(cmd);
    }
    m_applyEditing = true;
}

void TypingCommand::insertText(const String &text, bool selectInsertedText)
{
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    int offset = 0;
    int newline;
    while ((newline = text.find('\n', offset)) != -1) {
        if (newline != offset)
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), false);
        insertParagraphSeparator();
        offset = newline + 1;
    }
    if (offset == 0)
        insertTextRunWithoutNewlines(text, selectInsertedText);
    else {
        int length = text.length();
        if (length != offset) {
            insertTextRunWithoutNewlines(text.substring(offset, length - offset), selectInsertedText);
        }
    }
}

void TypingCommand::insertTextRunWithoutNewlines(const String &text, bool selectInsertedText)
{
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (document()->frame()->typingStyle() || m_cmds.count() == 0) {
        InsertTextCommand *impl = new InsertTextCommand(document());
        EditCommandPtr cmd(impl);
        applyCommandToComposite(cmd);
        impl->input(text, selectInsertedText);
    } else {
        EditCommandPtr lastCommand = m_cmds.last();
        if (lastCommand.isInsertTextCommand()) {
            InsertTextCommand *impl = static_cast<InsertTextCommand *>(lastCommand.get());
            impl->input(text, selectInsertedText);
        } else {
            InsertTextCommand *impl = new InsertTextCommand(document());
            EditCommandPtr cmd(impl);
            applyCommandToComposite(cmd);
            impl->input(text, selectInsertedText);
        }
    }
    typingAddedToOpenCommand();
}

void TypingCommand::insertLineBreak()
{
    EditCommandPtr cmd(new InsertLineBreakCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::insertParagraphSeparatorInQuotedContent()
{
    EditCommandPtr cmd(new BreakBlockquoteCommand(document()));
    applyCommandToComposite(cmd);
    typingAddedToOpenCommand();
}

void TypingCommand::deleteKeyPressed(TextGranularity granularity)
{
    Selection selectionToDelete;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            break;
        case Selection::CARET: {
            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            SelectionController sc = SelectionController(endingSelection().start(), endingSelection().end(), SEL_DEFAULT_AFFINITY);
            sc.modify(SelectionController::EXTEND, SelectionController::BACKWARD, granularity);
            
            // When the caret is at the start of the editable area in an empty list item, break out of the list item.
            if (endingSelection().visibleStart().previous(true).isNull()) {
                if (breakOutOfEmptyListItem()) {
                    typingAddedToOpenCommand();
                    return;
                }
            }
            
            // When the caret is a) just after a table, or b) at the beginning of the paragraph after a table, select the table.
            Position upstreamStart = endingSelection().start().upstream();
            VisiblePosition visibleStart = endingSelection().visibleStart();
            if (isStartOfParagraph(visibleStart))
                upstreamStart = visibleStart.previous(true).deepEquivalent().upstream();
            if (upstreamStart.node() && upstreamStart.node()->renderer() && upstreamStart.node()->renderer()->isTable() && upstreamStart.offset() == maxDeepOffset(upstreamStart.node())) {
                setEndingSelection(Selection(Position(upstreamStart.node(), 0), endingSelection().start(), DOWNSTREAM));
                typingAddedToOpenCommand();
                return;
            }

            selectionToDelete = sc.selection();
            
            // setStartingSelection so that undo selects what was deleted
            if (selectionToDelete.isCaretOrRange() && granularity != CharacterGranularity)
                setStartingSelection(selectionToDelete);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange() && document()->frame()->shouldDeleteSelection(SelectionController(selectionToDelete))) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
    else {
        setEndingSelection(document()->frame()->selection().selection());
        closeTyping(this);
    }
}

void TypingCommand::forwardDeleteKeyPressed(TextGranularity granularity)
{
    Selection selectionToDelete;
    
    switch (endingSelection().state()) {
        case Selection::RANGE:
            selectionToDelete = endingSelection();
            break;
        case Selection::CARET: {
            // Handle delete at beginning-of-block case.
            // Do nothing in the case that the caret is at the start of a
            // root editable element or at the start of a document.
            SelectionController sc = SelectionController(endingSelection().start(), endingSelection().end(), SEL_DEFAULT_AFFINITY);
            sc.modify(SelectionController::EXTEND, SelectionController::FORWARD, granularity);
            Position downstreamEnd = endingSelection().end().downstream();
            VisiblePosition visibleEnd = endingSelection().visibleEnd();
            if (visibleEnd == endOfParagraph(visibleEnd))
                downstreamEnd = visibleEnd.next(true).deepEquivalent().downstream();
            // When deleting tables: Select the table first, then perform the deletion
            if (downstreamEnd.node() && downstreamEnd.node()->renderer() && downstreamEnd.node()->renderer()->isTable() && downstreamEnd.offset() == 0) {
                setEndingSelection(Selection(endingSelection().end(), Position(downstreamEnd.node(), maxDeepOffset(downstreamEnd.node())), DOWNSTREAM));
                typingAddedToOpenCommand();
                return;
            }
            selectionToDelete = sc.selection();

            // setStartingSelection so that undo selects what was deleted
            if (selectionToDelete.isCaretOrRange() && granularity != CharacterGranularity)
                setStartingSelection(selectionToDelete);
            break;
        }
        case Selection::NONE:
            ASSERT_NOT_REACHED();
            break;
    }
    
    if (selectionToDelete.isCaretOrRange() && document()->frame()->shouldDeleteSelection(SelectionController(selectionToDelete))) {
        deleteSelection(selectionToDelete, m_smartDelete);
        setSmartDelete(false);
        typingAddedToOpenCommand();
    }
    else {
        setEndingSelection(document()->frame()->selection().selection());
        closeTyping(this);
    }
}

bool TypingCommand::preservesTypingStyle() const
{
    switch (m_commandType) {
        case DeleteKey:
        case ForwardDeleteKey:
        case InsertParagraphSeparator:
        case InsertLineBreak:
            return true;
        case InsertParagraphSeparatorInQuotedContent:
        case InsertText:
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

} // namespace WebCore
