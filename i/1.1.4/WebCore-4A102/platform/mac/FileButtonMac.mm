/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#import "FileButton.h"

#import "BlockExceptions.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreStringTruncator.h"
#import "WebCoreViewFactory.h"
#import "WidgetClient.h"

using namespace WebCore;

FileButton::FileButton(Frame *frame)
{
}

void FileButton::setFilename(const DeprecatedString &f)
{
}

void FileButton::click(bool sendMouseEvents)
{
}

IntSize FileButton::sizeForCharacterWidth(int characters) const
{
    return IntSize(0, 0);
}

IntRect FileButton::frameGeometry() const
{
    return IntRect();
}

void FileButton::setFrameGeometry(const IntRect &rect)
{
}

int FileButton::baselinePosition(int height) const
{
    return 0;
}

Widget::FocusPolicy FileButton::focusPolicy() const
{
    return Widget::focusPolicy();
}

void FileButton::filenameChanged(const DeprecatedString& filename)
{
    m_name = filename;
    if (client())
        client()->valueChanged(this);
}

void FileButton::setDisabled(bool flag)
{
}
