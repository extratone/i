/*
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

#include "config.h"
#include "ImageSource.h"

#include <CoreGraphics/CGImageSourcePrivate.h>

#include "IntSize.h"

namespace WebCore {

ImageSource::ImageSource()
    : m_decoder(0), m_isSubsampled(false)
{
}

ImageSource::~ImageSource()
{
    clear(); 
}

void ImageSource::clear() 
{ 
    if (m_decoder) {
        CFRelease(m_decoder);
        m_decoder = 0;
    }
}

const CFStringRef kCGImageSourceShouldPreferRGB32 = CFSTR("kCGImageSourceShouldPreferRGB32");

CFDictionaryRef ImageSource::imageSourceOptions() const
{
    static CFDictionaryRef options;
    
    if (!options) {
        const void *keys[2] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32 };
        const void *values[2] = { kCFBooleanTrue, kCFBooleanTrue };
        options = CFDictionaryCreate(NULL, keys, values, 2, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    if (m_isSubsampled) {
        static CFDictionaryRef subsampledOptions;
        if (!subsampledOptions) {
            const void *keys[3] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32, kCGImageSourceSubsampleFactor };
            static int subsampleFactor = 4;
            static CFNumberRef subsampleFactorNum = CFNumberCreate(NULL,  kCFNumberIntType,  &subsampleFactor);
            const void *values[3] = { kCFBooleanTrue, kCFBooleanTrue, subsampleFactorNum };
            subsampledOptions = CFDictionaryCreate(NULL, keys, values, 3, 
                                                   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
        return subsampledOptions;        
    }
    
    return options;
}

bool ImageSource::initialized() const
{
    return m_decoder;
}

void ImageSource::setData(NativeBytePtr data, bool allDataReceived)
{
    if (!m_decoder)
        m_decoder = CGImageSourceCreateIncremental(NULL);
    CGImageSourceUpdateData(m_decoder, data, allDataReceived);
}

bool ImageSource::isSizeAvailable()
{
    bool result = false;
    CGImageSourceStatus imageSourceStatus = CGImageSourceGetStatus(m_decoder);

    // Ragnaros yells: TOO SOON! You have awakened me TOO SOON, Executus!
    if (imageSourceStatus >= kCGImageStatusIncomplete) {
        CFDictionaryRef image0Properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, 0, imageSourceOptions());
        if (image0Properties) {
            CFNumberRef widthNumber = (CFNumberRef)CFDictionaryGetValue(image0Properties, kCGImagePropertyPixelWidth);
            CFNumberRef heightNumber = (CFNumberRef)CFDictionaryGetValue(image0Properties, kCGImagePropertyPixelHeight);
            result = widthNumber && heightNumber;
            CFRelease(image0Properties);
        }
    }
    
    return result;
}

IntSize ImageSource::size() const
{
    IntSize result;
    bool isProgressive = false;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, 0, imageSourceOptions());
    if (properties) {
        int w = 0, h = 0;
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth);
        if (num)
            CFNumberGetValue(num, kCFNumberIntType, &w);
        num = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight);
        if (num)
            CFNumberGetValue(num, kCFNumberIntType, &h);
        result = IntSize(w, h);            
        CFDictionaryRef jfifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyJFIFDictionary);
        if (jfifProperties) {
            CFBooleanRef isProgCFBool = (CFBooleanRef)CFDictionaryGetValue(jfifProperties, kCGImagePropertyJFIFIsProgressive);
            if (isProgCFBool)
                isProgressive = CFBooleanGetValue(isProgCFBool);
            // 5184655: Hang rendering very large progressive JPEG
            // Decoding progressive images hangs for a very long time right now
            // Until this is fixed, don't sub-sample progressive images
            // This will cause them to fail our large image check and they won't be decoded.
            // FIXME: remove once underlying issue is fixed (5191418)
        }
        CFRelease(properties);
    }
    if (!m_isSubsampled && !isProgressive) {
        if (result.width() * result.height() >  2000000) {
            // Image is very large and should be sub-sampled.
            // Set the flag and ask for the size again. If the image can be subsampled, the size will be
            // greatly reduced. 4x sub-sampling will make us support up to 32MP images, which should be plenty.
            // There's no callback from ImageIO when the size is available, so we do the check when we happen
            // to check the size and its non - zero.
            // Note: some clients of this class don't call isSizeAvailable() so we can't rely on that.
            m_isSubsampled = true;
            result = size();
        }
    }    
    return result;
}

int ImageSource::repetitionCount()
{
    int result = cAnimationLoopOnce; // No property means loop once.
        
    // A property with value 0 means loop forever.
    CFDictionaryRef properties = CGImageSourceCopyProperties(m_decoder, imageSourceOptions());
    if (properties) {
        CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary);
        if (gifProperties) {
            CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFLoopCount);
            if (num)
                CFNumberGetValue(num, kCFNumberIntType, &result);
        } else
            result = cAnimationNone; // Turns out we're not a GIF after all, so we don't animate.
        
        CFRelease(properties);
    }
    
    return result;
}

size_t ImageSource::frameCount() const
{
    return m_decoder ? CGImageSourceGetCount(m_decoder) : 0;
}

CGImageRef ImageSource::createFrameAtIndex(size_t index)
{
    return CGImageSourceCreateImageAtIndex(m_decoder, index, imageSourceOptions());
}

void ImageSource::destroyFrameAtIndex(size_t index) 
{ 
    // FIXME: Image I/O has no API for flushing frames from its internal cache.  The best we can do is tell it to create 
    // a new image with NULL options.  This will cause the cache/no-cache flags to mismatch, and it will then drop 
    // its reference to the old decoded image. 
    CGImageRef image = CGImageSourceCreateImageAtIndex(m_decoder, index, NULL); 
    CGImageRelease(image); 
} 

bool ImageSource::frameIsCompleteAtIndex(size_t index) 
{ 
    return CGImageSourceGetStatusAtIndex(m_decoder, index) == kCGImageStatusComplete; 
} 

float ImageSource::frameDurationAtIndex(size_t index)
{
    float duration = 0;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, index, imageSourceOptions());
    if (properties) {
        CFDictionaryRef typeProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary);
        if (typeProperties) {
            CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(typeProperties, kCGImagePropertyGIFDelayTime);
            if (num)
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
        }
        CFRelease(properties);
    }

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See gfxImageFrame::GetTimeout in Gecko or Radar 4051389 for more.
    if (duration <= 0.010)
        return 0.100;
    return duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    // Might be interesting to do this optimization on Mac some day, but for now we're just using this
    // for the Cairo source, since it uses our decoders, and our decoders can answer this question.
    // FIXME: Could return false for JPEG and other non-transparent image formats.
    // FIXME: Could maybe return false for a GIF Frame if we have enough info in the GIF properties dictionary
    // to determine whether or not a transparent color was defined.
    return true;
}


}
