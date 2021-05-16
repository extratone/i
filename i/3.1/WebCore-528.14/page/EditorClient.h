/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef EditorClient_h
#define EditorClient_h

#include "EditorInsertAction.h"
#include "PlatformString.h"
#include "TextAffinity.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
class NSArray;
class NSData;
class NSString;
class NSURL;
#endif

namespace WebCore {

class CSSStyleDeclaration;
class EditCommand;
class Element;
class Frame;
class HTMLElement;
class KeyboardEvent;
class Node;
class Range;
class Selection;
class String;
class VisiblePosition;

struct GrammarDetail {
    int location;
    int length;
    Vector<String> guesses;
    String userDescription;
};

class EditorClient {
public:
    virtual ~EditorClient() {  }
    virtual void pageDestroyed() = 0;
    
    virtual bool shouldDeleteRange(Range*) = 0;
    virtual bool shouldShowDeleteInterface(HTMLElement*) = 0;
    virtual bool smartInsertDeleteEnabled() = 0; 
    virtual bool isSelectTrailingWhitespaceEnabled() = 0;
    virtual bool isContinuousSpellCheckingEnabled() = 0;
    virtual void toggleContinuousSpellChecking() = 0;
    virtual bool isGrammarCheckingEnabled() = 0;
    virtual void toggleGrammarChecking() = 0;
    virtual int spellCheckerDocumentTag() = 0;
    
    virtual bool isEditable() = 0;

    virtual bool shouldBeginEditing(Range*) = 0;
    virtual bool shouldEndEditing(Range*) = 0;
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) = 0;
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) = 0;
    virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting) = 0;
    
    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*) = 0;
//  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle) = 0;
//  virtual bool doCommandBySelector(SEL selector) = 0;
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*) = 0;

    virtual void didBeginEditing() = 0;
    virtual void respondToChangedContents() = 0;
    virtual void respondToChangedSelection() = 0;
    virtual void didEndEditing() = 0;
    virtual void didWriteSelectionToPasteboard() = 0;
    virtual void didSetSelectionTypesForPasteboard() = 0;
//  virtual void didChangeTypingStyle:(NSNotification *)notification = 0;
//  virtual void didChangeSelection:(NSNotification *)notification = 0;
//  virtual NSUndoManager* undoManager:(WebView *)webView = 0;
    
    virtual void registerCommandForUndo(PassRefPtr<EditCommand>) = 0;
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>) = 0;
    virtual void clearUndoRedoOperations() = 0;

    virtual bool canUndo() const = 0;
    virtual bool canRedo() const = 0;
    
    virtual void undo() = 0;
    virtual void redo() = 0;

    virtual void handleKeyboardEvent(KeyboardEvent*) = 0;
    virtual void handleInputMethodKeydown(KeyboardEvent*) = 0;
    
    virtual void textFieldDidBeginEditing(Element*) = 0;
    virtual void textFieldDidEndEditing(Element*) = 0;
    virtual void textDidChangeInTextField(Element*) = 0;
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) = 0;
    virtual void textWillBeDeletedInTextField(Element*) = 0;
    virtual void textDidChangeInTextArea(Element*) = 0;
    virtual void formElementDidSetValue(Element*) = 0;
    virtual void formElementDidFocus(Element*) = 0;
    virtual void formElementDidBlur(Element*) = 0;
    virtual void suppressFormNotifications() = 0;
    virtual void restoreFormNotifications() = 0;
    virtual void suppressSelectionNotifications() = 0;
    virtual void restoreSelectionNotifications() = 0;
    virtual void startDelayingAndCoalescingContentChangeNotifications() = 0;
    virtual void stopDelayingAndCoalescingContentChangeNotifications() = 0;

#if PLATFORM(MAC)
    virtual NSString* userVisibleString(NSURL*) = 0;
#ifdef BUILDING_ON_TIGER
    virtual NSArray* pasteboardTypesForSelection(Frame*) = 0;
#endif
#endif

    virtual void ignoreWordInSpellDocument(const String&) = 0;
    virtual void learnWord(const String&) = 0;
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength) = 0;
    virtual void checkGrammarOfString(const UChar*, int length, Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) = 0;
    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail& detail) = 0;
    virtual void updateSpellingUIWithMisspelledWord(const String&) = 0;
    virtual void showSpellingUI(bool show) = 0;
    virtual bool spellingUIIsShowing() = 0;
    virtual void getGuessesForWord(const String&, Vector<String>& guesses) = 0;
    virtual void setInputMethodState(bool enabled) = 0;
};

}

#endif // EditorClient_h
