/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Icon.h"

#include <windows.h>
#include <shellapi.h>

#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "PlatformString.h"
#include "SkiaUtils.h"

namespace WebCore {

Icon::Icon(const PlatformIcon& icon)
    : m_icon(icon)
{
}

Icon::~Icon()
{
    if (m_icon)
        DestroyIcon(m_icon);
}

PassRefPtr<Icon> Icon::createIconForFile(const String& filename)
{
    SHFILEINFO sfi;
    memset(&sfi, 0, sizeof(sfi));

    String tmpFilename = filename;
    if (!SHGetFileInfo(tmpFilename.charactersWithNullTermination(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_SMALLICON))
        return 0;

    return adoptRef(new Icon(sfi.hIcon));
}

PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>& filenames)
{
    // FIXME: support multiple files.
    // http://code.google.com/p/chromium/issues/detail?id=4092
    if (!filenames.size())
        return 0;

    return createIconForFile(filenames[0]);
}

void Icon::paint(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled())
        return;

    HDC hdc = context->platformContext()->canvas()->beginPlatformPaint();
    DrawIconEx(hdc, rect.x(), rect.y(), m_icon, rect.width(), rect.height(), 0, 0, DI_NORMAL);
    context->platformContext()->canvas()->endPlatformPaint();
}

} // namespace WebCore
