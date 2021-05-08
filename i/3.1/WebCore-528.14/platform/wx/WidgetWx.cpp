/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "Widget.h"

#include "Cursor.h"
#include "GraphicsContext.h"
#include "IntRect.h"

#include <wx/defs.h>
#include <wx/scrolwin.h>

namespace WebCore {

Widget::Widget(PlatformWidget widget)
{
    init(widget);
}

Widget::~Widget()
{
}

void Widget::setFocus()
{
    if (platformWidget())
        platformWidget()->SetFocus();
}

void Widget::setCursor(const Cursor& cursor)
{
    if (platformWidget() && cursor.impl())
        platformWidget()->SetCursor(*cursor.impl());
}

void Widget::show()
{
    if (platformWidget())
        platformWidget()->Show();
}

void Widget::hide()
{
    if (platformWidget())
        platformWidget()->Hide();
}

IntRect Widget::frameRect() const
{
    if (platformWidget())
        return platformWidget()->GetRect();
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    if (platformWidget())
        platformWidget()->SetSize(rect);
    m_frame = rect;
}

void Widget::invalidateRect(const IntRect& r)
{
    if (platformWidget())
        platformWidget()->RefreshRect(r);
}

void Widget::paint(GraphicsContext*,const IntRect& r)
{
}

}
