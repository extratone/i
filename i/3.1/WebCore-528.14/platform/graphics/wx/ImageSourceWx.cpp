/*
 * Copyright (C) 2006, 2007 Apple Computer, Kevin Ollivier.  All rights reserved.
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
#include "ImageSource.h"

#include "BMPImageDecoder.h"
#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "NotImplemented.h"
#include "PNGImageDecoder.h"
#include "SharedBuffer.h"
#include "XBMImageDecoder.h"

#include <wx/defs.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/rawbmp.h>

namespace WebCore {

ImageDecoder* createDecoder(const SharedBuffer& data)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing with.
    int length = data.size();
    if (length < 4)
        return 0;

    const unsigned char* uContents = (const unsigned char*)data.data();
    const char* contents = data.data();

    // GIFs begin with GIF8(7 or 9).
    if (strncmp(contents, "GIF8", 4) == 0)
        return new GIFImageDecoder();

    // Test for PNG.
    if (uContents[0]==0x89 &&
        uContents[1]==0x50 &&
        uContents[2]==0x4E &&
        uContents[3]==0x47)
        return new PNGImageDecoder();

    // JPEG
    if (uContents[0]==0xFF &&
        uContents[1]==0xD8 &&
        uContents[2]==0xFF)
        return new JPEGImageDecoder();

    // BMP
    if (strncmp(contents, "BM", 2) == 0)
        return new BMPImageDecoder();

    // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
    // CURs begin with 2-byte 0 followed by 2-byte 2.
    if (!memcmp(contents, "\000\000\001\000", 4) ||
        !memcmp(contents, "\000\000\002\000", 4))
        return new ICOImageDecoder();
   
    // XBMs require 8 bytes of info.
    if (length >= 8 && strncmp(contents, "#define ", 8) == 0)
        return new XBMImageDecoder();

    // Give up. We don't know what the heck this is.
    return 0;
}

ImageSource::ImageSource()
  : m_decoder(0)
{}

ImageSource::~ImageSource()
{
    clear(true);
}

bool ImageSource::initialized() const
{
    return m_decoder;
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    // Make the decoder by sniffing the bytes.
    // This method will examine the data and instantiate an instance of the appropriate decoder plugin.
    // If insufficient bytes are available to determine the image type, no decoder plugin will be
    // made.
    m_decoder = createDecoder(*data);
    if (!m_decoder)
        return;
    m_decoder->setData(data, allDataReceived);
}

bool ImageSource::isSizeAvailable()
{
    if (!m_decoder)
        return false;

    return m_decoder->isSizeAvailable();
}

IntSize ImageSource::size() const
{
    if (!m_decoder)
        return IntSize();

    return m_decoder->size();
}

IntSize ImageSource::frameSizeAtIndex(size_t) const
{
    return size();
}

int ImageSource::repetitionCount()
{
    if (!m_decoder)
        return cAnimationNone;

    return m_decoder->repetitionCount();
}

String ImageSource::filenameExtension() const
{
    notImplemented();
    return String();
}

size_t ImageSource::frameCount() const
{
    return m_decoder ? m_decoder->frameCount() : 0;
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    // FIXME: should we be testing the RGBA32Buffer's status as well?
    return (m_decoder && m_decoder->frameBufferAtIndex(index) != 0);
}

void ImageSource::clear(bool destroyAll, size_t clearBeforeFrame, SharedBuffer* data, bool allDataReceived)
{
    if (!destroyAll) {
        if (m_decoder)
            m_decoder->clearFrameBufferCache(clearBeforeFrame);
        return;
    }

    delete m_decoder;
    m_decoder = 0;
    if (data)
      setData(data, allDataReceived);
}

NativeImagePtr ImageSource::createFrameAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return 0;
    
    IntRect imageRect = buffer->rect();
    unsigned char* bytes = (unsigned char*)buffer->bytes().data();
    long colorSize = buffer->bytes().size();
    
    typedef wxPixelData<wxBitmap, wxAlphaPixelFormat> PixelData;

    int width = size().width();
    int height = size().height();

    wxBitmap* bmp = new wxBitmap(width, height, 32);
    PixelData data(*bmp);
    
    int rowCounter = 0;
    long pixelCounter = 0;
    
    PixelData::Iterator p(data);
    
    PixelData::Iterator rowStart = p; 
    
    // NB: It appears that the data is in BGRA format instead of RGBA format.
    // This code works properly on both ppc and intel, meaning the issue is
    // likely not an issue of byte order getting mixed up on different archs. 
    for (long i = 0; i < buffer->bytes().size()*4; i+=4) {
        p.Red() = bytes[i+2];
        p.Green() = bytes[i+1];
        p.Blue() = bytes[i+0];
        p.Alpha() = bytes[i+3];
        
        p++;

        pixelCounter++;
        if ( (pixelCounter % width ) == 0 ) {
            rowCounter++;
            p = rowStart;
            p.MoveTo(data, 0, rowCounter);
        }

    }
#if !wxCHECK_VERSION(2,9,0)
    bmp->UseAlpha();
#endif
    ASSERT(bmp->IsOk());
    return bmp;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return 0;

    float duration = buffer->duration() / 1000.0f;

    // Follow other ports (and WinIE's) behavior to slow annoying ads that
    // specify a 0 duration.
    if (duration < 0.051f)
        return 0.100f;
    return duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    if (!m_decoder || !m_decoder->supportsAlpha())
        return false;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return false;

    return buffer->hasAlpha();
}

}
