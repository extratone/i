/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PopupMenuStyle_h
#define PopupMenuStyle_h

#include "Color.h"
#include "Font.h"

namespace WebCore {

class PopupMenuStyle {
public:
    PopupMenuStyle(const Color& foreground, const Color& background, const Font& font, bool visible)
        : m_foregroundColor(foreground)
        , m_backgroundColor(background)
        , m_font(font)
        , m_visible(visible)
    {
    }
    
    const Color& foregroundColor() const { return m_foregroundColor; }
    const Color& backgroundColor() const { return m_backgroundColor; }
    const Font& font() const { return m_font; }
    bool isVisible() const { return m_visible; }

private:
    Color m_foregroundColor;
    Color m_backgroundColor;
    Font m_font;
    bool m_visible;
};

} // namespace WebCore

#endif // PopupMenuStyle_h
