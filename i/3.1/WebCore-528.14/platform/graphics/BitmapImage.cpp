/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "BitmapImage.h"

#include "FloatRect.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "PlatformString.h"
#include "Timer.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>

#include "SystemTime.h"

namespace WebCore {

static int frameBytes(const IntSize& frameSize)
{
    return frameSize.width() * frameSize.height() * 4;
}

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_imageAnimationDisabled(false)
    , m_progressiveLoadChunkTime(0)
    , m_progressiveLoadChunkCount(0)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_desiredFrameStartTime(0)
    , m_isSolidColor(false)
    , m_animationFinished(false)
    , m_allDataReceived(false)
    , m_haveSize(false)
    , m_sizeAvailable(false)
    , m_hasUniformFrameSize(true)
    , m_decodedSize(0)
    , m_haveFrameCount(false)
    , m_frameCount(0)
{
    initPlatformData();
}

BitmapImage::~BitmapImage()
{
    invalidatePlatformData();
    stopAnimation();
}

void BitmapImage::destroyDecodedData(bool destroyAll)
{
    int deltaBytes = 0;
    const size_t clearBeforeFrame = destroyAll ? m_frames.size() : m_currentFrame;
    for (size_t i = 0; i < clearBeforeFrame; ++i) {
        // The underlying frame isn't actually changing (we're just trying to
        // save the memory for the framebuffer data), so we don't need to clear
        // the metadata.
        int bytes = m_frames[i].m_bytes;
        if (m_frames[i].clear(false))
            deltaBytes -= bytes;
    }

    destroyMetadataAndNotify(deltaBytes);

    m_source.clear(destroyAll, clearBeforeFrame, m_data.get(), m_allDataReceived);
    return;
}

void BitmapImage::destroyDecodedDataIfNecessary(bool destroyAll)
{
    // Animated images >5MB are considered large enough that we'll only hang on
    // to one frame at a time.
    static const unsigned cLargeAnimationCutoff = 2097152;
    if (frameCount() * frameBytes(m_size) > cLargeAnimationCutoff)
        destroyDecodedData(destroyAll);
}

void BitmapImage::destroyMetadataAndNotify(const int deltaBytes)
{
    ASSERT(deltaBytes <= 0);
    ASSERT(static_cast<int>(m_decodedSize) + deltaBytes >= 0);
    m_isSolidColor = false;
    invalidatePlatformData();

    m_decodedSize += deltaBytes;
    if (deltaBytes && imageObserver())
        imageObserver()->decodedSizeChanged(this, deltaBytes);
}

void BitmapImage::cacheFrame(size_t index, float scaleHint)
{
    size_t numFrames = frameCount();
    ASSERT(m_decodedSize == 0 || numFrames > 1);
    
    if (m_frames.size() < numFrames)
        m_frames.grow(numFrames);

    if (m_sizeAvailable && imageObserver() && !imageObserver()->shouldDecodeFrame(this, m_size))
        return;

    m_frames[index].m_frame = m_source.createFrameAtIndex(index, scaleHint, &m_frames[index].m_scale, &m_frames[index].m_bytes);
    if (numFrames == 1 && m_frames[index].m_frame)
        checkForSolidColor();

#if ENABLE(RESPECT_EXIF_ORIENTATION)
    m_frames[index].m_orientation = m_source.orientationAtIndex(index);
#endif
    m_frames[index].m_haveMetadata = true;
    m_frames[index].m_isComplete = m_source.frameIsCompleteAtIndex(index);
    if (repetitionCount(false) != cAnimationNone)
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);
    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);

    const IntSize frameSize(index ? m_source.frameSizeAtIndex(index) : m_size);
    if (frameSize != m_size)
        m_hasUniformFrameSize = false;
    if (m_frames[index].m_frame) {
        const int deltaBytes = m_frames[index].m_bytes;
        m_decodedSize += deltaBytes;
        if (imageObserver())
            imageObserver()->decodedSizeChanged(this, deltaBytes);
    }
}

void BitmapImage::cacheFrameInfo(size_t index)
{
    size_t numFrames = frameCount();

    if (m_frames.size() < numFrames)
        m_frames.resize(numFrames);

    ASSERT(!m_frames[index].m_haveInfo);

    if (shouldAnimate())
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);
    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);
    m_frames[index].m_haveInfo = true;
}

IntSize BitmapImage::size() const
{
    if (m_sizeAvailable && !m_haveSize) {
        m_size = m_source.size();
        m_haveSize = true;
    }
    return m_size;
}

IntSize BitmapImage::currentFrameSize() const
{
    if (!m_currentFrame || m_hasUniformFrameSize)
        return size();
    return m_source.frameSizeAtIndex(m_currentFrame);
}

bool BitmapImage::dataChanged(bool allDataReceived)
{
    // Because we're modifying the current frame, clear its (now possibly
    // inaccurate) metadata as well.
    int deltaBytes = 0;
    if (!m_frames.isEmpty()) {
        int bytes = m_frames[m_frames.size() - 1].m_bytes;
        if (m_frames[m_frames.size() - 1].clear(true))
            deltaBytes -= bytes;
    }
    destroyMetadataAndNotify(deltaBytes);
    
    // Feed all the data we've seen so far to the image decoder.
    m_allDataReceived = allDataReceived;

    static const double chunkLoadIntervals[] = {0.0, 1.0, 3.0, 6.0, 15.0};
    double interval = chunkLoadIntervals[std::min(m_progressiveLoadChunkCount, 4u)];
    
    bool needsUpdate = false;
    if (currentTime() - m_progressiveLoadChunkTime > interval) { // the first time through, the chunk time will be 0 and the image will get an update.
        needsUpdate = true;
        m_progressiveLoadChunkTime = currentTime();
        m_progressiveLoadChunkCount++;
    }
    if (needsUpdate || allDataReceived)
        m_source.setData(m_data.get(), allDataReceived);
    
    // Clear the frame count.
    m_haveFrameCount = false;

    m_hasUniformFrameSize = true;

    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.
    return isSizeAvailable();
}

String BitmapImage::filenameExtension() const
{
    return m_source.filenameExtension();
}

size_t BitmapImage::frameCount()
{
    if (!m_haveFrameCount) {
        m_haveFrameCount = true;
        m_frameCount = m_source.frameCount();
    }
    return m_frameCount;
}

bool BitmapImage::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_source.isSizeAvailable();

    return m_sizeAvailable;
}

NativeImagePtr BitmapImage::frameAtIndex(size_t index)
{
    return frameAtIndex(index, 1.0f);
}

NativeImagePtr BitmapImage::frameAtIndex(size_t index, float scaleHint)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index, scaleHint);
    else if (std::min(1.0f, scaleHint) > m_frames[index].m_scale) {
        // If the image is already cached, but at too small a size, re-decode a larger version.
        int sizeChange = -m_frames[index].m_bytes;
        ASSERT(static_cast<int>(m_decodedSize) + sizeChange >= 0);
        m_frames[index].clear(true);
        invalidatePlatformData();
        m_decodedSize += sizeChange;
        if (imageObserver())
            imageObserver()->decodedSizeChanged(this, sizeChange);

        cacheFrame(index, scaleHint);
    }

    return m_frames[index].m_frame;
}

bool BitmapImage::frameIsCompleteAtIndex(size_t index)
{
    if (index >= frameCount())
        return true;

    if (index >= m_frames.size() || !m_frames[index].m_haveInfo)
        cacheFrameInfo(index);

    return m_frames[index].m_isComplete;
}

float BitmapImage::frameDurationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_haveInfo)
        cacheFrameInfo(index);

    return m_frames[index].m_duration;
}

bool BitmapImage::frameHasAlphaAtIndex(size_t index)
{
    if (index >= frameCount())
        return true;

    if (index >= m_frames.size() || !m_frames[index].m_haveInfo)
        cacheFrameInfo(index);

    return m_frames[index].m_hasAlpha;
}

#if ENABLE(RESPECT_EXIF_ORIENTATION)
int BitmapImage::frameOrientationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 1;
        
    if (index >= m_frames.size() || !m_frames[index].m_haveInfo)
        cacheFrameInfo(index);
        
    return m_frames[index].m_orientation;
}
#endif

int BitmapImage::repetitionCount(bool imageKnownToBeComplete)
{
    if ((m_repetitionCountStatus == Unknown) || ((m_repetitionCountStatus == Uncertain) && imageKnownToBeComplete)) {
        // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
        // repetition count may not be accurate yet for GIFs; in this case the
        // decoder will default to cAnimationLoopOnce, and we'll try and read
        // the count again once the whole image is decoded.
        m_repetitionCount = m_source.repetitionCount();
        m_repetitionCountStatus = (imageKnownToBeComplete || m_repetitionCount == cAnimationNone) ? Certain : Uncertain;
    }
    return m_repetitionCount;
}

bool BitmapImage::shouldAnimate()
{
    return (repetitionCount(false) != cAnimationNone && !m_animationFinished && imageObserver());
}

void BitmapImage::startAnimation(bool catchUpIfNecessary)
{
    if (m_frameTimer || !shouldAnimate() || frameCount() <= 1)
        return;

    // Determine time for next frame to start.  By ignoring paint and timer lag
    // in this calculation, we make the animation appear to run at its desired
    // rate regardless of how fast it's being repainted.
    const double currentDuration = frameDurationAtIndex(m_currentFrame);
    const double time = currentTime();
    if (m_desiredFrameStartTime == 0) {
        m_desiredFrameStartTime = time + currentDuration;
    } else {
        m_desiredFrameStartTime += currentDuration;

        // Maintaining frame-to-frame delays is more important than
        // maintaining absolute animation timing, so reset the timings each frame.
        m_desiredFrameStartTime = time + currentDuration;
    }

    // Don't advance the animation to an incomplete frame.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!m_allDataReceived && !frameIsCompleteAtIndex(nextFrame))
        return;

    // Don't advance past the last frame if we haven't decoded the whole image
    // yet and our repetition count is potentially unset.  The repetition count
    // in a GIF can potentially come after all the rest of the image data, so
    // wait on it.
    if (!m_allDataReceived && repetitionCount(false) == cAnimationLoopOnce && m_currentFrame >= (frameCount() - 1))
        return;

    // The image may load more slowly than it's supposed to animate, so that by
    // the time we reach the end of the first repetition, we're well behind.
    // Clamp the desired frame start time in this case, so that we don't skip
    // frames (or whole iterations) trying to "catch up".  This is a tradeoff:
    // It guarantees users see the whole animation the second time through and
    // don't miss any repetitions, and is closer to what other browsers do; on
    // the other hand, it makes animations "less accurate" for pages that try to
    // sync an image and some other resource (e.g. audio), especially if users
    // switch tabs (and thus stop drawing the animation, which will pause it)
    // during that initial loop, then switch back later.
    if (nextFrame == 0 && m_repetitionsComplete == 0 && m_desiredFrameStartTime < time)
      m_desiredFrameStartTime = time;

    if (!catchUpIfNecessary || time < m_desiredFrameStartTime) {
        // Haven't yet reached time for next frame to start; delay until then.
        m_frameTimer = new Timer<BitmapImage>(this, &BitmapImage::advanceAnimation);
        m_frameTimer->startOneShot(std::max(m_desiredFrameStartTime - time, 0.));
    } else {
        // We've already reached or passed the time for the next frame to start.
        // See if we've also passed the time for frames after that to start, in
        // case we need to skip some frames entirely.  Remember not to advance
        // to an incomplete frame.
        for (size_t frameAfterNext = (nextFrame + 1) % frameCount(); frameIsCompleteAtIndex(frameAfterNext); frameAfterNext = (nextFrame + 1) % frameCount()) {
            // Should we skip the next frame?
            double frameAfterNextStartTime = m_desiredFrameStartTime + frameDurationAtIndex(nextFrame);
            if (time < frameAfterNextStartTime)
                break;

            // Yes; skip over it without notifying our observers.
            if (!internalAdvanceAnimation(true))
                return;
            m_desiredFrameStartTime = frameAfterNextStartTime;
            nextFrame = frameAfterNext;
        }

        // Draw the next frame immediately.  Note that m_desiredFrameStartTime
        // may be in the past, meaning the next time through this function we'll
        // kick off the next advancement sooner than this frame's duration would
        // suggest.
        if (internalAdvanceAnimation(false)) {
            // The image region has been marked dirty, but once we return to our
            // caller, draw() will clear it, and nothing will cause the
            // animation to advance again.  We need to start the timer for the
            // next frame running, or the animation can hang.  (Compare this
            // with when advanceAnimation() is called, and the region is dirtied
            // while draw() is not in the callstack, meaning draw() gets called
            // to update the region and thus startAnimation() is reached again.)
            // NOTE: For large images with slow or heavily-loaded systems,
            // throwing away data as we go (see destroyDecodedData()) means we
            // can spend so much time re-decoding data above that by the time we
            // reach here we're behind again.  If we let startAnimation() run
            // the catch-up code again, we can get long delays without painting
            // as we race the timer, or even infinite recursion.  In this
            // situation the best we can do is to simply change frames as fast
            // as possible, so force startAnimation() to set a zero-delay timer
            // and bail out if we're not caught up.
            startAnimation(false);
        }
    }
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    delete m_frameTimer;
    m_frameTimer = 0;
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_desiredFrameStartTime = 0;
    m_animationFinished = false;
    
    // For extremely large animations, when the animation is reset, we just throw everything away.
    destroyDecodedDataIfNecessary(true);
}

void BitmapImage::advanceAnimation(Timer<BitmapImage>*)
{
    internalAdvanceAnimation(false);
    // At this point the image region has been marked dirty, and if it's
    // onscreen, we'll soon make a call to draw(), which will call
    // startAnimation() again to keep the animation moving.
}

bool BitmapImage::internalAdvanceAnimation(bool skippingFrames)
{
    // Stop the animation.
    stopAnimation();
    

    ++m_currentFrame;
    bool advancedAnimation = true;
    bool destroyAll = false;
    if (m_currentFrame >= frameCount()) {
        ++m_repetitionsComplete;

        // Get the repetition count again.  If we weren't able to get a
        // repetition count before, we should have decoded the whole image by
        // now, so it should now be available.
        if (repetitionCount(true) && m_repetitionsComplete >= m_repetitionCount) {
            m_animationFinished = true;
            m_desiredFrameStartTime = 0;
            --m_currentFrame;
            advancedAnimation = false;
        } else {
            m_currentFrame = 0;
            destroyAll = true;
        }
    }
    destroyDecodedDataIfNecessary(destroyAll);

    // We need to draw this frame if we advanced to it while not skipping, or if
    // while trying to skip frames we hit the last frame and thus had to stop.
    if (skippingFrames != advancedAnimation)
        imageObserver()->animationAdvanced(this);
    return advancedAnimation;
}

unsigned BitmapImage::animatedImageSize()
{
    if (frameCount() <= 1)
        return 0;
    return (width() * height() * 4 * frameCount());
}
    
void BitmapImage::disableImageAnimation()
{
    m_imageAnimationDisabled = true;
}
    
}
