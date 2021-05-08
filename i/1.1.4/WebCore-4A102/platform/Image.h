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

#ifndef IMAGE_H_
#define IMAGE_H_

#include "Color.h"
#include "GraphicsTypes.h"
#include "ImageSource.h"
#include "IntSize.h"
#include "FloatSize.h"



namespace WebCore {
    struct FrameData;
}

// This complicated-looking declaration tells the framedata Vector that it can copy without
// having to invoke our copy constructor. This allows us to not have to worry about ref counting
// the native frames.
namespace WTF { 
    template<> class VectorTraits<WebCore::FrameData> : public SimpleClassVectorTraits {};
}

namespace WebCore {

class FloatPoint;
class FloatRect;
class GraphicsContext;
class IntRect;
class IntSize;
class PDFDocumentImage;
class String;

template <typename T> class Timer;

// This class gets notified when an image advances animation frames.
class ImageObserver;

// ================================================
// FrameData Class
// ================================================

struct FrameData {
    FrameData()
      :m_frame(0), m_bytes(0), m_scale(0), m_haveInfo(false), m_duration(0), m_hasAlpha(true) 
    {}

    ~FrameData()
    { 
        clear();
    }

    void clear();

    NativeImagePtr m_frame;
	ssize_t m_bytes;
	float m_scale;
	bool m_haveInfo;
    float m_duration;
    bool m_hasAlpha;
};

// =================================================
// Image Class
// =================================================

class Image : Noncopyable {
    friend class GraphicsContext;
public:
    Image();
    Image(ImageObserver* observer, bool isPDF = false);
    ~Image();
    
    static Image* loadResource(const char *name);
    static bool supportsType(const String& type);

    bool isNull() const;

    IntSize size() const;
    IntRect rect() const;
    int width() const;
    int height() const;

    bool setData(bool allDataReceived);
    bool setNativeData(NativeBytePtr, bool allDataReceived);
    
    Vector<char>& dataBuffer() { return m_data; }

    // It may look unusual that there is no start animation call as public API.  This is because
    // we start and stop animating lazily.  Animation begins whenever someone draws the image.  It will
    // automatically pause once all observers no longer want to render the image anywhere.
    void stopAnimation();
    void resetAnimation();
    
    unsigned decodedSize() const { return m_decodedSize; } 

    // Frame accessors.
    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount();
    NativeImagePtr frameAtIndex(size_t index, float scaleHint);
    NativeImagePtr frameAtIndex(size_t index);
    float frameDurationAtIndex(size_t index);
    bool frameHasAlphaAtIndex(size_t index);

    // Typically the CachedImage that owns us.
    ImageObserver* imageObserver() const { return m_imageObserver; }

    enum TileRule { StretchTile, RoundTile, RepeatTile };
    
#if __APPLE__
    // Accessors for native image formats.
    CGImageRef getCGImageRef();
    CFDataRef getTIFFRepresentation();

    // PDF
    void setIsPDF() { m_isPDF = true; }
#endif
    
    // Called to invalidate all our cached data.  If an image is loading incrementally, we only 
 	// invalidate the last cached frame. 
    void destroyDecodedData(bool incremental = false);     

private:
    void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator);
    void drawTiled(GraphicsContext*, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize,
        CompositeOperator);
    void drawTiled(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, TileRule hRule, TileRule vRule,
        CompositeOperator);

    // Decodes and caches a frame. Never accessed except internally.
    void cacheFrame(size_t index, float scaleHint);

	// Cache frame metadata without decoding image.
    void cacheFrameInfo(size_t index);
    
    // Whether or not size is available yet.    
    bool isSizeAvailable();

    // Animation.
    bool shouldAnimate();
    void startAnimation();
    void advanceAnimation(Timer<Image>* timer);
    
    // Constructor for native data.
    void initNativeData();

    // Destructor for native data.
    void destroyNativeData();

    // Invalidation of native data.
    void invalidateNativeData();

    // Checks to see if the image is a 1x1 solid color.  We optimize these images and just do a fill rect instead.
    void checkForSolidColor();

    // Members
    Vector<char> m_data; // The encoded raw data for the image.
    ImageSource m_source;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    
    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.
    
    // Our animation observer.
    ImageObserver* m_imageObserver;
    Timer<Image>* m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.
    int m_repetitionsComplete;  // How many repetitions we've finished.

#if __APPLE__
    mutable CFDataRef m_tiffRep; // Cached TIFF rep for frame 0.  Only built lazily if someone queries for one.
    PDFDocumentImage* m_PDFDoc;
    bool m_isPDF;
#endif

    Color m_solidColor;  // If we're a 1x1 solid color, this is the color to use to fill.
    bool m_isSolidColor;  // Whether or not we are a 1x1 solid image.

    bool m_animatingImageType;  // Whether or not we're an image type that is capable of animating (GIF).
    bool m_animationFinished;  // Whether or not we've completed the entire animation.

    mutable bool m_haveSize; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
    unsigned m_decodedSize; // The current size of all decoded frames. 
};

}

#endif
