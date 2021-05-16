/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"

#include "AtomicString.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CreateLinkCommand.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FormatBlockCommand.h"
#include "HTMLFontElement.h"
#include "HTMLImageElement.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "Page.h"
#include "ReplaceSelectionCommand.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "Sound.h"
#include "TypingCommand.h"
#include "UnlinkCommand.h"
#include "htmlediting.h"
#include "markup.h"

namespace WebCore {

using namespace HTMLNames;

class EditorInternalCommand {
public:
    bool (*execute)(Frame*, Event*, EditorCommandSource, const String&);
    bool (*isSupported)(Frame*, EditorCommandSource);
    bool (*isEnabled)(Frame*, Event*, EditorCommandSource);
    TriState (*state)(Frame*, Event*);
    String (*value)(Frame*, Event*);
    bool isTextInsertion;
    bool allowExecutionWhenDisabled;
};

typedef HashMap<String, const EditorInternalCommand*, CaseFoldingHash> CommandMap;

static const bool notTextInsertion = false;
static const bool isTextInsertion = true;

static const bool allowExecutionWhenDisabled = true;
static const bool doNotAllowExecutionWhenDisabled = false;

// Related to Editor::selectionForCommand.
// Certain operations continue to use the target control's selection even if the event handler
// already moved the selection outside of the text control.
static Frame* targetFrame(Frame* frame, Event* event)
{
    if (!event)
        return frame;
    Node* node = event->target()->toNode();
    if (!node)
        return frame;
    return node->document()->frame();
}

static bool executeApplyStyle(Frame* frame, EditorCommandSource source, EditAction action, int propertyID, const String& propertyValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            frame->editor()->applyStyleToSelection(style.get(), action);
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            frame->editor()->applyStyle(style.get());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeApplyStyle(Frame* frame, EditorCommandSource source, EditAction action, int propertyID, const char* propertyValue)
{
    return executeApplyStyle(frame, source, action, propertyID, String(propertyValue));
}

static bool executeApplyStyle(Frame* frame, EditorCommandSource source, EditAction action, int propertyID, int propertyValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            frame->editor()->applyStyleToSelection(style.get(), action);
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            frame->editor()->applyStyle(style.get());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeToggleStyle(Frame* frame, EditorCommandSource source, EditAction action, int propertyID, const char* offValue, const char* onValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(propertyID, onValue);
    style->setProperty(propertyID, frame->editor()->selectionStartHasStyle(style.get()) ? offValue : onValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            frame->editor()->applyStyleToSelection(style.get(), action);
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            frame->editor()->applyStyle(style.get());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeApplyParagraphStyle(Frame* frame, EditorCommandSource source, EditAction action, int propertyID, const String& propertyValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            frame->editor()->applyParagraphStyleToSelection(style.get(), action);
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            frame->editor()->applyParagraphStyle(style.get());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertFragment(Frame* frame, PassRefPtr<DocumentFragment> fragment)
{
    applyCommand(ReplaceSelectionCommand::create(frame->document(), fragment,
        false, false, false, true, false, EditActionUnspecified));
    return true;
}

static bool executeInsertNode(Frame* frame, PassRefPtr<Node> content)
{
    RefPtr<DocumentFragment> fragment = new DocumentFragment(frame->document());
    ExceptionCode ec = 0;
    fragment->appendChild(content, ec);
    if (ec)
        return false;
    return executeInsertFragment(frame, fragment.release());
}

static bool expandSelectionToGranularity(Frame* frame, TextGranularity granularity)
{
    Selection selection = frame->selection()->selection();
    selection.expandUsingGranularity(granularity);
    RefPtr<Range> newRange = selection.toRange();
    if (!newRange)
        return false;
    ExceptionCode ec = 0;
    if (newRange->collapsed(ec))
        return false;
    RefPtr<Range> oldRange = frame->selection()->selection().toRange();
    EAffinity affinity = frame->selection()->affinity();
    if (!frame->editor()->client()->shouldChangeSelectedRange(oldRange.get(), newRange.get(), affinity, false))
        return false;
    frame->selection()->setSelectedRange(newRange.get(), affinity, true);
    return true;
}

static TriState stateStyle(Frame* frame, int propertyID, const char* desiredValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(propertyID, desiredValue);
    return frame->editor()->selectionHasStyle(style.get());
}

static String valueStyle(Frame* frame, int propertyID)
{
    return frame->selectionStartStylePropertyValue(propertyID);
}

static TriState stateTextWritingDirection(Frame* frame, WritingDirection direction)
{
    bool hasNestedOrMultipleEmbeddings;
    WritingDirection selectionDirection = frame->editor()->textDirectionForSelection(hasNestedOrMultipleEmbeddings);
    return (selectionDirection == direction && !hasNestedOrMultipleEmbeddings) ? TrueTriState : FalseTriState;
}

static int verticalScrollDistance(Frame* frame)
{
    Node* focusedNode = frame->document()->focusedNode();
    if (!focusedNode)
        return 0;
    RenderObject* renderer = focusedNode->renderer();
    if (!renderer || !renderer->isBox())
        return 0;
    RenderStyle* style = renderer->style();
    if (!style)
        return 0;
    if (!(style->overflowY() == OSCROLL || style->overflowY() == OAUTO || renderer->isTextArea()))
        return 0;
    int height = toRenderBox(renderer)->clientHeight();
    return max((height + 1) / 2, height - cAmountToKeepWhenPaging);
}

static RefPtr<Range> unionDOMRanges(Range* a, Range* b)
{
    ExceptionCode ec = 0;
    Range* start = a->compareBoundaryPoints(Range::START_TO_START, b, ec) <= 0 ? a : b;
    ASSERT(!ec);
    Range* end = a->compareBoundaryPoints(Range::END_TO_END, b, ec) <= 0 ? b : a;
    ASSERT(!ec);

    return Range::create(a->startContainer(ec)->ownerDocument(), start->startContainer(ec), start->startOffset(ec), end->endContainer(ec), end->endOffset(ec));
}

// Execute command functions

static bool executeBackColor(Frame* frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetBackgroundColor, CSSPropertyBackgroundColor, value);
}

static bool executeCopy(Frame* frame, Event*, EditorCommandSource, const String&)
{
    UNUSED_PARAM(frame);
    return true;
}

static bool executeCreateLink(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box to let the user enter a URL.
    if (value.isEmpty())
        return false;
    applyCommand(CreateLinkCommand::create(frame->document(), value));
    return true;
}

static bool executeCut(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->cut();
    return true;
}

static bool executeClearText(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->clearText();
    return true;
}

static bool executeDelete(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            // Doesn't modify the text if the current selection isn't a range.
            frame->editor()->performDelete();
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            // If the current selection is a caret, delete the preceding character. IE performs forwardDelete, but we currently side with Firefox.
            // Doesn't scroll to make the selection visible, or modify the kill ring (this time, siding with IE, not Firefox).
            TypingCommand::deleteKeyPressed(frame->document(), frame->selectionGranularity() == WordGranularity);
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeDeleteBackward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteBackwardByDecomposingPreviousCharacter(Frame* frame, Event*, EditorCommandSource, const String&)
{
    LOG_ERROR("DeleteBackwardByDecomposingPreviousCharacter is not implemented, doing DeleteBackward instead");
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteForward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::FORWARD, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteToBeginningOfLine(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToBeginningOfParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfLine(Frame* frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like DeleteToEndOfParagraph).
    frame->editor()->deleteWithDirection(SelectionController::FORWARD, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph.
    frame->editor()->deleteWithDirection(SelectionController::FORWARD, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToMark(Frame* frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<Range> mark = frame->mark().toRange();
    if (mark) {
        SelectionController* selection = frame->selection();
        bool selected = selection->setSelectedRange(unionDOMRanges(mark.get(), frame->editor()->selectedRange().get()).get(), DOWNSTREAM, true);
        ASSERT(selected);
        if (!selected)
            return false;
    }
    frame->editor()->performDelete();
    frame->setMark(frame->selection()->selection());
    return true;
}

static bool executeDeleteWordBackward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::BACKWARD, WordGranularity, true, false);
    return true;
}

static bool executeDeleteWordForward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->deleteWithDirection(SelectionController::FORWARD, WordGranularity, true, false);
    return true;
}

static bool executeFindString(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    return frame->findString(value, true, false, true, false);
}

static bool executeFontName(Frame* frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetFont, CSSPropertyFontFamily, value);
}

static bool executeFontSize(Frame* frame, Event*, EditorCommandSource source, const String& value)
{
    int size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return false;
    return executeApplyStyle(frame, source, EditActionChangeAttributes, CSSPropertyFontSize, size);
}

static bool executeFontSizeDelta(Frame* frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionChangeAttributes, CSSPropertyWebkitFontSizeDelta, value);
}

static bool executeForeColor(Frame* frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetColor, CSSPropertyColor, value);
}

static bool executeFormatBlock(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    String tagName = value.lower();
    if (tagName[0] == '<' && tagName[tagName.length() - 1] == '>')
        tagName = tagName.substring(1, tagName.length() - 2);
    if (!validBlockTag(tagName))
        return false;
    applyCommand(FormatBlockCommand::create(frame->document(), tagName));
    return true;
}

static bool executeForwardDelete(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            frame->editor()->deleteWithDirection(SelectionController::FORWARD, CharacterGranularity, false, true);
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            // Doesn't scroll to make the selection visible, or modify the kill ring.
            // ForwardDelete is not implemented in IE or Firefox, so this behavior is only needed for
            // backward compatibility with ourselves, and for consistency with Delete.
            TypingCommand::forwardDeleteKeyPressed(frame->document());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeIgnoreSpelling(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->ignoreSpelling();
    return true;
}

static bool executeIndent(Frame* frame, Event*, EditorCommandSource, const String&)
{
    applyCommand(IndentOutdentCommand::create(frame->document(), IndentOutdentCommand::Indent));
    return true;
}

static bool executeInsertBacktab(Frame* frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler()->handleTextInputEvent("\t", event, false, true);
}

static bool executeInsertHorizontalRule(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    RefPtr<HTMLElement> hr = new HTMLElement(hrTag, frame->document());
    if (!value.isEmpty())
        hr->setId(value);
    return executeInsertNode(frame, hr.release());
}

static bool executeInsertHTML(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    return executeInsertFragment(frame, createFragmentFromMarkup(frame->document(), value, ""));
}

static bool executeInsertImage(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box and let the user choose a local image.
    RefPtr<HTMLImageElement> image = new HTMLImageElement(imgTag, frame->document());
    image->setSrc(value);
    return executeInsertNode(frame, image.release());
}

static bool executeInsertLineBreak(Frame* frame, Event* event, EditorCommandSource source, const String&)
{
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            return targetFrame(frame, event)->eventHandler()->handleTextInputEvent("\n", event, true);
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            // Doesn't scroll to make the selection visible, or modify the kill ring.
            // InsertLineBreak is not implemented in IE or Firefox, so this behavior is only needed for
            // backward compatibility with ourselves, and for consistency with other commands.
            TypingCommand::insertLineBreak(frame->document());
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertNewline(Frame* frame, Event* event, EditorCommandSource, const String&)
{
    Frame* targetFrame = WebCore::targetFrame(frame, event);
    return targetFrame->eventHandler()->handleTextInputEvent("\n", event, !targetFrame->editor()->canEditRichly());
}

static bool executeInsertNewlineInQuotedContent(Frame* frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparatorInQuotedContent(frame->document());
    return true;
}

static bool executeInsertOrderedList(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    applyCommand(InsertListCommand::create(frame->document(), InsertListCommand::OrderedList, value));
    return true;
}

static bool executeInsertParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparator(frame->document());
    return true;
}

static bool executeInsertTab(Frame* frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler()->handleTextInputEvent("\t", event, false, false);
}

static bool executeInsertText(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    TypingCommand::insertText(frame->document(), value);
    return true;
}

static bool executeInsertUnorderedList(Frame* frame, Event*, EditorCommandSource, const String& value)
{
    applyCommand(InsertListCommand::create(frame->document(), InsertListCommand::UnorderedList, value));
    return true;
}

static bool executeJustifyCenter(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionCenter, CSSPropertyTextAlign, "center");
}

static bool executeJustifyFull(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionJustify, CSSPropertyTextAlign, "justify");
}

static bool executeJustifyLeft(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionAlignLeft, CSSPropertyTextAlign, "left");
}

static bool executeJustifyRight(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionAlignRight, CSSPropertyTextAlign, "right");
}

static bool executeMakeTextWritingDirectionLeftToRight(Frame* frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueLtr);
    frame->editor()->applyStyle(style.get(), EditActionSetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionNatural(Frame* frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
    frame->editor()->applyStyle(style.get(), EditActionSetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionRightToLeft(Frame* frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueRtl);
    frame->editor()->applyStyle(style.get(), EditActionSetWritingDirection);
    return true;
}

static bool executeMoveBackward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool executeMoveBackwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, CharacterGranularity, true);
    return true;
}

static bool executeMoveDown(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool executeMoveDownAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineGranularity, true);
    return true;
}

static bool executeMoveForward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool executeMoveForwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, CharacterGranularity, true);
    return true;
}

static bool executeMoveLeft(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool executeMoveLeftAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::LEFT, CharacterGranularity, true);
    return true;
}

static bool executeMovePageDown(Frame* frame, Event*, EditorCommandSource, const String&)
{
    int distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame->selection()->modify(SelectionController::MOVE, distance, true);
}

static bool executeMovePageDownAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    int distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame->selection()->modify(SelectionController::EXTEND, distance, true);
}

static bool executeMovePageUp(Frame* frame, Event*, EditorCommandSource, const String&)
{
    int distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame->selection()->modify(SelectionController::MOVE, -distance, true);
}

static bool executeMovePageUpAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    int distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame->selection()->modify(SelectionController::EXTEND, -distance, true);
}

static bool executeMoveRight(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool executeMoveRightAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::RIGHT, CharacterGranularity, true);
    return true;
}

static bool executeMoveToBeginningOfDocument(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfDocumentAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, DocumentBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfLine(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfLineAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfParagraphAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfSentence(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool executeMoveToBeginningOfSentenceAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, SentenceBoundary, true);
    return true;
}

static bool executeMoveToEndOfDocument(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool executeMoveToEndOfDocumentAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, DocumentBoundary, true);
    return true;
}

static bool executeMoveToEndOfSentence(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool executeMoveToEndOfSentenceAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, SentenceBoundary, true);
    return true;
}

static bool executeMoveToEndOfLine(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool executeMoveToEndOfLineAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, LineBoundary, true);
    return true;
}

static bool executeMoveToEndOfParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool executeMoveToEndOfParagraphAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphBoundary, true);
    return true;
}

static bool executeMoveParagraphBackwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, ParagraphGranularity, true);
    return true;
}

static bool executeMoveParagraphForwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, ParagraphGranularity, true);
    return true;
}

static bool executeMoveUp(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool executeMoveUpAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, LineGranularity, true);
    return true;
}

static bool executeMoveWordBackward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool executeMoveWordBackwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::BACKWARD, WordGranularity, true);
    return true;
}

static bool executeMoveWordForward(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool executeMoveWordForwardAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::FORWARD, WordGranularity, true);
    return true;
}

static bool executeMoveWordLeft(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool executeMoveWordLeftAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::LEFT, WordGranularity, true);
    return true;
}

static bool executeMoveWordRight(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::MOVE, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool executeMoveWordRightAndModifySelection(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->modify(SelectionController::EXTEND, SelectionController::RIGHT, WordGranularity, true);
    return true;
}

static bool executeOutdent(Frame* frame, Event*, EditorCommandSource, const String&)
{
    applyCommand(IndentOutdentCommand::create(frame->document(), IndentOutdentCommand::Outdent));
    return true;
}

static bool executePaste(Frame* frame, Event*, EditorCommandSource, const String&)
{
    UNUSED_PARAM(frame);
    return true;
}

static bool executePasteAndMatchStyle(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->pasteAsPlainText();
    return true;
}

static bool executePrint(Frame* frame, Event*, EditorCommandSource, const String&)
{
    Page* page = frame->page();
    if (!page)
        return false;
    page->chrome()->print(frame);
    return true;
}

static bool executeRedo(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->redo();
    return true;
}

static bool executeRemoveFormat(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->removeFormattingAndStyle();
    return true;
}

static bool executeSelectAll(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->selectAll();
    return true;
}

static bool executeSelectLine(Frame* frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, LineGranularity);
}

static bool executeSelectParagraph(Frame* frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, ParagraphGranularity);
}

static bool executeSelectSentence(Frame* frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, SentenceGranularity);
}

static bool executeSelectToMark(Frame* frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<Range> mark = frame->mark().toRange();
    RefPtr<Range> selection = frame->editor()->selectedRange();
    if (!mark || !selection) {
        systemBeep();
        return false;
    }
    frame->selection()->setSelectedRange(unionDOMRanges(mark.get(), selection.get()).get(), DOWNSTREAM, true);
    return true;
}

static bool executeSelectWord(Frame* frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, WordGranularity);
}

static bool executeSetMark(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->setMark(frame->selection()->selection());
    return true;
}

static bool executeStrikethrough(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionChangeAttributes, CSSPropertyWebkitTextDecorationsInEffect, "none", "line-through");
}

static bool executeSubscript(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionSubscript, CSSPropertyVerticalAlign, "baseline", "sub");
}

static bool executeSuperscript(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionSuperscript, CSSPropertyVerticalAlign, "baseline", "super");
}

static bool executeSwapWithMark(Frame* frame, Event*, EditorCommandSource, const String&)
{
    const Selection& mark = frame->mark();
    const Selection& selection = frame->selection()->selection();
    if (mark.isNone() || selection.isNone()) {
        systemBeep();
        return false;
    }
    frame->selection()->setSelection(mark);
    frame->setMark(selection);
    return true;
}

static bool executeToggleBold(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionChangeAttributes, CSSPropertyFontWeight, "normal", "bold");
}

static bool executeToggleItalic(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionChangeAttributes, CSSPropertyFontStyle, "normal", "italic");
}

static bool executeTranspose(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->transpose();
    return true;
}

static bool executeUnderline(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    // FIXME: This currently clears overline, line-through, and blink as an unwanted side effect.
    return executeToggleStyle(frame, source, EditActionUnderline, CSSPropertyWebkitTextDecorationsInEffect, "none", "underline");
}

static bool executeUndo(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->undo();
    return true;
}

static bool executeUnlink(Frame* frame, Event*, EditorCommandSource, const String&)
{
    applyCommand(UnlinkCommand::create(frame->document()));
    return true;
}

static bool executeUnscript(Frame* frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyStyle(frame, source, EditActionUnscript, CSSPropertyVerticalAlign, "baseline");
}

static bool executeUnselect(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->selection()->clear();
    return true;
}

static bool executeYank(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->insertTextWithoutSendingTextEvent(frame->editor()->yankFromKillRing(), false, 0);
    frame->editor()->setKillRingToYankedState();
    return true;
}

static bool executeYankAndSelect(Frame* frame, Event*, EditorCommandSource, const String&)
{
    frame->editor()->insertTextWithoutSendingTextEvent(frame->editor()->yankFromKillRing(), true, 0);
    frame->editor()->setKillRingToYankedState();
    return true;
}

// Supported functions

static bool supported(Frame*, EditorCommandSource)
{
    return true;
}

static bool supportedFromMenuOrKeyBinding(Frame*, EditorCommandSource source)
{
    return source == CommandFromMenuOrKeyBinding;
}

static bool supportedPaste(Frame* frame, EditorCommandSource source)
{
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            return true;
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface: {
            Settings* settings = frame ? frame->settings() : 0;
            return settings && settings->isDOMPasteAllowed();
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

// Enabled functions

static bool enabled(Frame*, Event*, EditorCommandSource)
{
    return true;
}

static bool enabledVisibleSelection(Frame* frame, Event* event, EditorCommandSource)
{
    // The term "visible" here includes a caret in editable text or a range in any text.
    const Selection& selection = frame->editor()->selectionForCommand(event);
    return (selection.isCaret() && selection.isContentEditable()) || selection.isRange();
}

static bool enabledVisibleSelectionAndMark(Frame* frame, Event* event, EditorCommandSource)
{
    const Selection& selection = frame->editor()->selectionForCommand(event);
    return ((selection.isCaret() && selection.isContentEditable()) || selection.isRange())
        && frame->mark().isCaretOrRange();
}

static bool enableCaretInEditableText(Frame* frame, Event* event, EditorCommandSource)
{
    const Selection& selection = frame->editor()->selectionForCommand(event);
    return selection.isCaret() && selection.isContentEditable();
}

static bool enabledCopy(Frame* frame, Event*, EditorCommandSource)
{
    UNUSED_PARAM(frame);
    return false;
}

static bool enabledCut(Frame* frame, Event*, EditorCommandSource)
{
    UNUSED_PARAM(frame);
    return false;
}

static bool enabledClearText(Frame *frame, Event*, EditorCommandSource)
{
    UNUSED_PARAM(frame);
    return false;
}

static bool enabledDelete(Frame* frame, Event* event, EditorCommandSource source)
{
    switch (source) {
        case CommandFromMenuOrKeyBinding:
            // "Delete" from menu only affects selected range, just like Cut but without affecting pasteboard
            return enabledCut(frame, event, source);
        case CommandFromDOM:
        case CommandFromDOMWithUserInterface:
            // "Delete" from DOM is like delete/backspace keypress, affects selected range if non-empty,
            // otherwise removes a character
            return frame->editor()->selectionForCommand(event).isContentEditable();
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledInEditableText(Frame* frame, Event* event, EditorCommandSource)
{
    return frame->editor()->selectionForCommand(event).isContentEditable();
}

static bool enabledInRichlyEditableText(Frame* frame, Event*, EditorCommandSource)
{
    return frame->selection()->isCaretOrRange() && frame->selection()->isContentRichlyEditable();
}

static bool enabledPaste(Frame* frame, Event*, EditorCommandSource)
{
    return frame->editor()->canPaste();
}

static bool enabledRangeInEditableText(Frame* frame, Event*, EditorCommandSource)
{
    return frame->selection()->isRange() && frame->selection()->isContentEditable();
}

static bool enabledRangeInRichlyEditableText(Frame* frame, Event*, EditorCommandSource)
{
    return frame->selection()->isRange() && frame->selection()->isContentRichlyEditable();
}

static bool enabledRedo(Frame* frame, Event*, EditorCommandSource)
{
    return frame->editor()->canRedo();
}

static bool enabledUndo(Frame* frame, Event*, EditorCommandSource)
{
    return frame->editor()->canUndo();
}

// State functions

static TriState stateNone(Frame*, Event*)
{
    return FalseTriState;
}

static TriState stateBold(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontWeight, "bold");
}

static TriState stateItalic(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontStyle, "italic");
}

static TriState stateOrderedList(Frame* frame, Event*)
{
    return frame->editor()->selectionOrderedListState();
}

static TriState stateStrikethrough(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextDecoration, "line-through");
}

static TriState stateSubscript(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "sub");
}

static TriState stateSuperscript(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "super");
}

static TriState stateTextWritingDirectionLeftToRight(Frame* frame, Event*)
{
    return stateTextWritingDirection(frame, LeftToRightWritingDirection);
}

static TriState stateTextWritingDirectionNatural(Frame* frame, Event*)
{
    return stateTextWritingDirection(frame, NaturalWritingDirection);
}

static TriState stateTextWritingDirectionRightToLeft(Frame* frame, Event*)
{
    return stateTextWritingDirection(frame, RightToLeftWritingDirection);
}

static TriState stateUnderline(Frame* frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextDecoration, "underline");
}

static TriState stateUnorderedList(Frame* frame, Event*)
{
    return frame->editor()->selectionUnorderedListState();
}

// Value functions

static String valueNull(Frame*, Event*)
{
    return String();
}

static String valueBackColor(Frame* frame, Event*)
{
    return valueStyle(frame, CSSPropertyBackgroundColor);
}

static String valueFontName(Frame* frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontFamily);
}

static String valueFontSize(Frame* frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontSize);
}

static String valueFontSizeDelta(Frame* frame, Event*)
{
    return valueStyle(frame, CSSPropertyWebkitFontSizeDelta);
}

static String valueForeColor(Frame* frame, Event*)
{
    return valueStyle(frame, CSSPropertyColor);
}

// Map of functions

static const CommandMap& createCommandMap()
{
    struct CommandEntry { const char* name; EditorInternalCommand command; };
    
    static const CommandEntry commands[] = {
        { "AlignCenter", { executeJustifyCenter, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignJustified", { executeJustifyFull, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignLeft", { executeJustifyLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignRight", { executeJustifyRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "BackColor", { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueBackColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "BackwardDelete", { executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } }, // FIXME: remove BackwardDelete when Safari for Windows stops using it.
        { "Bold", { executeToggleBold, supported, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Copy", { executeCopy, supported, enabledCopy, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "CreateLink", { executeCreateLink, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Cut", { executeCut, supported, enabledCut, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "Delete", { executeDelete, supported, enabledDelete, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackward", { executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackwardByDecomposingPreviousCharacter", { executeDeleteBackwardByDecomposingPreviousCharacter, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteForward", { executeDeleteForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfLine", { executeDeleteToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfParagraph", { executeDeleteToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfLine", { executeDeleteToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfParagraph", { executeDeleteToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToMark", { executeDeleteToMark, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordBackward", { executeDeleteWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordForward", { executeDeleteWordForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FindString", { executeFindString, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontName", { executeFontName, supported, enabledInEditableText, stateNone, valueFontName, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSize", { executeFontSize, supported, enabledInEditableText, stateNone, valueFontSize, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSizeDelta", { executeFontSizeDelta, supported, enabledInEditableText, stateNone, valueFontSizeDelta, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForeColor", { executeForeColor, supported, enabledInRichlyEditableText, stateNone, valueForeColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FormatBlock", { executeFormatBlock, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForwardDelete", { executeForwardDelete, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "HiliteColor", { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "IgnoreSpelling", { executeIgnoreSpelling, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Indent", { executeIndent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertBacktab", { executeInsertBacktab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHorizontalRule", { executeInsertHorizontalRule, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHTML", { executeInsertHTML, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertImage", { executeInsertImage, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertLineBreak", { executeInsertLineBreak, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNewline", { executeInsertNewline, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },    
        { "InsertNewlineInQuotedContent", { executeInsertNewlineInQuotedContent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertOrderedList", { executeInsertOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertParagraph", { executeInsertParagraph, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertTab", { executeInsertTab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertText", { executeInsertText, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertUnorderedList", { executeInsertUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Italic", { executeToggleItalic, supported, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyCenter", { executeJustifyCenter, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyFull", { executeJustifyFull, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyLeft", { executeJustifyLeft, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyNone", { executeJustifyLeft, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyRight", { executeJustifyRight, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionLeftToRight", { executeMakeTextWritingDirectionLeftToRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionLeftToRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionNatural", { executeMakeTextWritingDirectionNatural, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionNatural, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionRightToLeft", { executeMakeTextWritingDirectionRightToLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionRightToLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackward", { executeMoveBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackwardAndModifySelection", { executeMoveBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDown", { executeMoveDown, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDownAndModifySelection", { executeMoveDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForward", { executeMoveForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForwardAndModifySelection", { executeMoveForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeft", { executeMoveLeft, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeftAndModifySelection", { executeMoveLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDown", { executeMovePageDown, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDownAndModifySelection", { executeMovePageDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUp", { executeMovePageUp, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUpAndModifySelection", { executeMovePageUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphBackwardAndModifySelection", { executeMoveParagraphBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphForwardAndModifySelection", { executeMoveParagraphForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRight", { executeMoveRight, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRightAndModifySelection", { executeMoveRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocument", { executeMoveToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocumentAndModifySelection", { executeMoveToBeginningOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLine", { executeMoveToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLineAndModifySelection", { executeMoveToBeginningOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraph", { executeMoveToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraphAndModifySelection", { executeMoveToBeginningOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentence", { executeMoveToBeginningOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentenceAndModifySelection", { executeMoveToBeginningOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocument", { executeMoveToEndOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocumentAndModifySelection", { executeMoveToEndOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLine", { executeMoveToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLineAndModifySelection", { executeMoveToEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraph", { executeMoveToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraphAndModifySelection", { executeMoveToEndOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentence", { executeMoveToEndOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentenceAndModifySelection", { executeMoveToEndOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUp", { executeMoveUp, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUpAndModifySelection", { executeMoveUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackward", { executeMoveWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackwardAndModifySelection", { executeMoveWordBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForward", { executeMoveWordForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForwardAndModifySelection", { executeMoveWordForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeft", { executeMoveWordLeft, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeftAndModifySelection", { executeMoveWordLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRight", { executeMoveWordRight, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRightAndModifySelection", { executeMoveWordRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Outdent", { executeOutdent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Paste", { executePaste, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "PasteAndMatchStyle", { executePasteAndMatchStyle, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "Print", { executePrint, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Redo", { executeRedo, supported, enabledRedo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "RemoveFormat", { executeRemoveFormat, supported, enabledRangeInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectAll", { executeSelectAll, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectLine", { executeSelectLine, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectParagraph", { executeSelectParagraph, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectSentence", { executeSelectSentence, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectToMark", { executeSelectToMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectWord", { executeSelectWord, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SetMark", { executeSetMark, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Strikethrough", { executeStrikethrough, supported, enabledInRichlyEditableText, stateStrikethrough, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Subscript", { executeSubscript, supported, enabledInRichlyEditableText, stateSubscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Superscript", { executeSuperscript, supported, enabledInRichlyEditableText, stateSuperscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SwapWithMark", { executeSwapWithMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleBold", { executeToggleBold, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleItalic", { executeToggleItalic, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleUnderline", { executeUnderline, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Transpose", { executeTranspose, supported, enableCaretInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Underline", { executeUnderline, supported, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Undo", { executeUndo, supported, enabledUndo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unlink", { executeUnlink, supported, enabledRangeInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unscript", { executeUnscript, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unselect", { executeUnselect, supported, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Yank", { executeYank, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "YankAndSelect", { executeYankAndSelect, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ClearText", { executeClearText, supported, enabledClearText, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
    };

    // These unsupported commands are listed here since they appear in the Microsoft
    // documentation used as the starting point for our DOM executeCommand support.
    //
    // 2D-Position (not supported)
    // AbsolutePosition (not supported)
    // BlockDirLTR (not supported)
    // BlockDirRTL (not supported)
    // BrowseMode (not supported)
    // ClearAuthenticationCache (not supported)
    // CreateBookmark (not supported)
    // DirLTR (not supported)
    // DirRTL (not supported)
    // EditMode (not supported)
    // InlineDirLTR (not supported)
    // InlineDirRTL (not supported)
    // InsertButton (not supported)
    // InsertFieldSet (not supported)
    // InsertIFrame (not supported)
    // InsertInputButton (not supported)
    // InsertInputCheckbox (not supported)
    // InsertInputFileUpload (not supported)
    // InsertInputHidden (not supported)
    // InsertInputImage (not supported)
    // InsertInputPassword (not supported)
    // InsertInputRadio (not supported)
    // InsertInputReset (not supported)
    // InsertInputSubmit (not supported)
    // InsertInputText (not supported)
    // InsertMarquee (not supported)
    // InsertSelectDropDown (not supported)
    // InsertSelectListBox (not supported)
    // InsertTextArea (not supported)
    // LiveResize (not supported)
    // MultipleSelection (not supported)
    // Open (not supported)
    // Overwrite (not supported)
    // PlayImage (not supported)
    // Refresh (not supported)
    // RemoveParaFormat (not supported)
    // SaveAs (not supported)
    // SizeToControl (not supported)
    // SizeToControlHeight (not supported)
    // SizeToControlWidth (not supported)
    // Stop (not supported)
    // StopImage (not supported)
    // Unbookmark (not supported)

    CommandMap& commandMap = *new CommandMap;

    const unsigned numCommands = sizeof(commands) / sizeof(commands[0]);
    for (unsigned i = 0; i < numCommands; i++) {
        ASSERT(!commandMap.get(commands[i].name));
        commandMap.set(commands[i].name, &commands[i].command);
    }

    return commandMap;
}

Editor::Command Editor::command(const String& commandName)
{
    return command(commandName, CommandFromMenuOrKeyBinding);
}

Editor::Command Editor::command(const String& commandName, EditorCommandSource source)
{
    if (commandName.isEmpty())
        return Command();

    static const CommandMap& commandMap = createCommandMap();
    const EditorInternalCommand* internalCommand = commandMap.get(commandName);
    return internalCommand ? Command(m_frame, internalCommand, source) : Command();
}

Editor::Command::Command()
    : m_command(0)
    , m_source()
{
}

Editor::Command::Command(PassRefPtr<Frame> frame, const EditorInternalCommand* command, EditorCommandSource source)
    : m_frame(frame)
    , m_command(command)
    , m_source(source)
{
    ASSERT(m_frame);
    ASSERT(m_command);
}

bool Editor::Command::execute(const String& parameter, Event* triggeringEvent) const
{
    if (!isEnabled(triggeringEvent)) {
        // Let certain commands be executed when performed explicitly even if they are disabled.
        if (!isSupported() || !m_frame || !m_frame->document() || !m_command->allowExecutionWhenDisabled)
            return false;
    }
    m_frame->document()->updateLayoutIgnorePendingStylesheets();
    return m_command->execute(m_frame.get(), triggeringEvent, m_source, parameter);
}

bool Editor::Command::execute(Event* triggeringEvent) const
{
    return execute(String(), triggeringEvent);
}

bool Editor::Command::isSupported() const
{
    return m_command && m_command->isSupported(m_frame.get(), m_source);
}

bool Editor::Command::isEnabled(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame || !m_frame->document())
        return false;
    return m_command->isEnabled(m_frame.get(), triggeringEvent, m_source);
}

TriState Editor::Command::state(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame || !m_frame->document())
        return FalseTriState;
    return m_command->state(m_frame.get(), triggeringEvent);
}

String Editor::Command::value(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame || !m_frame->document())
        return String();
    return m_command->value(m_frame.get(), triggeringEvent);
}

bool Editor::Command::isTextInsertion() const
{
    return m_command && m_command->isTextInsertion;
}

} // namespace WebCore
