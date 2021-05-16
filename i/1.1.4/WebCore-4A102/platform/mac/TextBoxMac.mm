/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "TextBox.h"

#import "Font.h"
#import "IntSize.h"

using namespace WebCore;

TextBox::TextBox(Widget *parent)
{
    // Not implemented.
}

TextBox::~TextBox()
{
    // Not implemented.
}

void TextBox::setText(const String& string)
{
    // Not implemented.
}

String TextBox::text() const
{
    // Not implemented.
    
    return String();
}

String TextBox::textWithHardLineBreaks() const
{
    // Not implemented.
    
    return String();
}

void TextBox::getCursorPosition(int *paragraph, int *index) const
{
    // Not implemented.
}

void TextBox::setCursorPosition(int paragraph, int index)
{
    // Not implemented.
}

TextBox::WrapStyle TextBox::wordWrap() const
{
    // Not implemented.
    
    return NoWrap;
}

void TextBox::setWordWrap(WrapStyle style)
{
    // Not implemented.
}

void TextBox::setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode)
{
    // Not implemented.
}

bool TextBox::isReadOnly() const
{
    // Not implemented.
    return false;
}

void TextBox::setReadOnly(bool flag)
{
    // Not implemented.
}

bool TextBox::isDisabled() const
{
    // Not implemented.
    
    return false;
}

void TextBox::setDisabled(bool flag)
{
    // Not implemented.
}

int TextBox::selectionStart()
{
    // Not implemented.
    
    return 0;
}

int TextBox::selectionEnd()
{
    // Not implemented.
    
    return 0;
}

void TextBox::setSelectionStart(int start)
{
    // Not implemented.
}

void TextBox::setSelectionEnd(int end)
{
    // Not implemented.
}

bool TextBox::hasSelectedText() const
{
    // Not implemented.
    
    return 0;
}

void TextBox::selectAll()
{
    // Not implemented.
}

void TextBox::setSelectionRange(int start, int length)
{
    // Not implemented.
}

void TextBox::setFont(const Font& font)
{
    // Not implemented.
}

void TextBox::setAlignment(HorizontalAlignment alignment)
{
    // Not implemented.
}

void TextBox::setLineHeight(int lineHeight)
{
    // Not implemented.
}

void TextBox::setWritingDirection(TextDirection direction)
{
    // Not implemented.
}
 
IntSize TextBox::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
    // Not implemented.
    return IntSize(0, 0);
}

Widget::FocusPolicy TextBox::focusPolicy() const
{
    // Not implemented.
    return TabFocus;
}

bool TextBox::checksDescendantsForFocus() const
{
    return true;
}

void TextBox::setColors(const Color& background, const Color& foreground)
{
    // Not implemented.
}

