/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>
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
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Pattern.h"
#include "PlatformString.h"

#include <cairo.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_surface(0)
{
}

ImageBuffer::ImageBuffer(const IntSize& size, bool grayScale, bool& success)
    : m_data(size)
    , m_size(size)
{
    success = false;  // Make early return mean error.
    m_data.m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                  size.width(),
                                                  size.height());
    if (cairo_surface_status(m_data.m_surface) != CAIRO_STATUS_SUCCESS)
        return;  // create will notice we didn't set m_initialized and fail.

    cairo_t* cr = cairo_create(m_data.m_surface);
    m_context.set(new GraphicsContext(cr));
    cairo_destroy(cr);  // The context is now owned by the GraphicsContext.
    success = true;
}

ImageBuffer::~ImageBuffer()
{
    cairo_surface_destroy(m_data.m_surface);
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        // BitmapImage will release the passed in surface on destruction
        m_image = BitmapImage::create(cairo_surface_reference(m_data.m_surface));
    }
    return m_image.get();
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect& rect) const
{
    ASSERT(cairo_surface_get_type(m_data.m_surface) == CAIRO_SURFACE_TYPE_IMAGE);

    PassRefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* dataSrc = cairo_image_surface_get_data(m_data.m_surface);
    unsigned char* dataDst = result->data()->data()->data();

    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > m_size.width() || (rect.y() + rect.height()) > m_size.height())
        memset(dataSrc, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > m_size.width())
        endx = m_size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > m_size.height())
        endy = m_size.height();
    int numRows = endy - originy;

    int stride = cairo_image_surface_get_stride(m_data.m_surface);
    unsigned destBytesPerRow = 4 * rect.width();

    unsigned char* destRows = dataDst + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        unsigned char *row = dataSrc + stride * (y + originy);
        for (int x = 0; x < numColumns; x++) {
            uint32_t *pixel = (uint32_t *) row + x + originx;
            int basex = x * 4;
            if (unsigned int alpha = (*pixel & 0xff000000) >> 24) {
                destRows[basex] = (*pixel & 0x00ff0000) >> 16;
                destRows[basex + 1] = (*pixel & 0x0000ff00) >> 8;
                destRows[basex + 2] = (*pixel & 0x000000ff);
                destRows[basex + 3] = alpha;
            } else
                reinterpret_cast<uint32_t*>(destRows + basex)[0] = pixel[0];
        }
        destRows += destBytesPerRow;
    }

    return result;
}

void ImageBuffer::putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    ASSERT(cairo_surface_get_type(m_data.m_surface) == CAIRO_SURFACE_TYPE_IMAGE);

    unsigned char* dataDst = cairo_image_surface_get_data(m_data.m_surface);

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    unsigned srcBytesPerRow = 4 * source->width();
    int stride = cairo_image_surface_get_stride(m_data.m_surface);

    unsigned char* srcRows = source->data()->data()->data() + originy * srcBytesPerRow + originx * 4;
    for (int y = 0; y < numRows; ++y) {
        unsigned char *row = dataDst + stride * (y + desty);
        for (int x = 0; x < numColumns; x++) {
            uint32_t *pixel = (uint32_t *) row + x + destx;
            int basex = x * 4;
            if (unsigned int alpha = srcRows[basex + 3]) {
                *pixel = alpha << 24 | srcRows[basex] << 16 | srcRows[basex + 1] << 8 | srcRows[basex + 2];
            } else
                pixel[0] = reinterpret_cast<uint32_t*>(srcRows + basex)[0];
        }
        srcRows += srcBytesPerRow;
    }
}

static cairo_status_t writeFunction(void* closure, const unsigned char* data, unsigned int length)
{
    Vector<char>* in = reinterpret_cast<Vector<char>*>(closure);
    in->append(data, length);
    return CAIRO_STATUS_SUCCESS;
}

String ImageBuffer::toDataURL(const String& mimeType) const
{
    cairo_surface_t* image = cairo_get_target(context()->platformContext());
    if (!image)
        return "data:,";

    String actualMimeType("image/png");
    if (MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        actualMimeType = mimeType;

    Vector<char> in;
    // Only PNG output is supported for now.
    cairo_surface_write_to_png_stream(image, writeFunction, &in);

    Vector<char> out;
    base64Encode(in, out);

    return "data:" + actualMimeType + ";base64," + String(out.data(), out.size());
}

} // namespace WebCore
