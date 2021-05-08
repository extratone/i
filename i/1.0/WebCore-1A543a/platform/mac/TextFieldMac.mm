/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "TextField.h"

#import "Color.h"
#import "IntSize.h"

using namespace WebCore;

TextField::TextField(Type aType)
{
}

TextField::~TextField()
{
}

void TextField::setText(const String& string)
{
    // Not implemented.
}

String TextField::text() const
{
    // Not implemented.
    return String();
}

void TextField::setCursorPosition(int index)
{
    // Not implemented.
}

void TextField::setMaxLength(int len)
{
    // Not implemented.
}

bool TextField::isReadOnly() const
{
    return false;
}

void TextField::setReadOnly(bool flag)
{
    // Not implemented.
}

int TextField::maxLength() const
{
    return 0;
}

void TextField::setLiveSearch(bool liveSearch)
{
    // Not implemented.
}

void TextField::setAutoSaveName(const String& name)
{
    // Not implemented.
}

void TextField::setMaxResults(int maxResults)
{
    // Not implemented.
}

void TextField::setPlaceholderString(const String& placeholder)
{
    // Not implemented.
}

void TextField::addSearchResult()
{
    // Not implemented.
}

void TextField::setFont(const Font& font)
{
    // Not implemented.
}

void TextField::setAlignment(HorizontalAlignment alignment)
{
    // Not implemented.
}

void TextField::setWritingDirection(TextDirection direction)
{
    // Not implemented.
}

Widget::FocusPolicy TextField::focusPolicy() const
{
//    FocusPolicy policy = ScrollView::focusPolicy();
//    return policy == TabFocus ? StrongFocus : policy;
    return TabFocus;
}

bool TextField::checksDescendantsForFocus() const
{
    return true;
}

void TextField::setColors(const Color& background, const Color& foreground){
    // Not implemented.
}

int TextField::selectionStart() const
{
    // Not implemented.
    return 0;
}

void TextField::setSelection(int start, int length)
{
    // Not implemented.
}

bool TextField::hasSelectedText() const
{
    // Not implemented.
    return false;
}

void TextField::selectAll()
{
    // Not implemented.
}

String TextField::selectedText() const
{
    // Not implemented.
    return String("");
}

int TextField::cursorPosition() const
{
    // Not implemented.
    return 0;
}

void TextField::setEdited(bool flag)
{
    // Not implemented.
}

IntSize TextField::sizeForCharacterWidth(int numCharacters) const
{
    // Not implemented.
    
    return IntSize(0, 0);
}

bool TextField::edited() const
{
    // Not implemented.
    return 0;
}

int TextField::baselinePosition(int height) const
{
    // Not implemented.
    return 0;
}




