/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "GIFImageDecoder.h"
#include "GIFImageReader.h"

namespace WebCore {

class GIFImageDecoderPrivate
{
public:
    GIFImageDecoderPrivate(GIFImageDecoder* decoder = 0)
        : m_reader(decoder)
    {
        m_readOffset = 0;
    }

    ~GIFImageDecoderPrivate()
    {
        m_reader.close();
    }

    bool decode(const Vector<char>& data, 
                GIFImageDecoder::GIFQuery query = GIFImageDecoder::GIFFullQuery,
                unsigned int haltFrame = -1)
    {
        return m_reader.read((const unsigned char*)data.data() + m_readOffset, data.size() - m_readOffset, 
                             query,
                             haltFrame);
    }

    unsigned frameCount() const { return m_reader.images_count; }
    int repetitionCount() const { return m_reader.loop_count; }

    void setReadOffset(unsigned o) { m_readOffset = o; }

    bool isTransparent() const { return m_reader.frame_reader->is_transparent; }

    void getColorMap(unsigned char*& map, unsigned& size) const {
        if (m_reader.frame_reader->is_local_colormap_defined) {
            map = m_reader.frame_reader->local_colormap;
            size = (unsigned)m_reader.frame_reader->local_colormap_size;
        } else {
            map = m_reader.global_colormap;
            size = m_reader.global_colormap_size;
        }
    }

    unsigned frameXOffset() const { return m_reader.frame_reader->x_offset; }
    unsigned frameYOffset() const { return m_reader.frame_reader->y_offset; }
    unsigned frameWidth() const { return m_reader.frame_reader->width; }
    unsigned frameHeight() const { return m_reader.frame_reader->height; }

    int transparentPixel() const { return m_reader.frame_reader->tpixel; }

    unsigned duration() const { return m_reader.frame_reader->delay_time; }

private:
    GIFImageReader m_reader;
    unsigned m_readOffset;
};

GIFImageDecoder::GIFImageDecoder()
: m_frameCountValid(true), m_reader(0)
{}

GIFImageDecoder::~GIFImageDecoder()
{
    delete m_reader;
}

// Take the data and store it.
void GIFImageDecoder::setData(const Vector<char>& data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our new data.
    ImageDecoder::setData(data, allDataReceived);

    // Our frame count is now unknown.
    m_frameCountValid = false;

    // Create the GIF reader.
    if (!m_reader && !m_failed)
        m_reader = new GIFImageDecoderPrivate(this);
}

// Whether or not the size information has been decoded yet.
bool GIFImageDecoder::isSizeAvailable() const
{
    // If we have pending data to decode, send it to the GIF reader now.
    if (!m_sizeAvailable && m_reader) {
        if (m_failed)
            return false;

        // The decoder will go ahead and aggressively consume everything up until the first
        // size is encountered.
        decode(GIFSizeQuery, 0);
    }

    return m_sizeAvailable;
}

// The total number of frames for the image.  Will scan the image data for the answer
// (without necessarily decoding all of the individual frames).
int GIFImageDecoder::frameCount()
{
    // If the decoder had an earlier error, we will just return what we had decoded
    // so far.
    if (!m_frameCountValid) {
        // FIXME: Scanning all the data has O(n^2) behavior if the data were to come in really
        // slowly.  Might be interesting to try to clone our existing read session to preserve
        // state, but for now we just crawl all the data.  Note that this is no worse than what
        // ImageIO does on Mac right now (it also crawls all the data again).
        GIFImageDecoderPrivate reader;
        reader.decode(m_data, GIFFrameCountQuery);
        m_frameCountValid = true;
        m_frameBufferCache.resize(reader.frameCount());
    }

    return m_frameBufferCache.size();
}

// The number of repetitions to perform for an animation loop.
int GIFImageDecoder::repetitionCount() const
{
    // We don't have to do any decoding to determine this, since the loop count was determined after
    // the initial query for size.
    if (m_reader)
        return m_reader->repetitionCount();
    return cAnimationNone;
}

RGBA32Buffer* GIFImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index < 0 || index >= m_frameBufferCache.size())
        return 0;

    RGBA32Buffer& frame = m_frameBufferCache[index];
    if (frame.status() != RGBA32Buffer::FrameComplete && m_reader)
        // Decode this frame.
        decode(GIFFullQuery, index+1);
    return &frame;
}

// Feed data to the GIF reader.
void GIFImageDecoder::decode(GIFQuery query, unsigned haltAtFrame) const
{
    if (m_failed)
        return;

    m_failed = !m_reader->decode(m_data, query, haltAtFrame);
    
    if (m_failed) {
        delete m_reader;
        m_reader = 0;
    }
}

// Callbacks from the GIF reader.
void GIFImageDecoder::sizeNowAvailable(unsigned width, unsigned height)
{
    m_size = IntSize(width, height);
    m_sizeAvailable = true;
}

void GIFImageDecoder::decodingHalted(unsigned bytesLeft)
{
    m_reader->setReadOffset(m_data.size() - bytesLeft);
}

void GIFImageDecoder::initFrameBuffer(RGBA32Buffer& buffer, 
                                      RGBA32Buffer* previousBuffer,
                                      bool compositeWithPreviousFrame)
{
    // Initialize the frame rect in our buffer.
    IntRect frameRect(m_reader->frameXOffset(), m_reader->frameYOffset(),
                      m_reader->frameWidth(), m_reader->frameHeight());
    buffer.setRect(frameRect);

    bool isSubRect = (frameRect.x() > 0 || frameRect.y() > 0 ||
                      frameRect.width() < m_size.width() ||
                      frameRect.height() < m_size.height());
    
    // Let's resize our buffer now to the correct width/height and then
    // initialize portions of it if needed.
    RGBA32Array& bytes = buffer.bytes();
        
    // If the disposal method of the previous frame said to stick around, then we need     
    // to copy that frame into our frame.  We also dont want to have any impact on
    // anything outside our frame's rect, so if we don't overlay the entire image,
    // then also composite with the previous frame.
    if (previousBuffer && (compositeWithPreviousFrame || isSubRect)) {
        bytes = previousBuffer->bytes();
        buffer.ensureHeight(m_size.height());
        buffer.setHasAlpha(previousBuffer->hasAlpha());
    }
    else // Resize to the width and height of the image.
        bytes.resize(m_size.width() * m_size.height());

    if (isSubRect) {
        // We need to go ahead and initialize the first frame to make sure
        // that areas outside the subrect start off transparent.
        if (!previousBuffer) {
            bytes.fill(0);
            buffer.setHasAlpha(true);
        } else if (!compositeWithPreviousFrame) {
            // Now this is an interesting case.  In the case where we fill 
            // the entire image, we effectively do a full clear of the image (and thus
            // don't have to initialize anything in our buffer).
            // 
            // However in the case where we only fill a piece of the image, two problems occur:
            // (1) We need to wipe out the area occupied by the previous frame, which
            // could also have been a subrect.
            // (2) Anything outside the previous frame's rect *and* outside our current
            // frame's rect should be left alone.
            // We have handled (2) by just initializing our buffer from the previous frame.
            // Our subrect will correctly overwrite the previous frame's contents as we
            // decode rows.  However that still leaves the problem of having to wipe out
            // the area occupied by the previous frame that does not overlap with
            // the new frame.
            if (previousBuffer->rect() != frameRect) {
                // We have to clear out the entire previous subframe.
                bool sawAlpha = buffer.hasAlpha();
                IntRect prevRect = previousBuffer->rect();
                unsigned end = prevRect.y() + prevRect.height();
                unsigned* src;
                for (unsigned i = prevRect.y(); i < end; i++) {
                    unsigned* curr = buffer.bytes().data() + (i * m_size.width() + prevRect.x());
                    unsigned* end = curr + prevRect.width();
                    while (curr != end) {
                        if (!sawAlpha) {
                            sawAlpha = true;
                            buffer.setHasAlpha(true);
                        }
                        RGBA32Buffer::setRGBA(*curr++, 0, 0, 0, 0);
                    }
                }
            }
        }
    }

    // Update our status to be partially complete.
    buffer.setStatus(RGBA32Buffer::FramePartial);
}

void GIFImageDecoder::haveDecodedRow(unsigned frameIndex,
                                     unsigned char* rowBuffer,   // Pointer to single scanline temporary buffer
                                     unsigned char* rowEnd,
                                     unsigned rowNumber,  // The row index
                                     unsigned repeatCount) // How many times to repeat the row
{
    // Resize to the width and height of the image.
    RGBA32Buffer& buffer = m_frameBufferCache[frameIndex];
    RGBA32Buffer* previousBuffer = (frameIndex > 0) ? &m_frameBufferCache[frameIndex-1] : 0;
    bool compositeWithPreviousFrame = previousBuffer && previousBuffer->includeInNextFrame();

    if (buffer.status() == RGBA32Buffer::FrameEmpty)
        initFrameBuffer(buffer, previousBuffer, compositeWithPreviousFrame);

    if (rowBuffer == 0)
      return;

    unsigned colorMapSize;
    unsigned char* colorMap;
    m_reader->getColorMap(colorMap, colorMapSize);
    if (!colorMap)
        return;

    // The buffers that we draw are the entire image's width and height, so a final output frame is
    // width * height RGBA32 values in size.
    //
    // A single GIF frame, however, can be smaller than the entire image, i.e., it can represent some sub-rectangle
    // within the overall image.  The rows we are decoding are within this
    // sub-rectangle.  This means that if the GIF frame's sub-rectangle is (x,y,w,h) then row 0 is really row
    // y, and each row goes from x to x+w.
    unsigned dstPos = (m_reader->frameYOffset() + rowNumber) * m_size.width() + m_reader->frameXOffset();
    unsigned* dst = buffer.bytes().data() + dstPos;
    unsigned* currDst = dst;
    unsigned char* currentRowByte = rowBuffer;
    
    bool hasAlpha = m_reader->isTransparent(); 
    bool sawAlpha = false;
    while (currentRowByte != rowEnd) {
        if ((!hasAlpha || *currentRowByte != m_reader->transparentPixel()) && *currentRowByte < colorMapSize) {
            unsigned colorIndex = *currentRowByte * 3;
            unsigned red = colorMap[colorIndex];
            unsigned green = colorMap[colorIndex + 1];
            unsigned blue = colorMap[colorIndex + 2];
            RGBA32Buffer::setRGBA(*currDst, red, green, blue, 255);
        } else {
            if (!sawAlpha) {
                sawAlpha = true;
                buffer.setHasAlpha(true);
            }
            
            if (!compositeWithPreviousFrame)
                RGBA32Buffer::setRGBA(*currDst, 0, 0, 0, 0);
        }
        currDst++;
        currentRowByte++;
    }

    if (repeatCount > 1) {
        // Copy the row |repeatCount|-1 times.
        unsigned size = (currDst - dst) * sizeof(unsigned);
        unsigned width = m_size.width();
        unsigned* end = buffer.bytes().data() + width * m_size.height();
        currDst = dst + width;
        for (unsigned i = 1; i < repeatCount; i++) {
            if (currDst + size > end) // Protect against a buffer overrun from a bogus repeatCount.
                break;
            memcpy(currDst, dst, size);
            currDst += width;
        }
    }

    // Our partial height is rowNumber + 1, e.g., row 2 is the 3rd row, so that's a height of 3.
    // Adding in repeatCount - 1 to rowNumber + 1 works out to just be rowNumber + repeatCount.
    buffer.ensureHeight(rowNumber + repeatCount);
}

void GIFImageDecoder::frameComplete(unsigned frameIndex, unsigned frameDuration, bool includeInNextFrame)
{
    RGBA32Buffer& buffer = m_frameBufferCache[frameIndex];
    buffer.setStatus(RGBA32Buffer::FrameComplete);
    buffer.setDuration(frameDuration);
    buffer.setIncludeInNextFrame(includeInNextFrame);
    buffer.ensureHeight(m_size.height());
}

void GIFImageDecoder::gifComplete()
{
    delete m_reader;
    m_reader = 0;
}

}
