/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef TextBox_h
#define TextBox_h

#include "GraphicsTypes.h"
#include "ScrollView.h"
#include "TextDirection.h"

namespace WebCore {

    class Color;
    class String;

class TextBox : public ScrollView {
 public:
    typedef enum { 
        NoWrap,
        WidgetWidth
    } WrapStyle;

    typedef enum {
        PlainText,
    } TextFormat;

    TextBox(Widget* parent);
    ~TextBox();

    void setColors(const Color& background, const Color& foreground);

    void setAlignment(HorizontalAlignment);
    void setLineHeight(int lineHeight);

    void setCursorPosition(int, int);
    void getCursorPosition(int *, int *) const;

    void setFont(const Font&);

    void setReadOnly(bool);
    bool isReadOnly() const;

    void setDisabled(bool);
    bool isDisabled() const;

    bool hasSelectedText() const;
    
    void setText(const String&);
    String text() const;
    String textWithHardLineBreaks() const;

    void setTextFormat(TextFormat) { }

    void setWordWrap(WrapStyle);
    WrapStyle wordWrap() const;

    void setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode);

    void setWritingDirection(TextDirection);

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    
    void selectAll();
    void setSelectionRange(int, int);

    IntSize sizeWithColumnsAndRows(int numColumns, int numRows) const;

    virtual FocusPolicy focusPolicy() const;
    virtual bool checksDescendantsForFocus() const;
};

}

#endif /* TextBox_h */
