/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef BitmapImage_h
#define BitmapImage_h

#include "Image.h"
#include "Color.h"
#include "IntSize.h"

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(WIN)
typedef struct HBITMAP__ *HBITMAP;
#endif

namespace WebCore {
    struct FrameData;
}

// This complicated-looking declaration tells the FrameData Vector that it should copy without
// invoking our constructor or destructor. This allows us to have a vector even for a struct
// that's not copyable.
namespace WTF {
    template<> class VectorTraits<WebCore::FrameData> : public SimpleClassVectorTraits {};
}

namespace WebCore {

template <typename T> class Timer;

// ================================================
// FrameData Class
// ================================================

struct FrameData : Noncopyable {
    FrameData()
        : m_frame(0)
        , m_haveMetadata(false)
        , m_isComplete(false)
#if ENABLE(RESPECT_EXIF_ORIENTATION)
        , m_orientation(0)
#endif
        , m_bytes(0)
        , m_scale(0.0f)
        , m_haveInfo(false)
        , m_duration(0)
        , m_hasAlpha(true) 
    {
    }

    ~FrameData()
    { 
        clear(true);
    }

    // Clear the cached image data on the frame, and (optionally) the metadata.
    // Returns whether there was cached image data to clear.
    bool clear(bool clearMetadata);

    NativeImagePtr m_frame;
    bool m_haveMetadata;
    bool m_isComplete;
#if ENABLE(RESPECT_EXIF_ORIENTATION)
    int m_orientation;
#endif
    ssize_t m_bytes;
    float m_scale;
    bool m_haveInfo;
    float m_duration;
    bool m_hasAlpha;
};

// =================================================
// BitmapImage Class
// =================================================

class BitmapImage : public Image {
    friend class GeneratedImage;
    friend class GraphicsContext;
public:
    static PassRefPtr<BitmapImage> create(NativeImagePtr nativeImage, ImageObserver* observer = 0)
    {
        return adoptRef(new BitmapImage(nativeImage, observer));
    }
    static PassRefPtr<BitmapImage> create(ImageObserver* observer = 0)
    {
        return adoptRef(new BitmapImage(observer));
    }
    ~BitmapImage();
    
    virtual bool isBitmapImage() const { return true; }

    virtual bool hasSingleSecurityOrigin() const { return true; }

    virtual IntSize size() const;
    IntSize currentFrameSize() const;

    virtual bool dataChanged(bool allDataReceived);
    virtual String filenameExtension() const; 

    // It may look unusual that there is no start animation call as public API.  This is because
    // we start and stop animating lazily.  Animation begins whenever someone draws the image.  It will
    // automatically pause once all observers no longer want to render the image anywhere.
    virtual void stopAnimation();
    virtual void resetAnimation();
    
    virtual unsigned decodedSize() const { return m_decodedSize; }

#if PLATFORM(MAC)
    // Accessors for native image formats.
    virtual CFDataRef getTIFFRepresentation();
#endif
    
#if PLATFORM(CG)
    virtual CGImageRef getCGImageRef();
#endif

#if PLATFORM(WIN)
    virtual bool getHBITMAP(HBITMAP);
    virtual bool getHBITMAPOfSize(HBITMAP, LPSIZE);
#endif

    virtual NativeImagePtr nativeImageForCurrentFrame() { return frameAtIndex(currentFrame()); }

#if ENABLE(RESPECT_EXIF_ORIENTATION)    
    // EXIF orientation specified by EXIF spec
    static const int ImageEXIFOrientationTopLeft = 1;
    static const int ImageEXIFOrientationTopRight = 2;
    static const int ImageEXIFOrientationBottomRight = 3;
    static const int ImageEXIFOrientationBottomLeft = 4;
    static const int ImageEXIFOrientationLeftTop = 5;
    static const int ImageEXIFOrientationRightTop = 6;
    static const int ImageEXIFOrientationRightBottom = 7;
    static const int ImageEXIFOrientationLeftBottom = 8;
#endif

protected:
    enum RepetitionCountStatus {
      Unknown,    // We haven't checked the source's repetition count.
      Uncertain,  // We have a repetition count, but it might be wrong (some GIFs have a count after the image data, and will report "loop once" until all data has been decoded).
      Certain,    // The repetition count is known to be correct.
    };

    BitmapImage(NativeImagePtr, ImageObserver* = 0);
    BitmapImage(ImageObserver* = 0);

#if PLATFORM(WIN)
    virtual void drawFrameMatchingSourceSize(GraphicsContext*, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator);
#endif
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator);
#if PLATFORM(QT) || PLATFORM(WX)
    virtual void drawPattern(GraphicsContext*, const FloatRect& srcRect, const TransformationMatrix& patternTransform,
                             const FloatPoint& phase, CompositeOperator, const FloatRect& destRect);
#endif    
    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount();
    NativeImagePtr frameAtIndex(size_t index, float scaleHint);
    NativeImagePtr frameAtIndex(size_t);
    bool frameIsCompleteAtIndex(size_t);
    float frameDurationAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t); 
    int frameOrientationAtIndex(size_t);

    // Decodes and caches a frame. Never accessed except internally.
    virtual unsigned animatedImageSize();
    virtual void disableImageAnimation();
    bool m_imageAnimationDisabled;
    double m_progressiveLoadChunkTime;
    unsigned m_progressiveLoadChunkCount;
    
    void cacheFrame(size_t index, float scaleHint);

    // Cache frame metadata without decoding image.
    void cacheFrameInfo(size_t index);

    // Called to invalidate cached data.  When |destroyAll| is true, we wipe out
    // the entire frame buffer cache and tell the image source to destroy
    // everything; this is used when e.g. we want to free some room in the image
    // cache.  If |destroyAll| is false, we only delete frames up to the current
    // one; this is used while animating large images to keep memory footprint
    // low without redecoding the whole image on every frame.
    virtual void destroyDecodedData(bool destroyAll = true);

    // If the image is large enough, calls destroyDecodedData() and passes
    // |destroyAll| along.
    void destroyDecodedDataIfNecessary(bool destroyAll);

    // Generally called by destroyDecodedData(), destroys whole-image metadata
    // and notifies observers that the memory footprint has (hopefully)
    // decreased by |framesCleared| times the size (in bytes) of a frame.
    void destroyMetadataAndNotify(int framesCleared);

    // Whether or not size is available yet.    
    bool isSizeAvailable();

    // Animation.
    int repetitionCount(bool imageKnownToBeComplete);  // |imageKnownToBeComplete| should be set if the caller knows the entire image has been decoded.
    bool shouldAnimate();
    virtual void startAnimation(bool catchUpIfNecessary = true);
    void advanceAnimation(Timer<BitmapImage>*);

    // Function that does the real work of advancing the animation.  When
    // skippingFrames is true, we're in the middle of a loop trying to skip over
    // a bunch of animation frames, so we should not do things like decode each
    // one or notify our observers.
    // Returns whether the animation was advanced.
    bool internalAdvanceAnimation(bool skippingFrames);

    // Handle platform-specific data
    void initPlatformData();
    void invalidatePlatformData();
    
    // Checks to see if the image is a 1x1 solid color.  We optimize these images and just do a fill rect instead.
    void checkForSolidColor();
    
    virtual bool mayFillWithSolidColor() const { return m_isSolidColor && m_currentFrame == 0; }
    virtual Color solidColor() const { return m_solidColor; }
    
    ImageSource m_source;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    
    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.
    
    Timer<BitmapImage>* m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.  This will be cAnimationNone if this image type is incapable of animation.
    RepetitionCountStatus m_repetitionCountStatus;
    int m_repetitionsComplete;  // How many repetitions we've finished.
    double m_desiredFrameStartTime;  // The system time at which we hope to see the next call to startAnimation().

#if PLATFORM(MAC)
    mutable RetainPtr<CFDataRef> m_tiffRep; // Cached TIFF rep for frame 0.  Only built lazily if someone queries for one.
#endif

    Color m_solidColor;  // If we're a 1x1 solid color, this is the color to use to fill.
    bool m_isSolidColor;  // Whether or not we are a 1x1 solid image.

    bool m_animationFinished;  // Whether or not we've completed the entire animation.

    bool m_allDataReceived;  // Whether or not we've received all our data.

    mutable bool m_haveSize; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
    mutable bool m_hasUniformFrameSize;

    unsigned m_decodedSize; // The current size of all decoded frames.

    mutable bool m_haveFrameCount;
    size_t m_frameCount;
};

}

#endif
