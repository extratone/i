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
#import "ListBox.h"

#include "IntSize.h"

using namespace WebCore;

ListBox::ListBox()
    : _changingSelection(false)
    , _enabled(true)
    , _widthGood(false)
{
    //Not implemented.
}

ListBox::~ListBox()
{
    // Not implemented.
}

void ListBox::clear()
{
    // Not implemented.
}

void ListBox::setSelectionMode(SelectionMode mode)
{
    // Not implemented.
}

void ListBox::appendItem(const DeprecatedString &text, ListBoxItemType type, bool enabled)
{
    // Not implemented
}

void ListBox::doneAppendingItems()
{
    // Not implemented.
}

void ListBox::setSelected(int index, bool selectIt)
{
    // Not implemented.
}

bool ListBox::isSelected(int index) const
{
    // Not implemented.

    return false;
}

void ListBox::setEnabled(bool enabled)
{
    // Not implemented.
}

bool ListBox::isEnabled()
{
    // Not implemented.
    
    return true;
}

IntSize ListBox::sizeForNumberOfLines(int lines) const
{
    // Not implemented.
    
    return IntSize(0, 0);
}

Widget::FocusPolicy ListBox::focusPolicy() const
{
    return TabFocus;
}

bool ListBox::checksDescendantsForFocus() const
{
    return true;
}

void ListBox::setWritingDirection(TextDirection d)
{
    // Not implemented.
}

void ListBox::clearCachedTextRenderers()
{
    // Not implemented.
}

void ListBox::setFont(const Font& font)
{
    // Not implemented.
}

