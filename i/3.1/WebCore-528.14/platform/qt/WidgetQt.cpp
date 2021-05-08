/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 George Stiakos <staikos@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * All rights reserved.
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

#include "Cursor.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "ScrollView.h"
#include "Widget.h"
#include "NotImplemented.h"

#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebpage.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPaintEngine>

#include <QDebug>

namespace WebCore {

Widget::Widget(QWidget* widget)
{
    init(widget);
}

Widget::~Widget()
{
    Q_ASSERT(!parent());
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;

    frameRectsChanged();
}

void Widget::setFocus()
{
}

void Widget::setCursor(const Cursor& cursor)
{
#ifndef QT_NO_CURSOR
    if (QWidget* widget = root()->hostWindow()->platformWindow())
        QCoreApplication::postEvent(widget, new SetCursorEvent(cursor.impl()));
#endif
}

void Widget::show()
{
    if (platformWidget())
        platformWidget()->show();
}

void Widget::hide()
{
    if (platformWidget())
        platformWidget()->hide();
}

void Widget::paint(GraphicsContext *, const IntRect &rect)
{
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
