/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
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

#ifndef SVGImage_h
#define SVGImage_h

#if ENABLE(SVG)

#include "Image.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include <wtf/OwnPtr.h>

namespace WebCore {
    
    class SVGDocument;
    class Frame;
    class FrameView;
    class Page;
    class SVGImageChromeClient;
    
    class SVGImage : public Image {
    public:
        static PassRefPtr<SVGImage> create(ImageObserver* observer)
        {
            return adoptRef(new SVGImage(observer));
        }

    private:
        virtual ~SVGImage();

        virtual void setContainerSize(const IntSize&);
        virtual bool usesContainerSize() const;
        virtual bool hasRelativeWidth() const;
        virtual bool hasRelativeHeight() const;

        virtual IntSize size() const;
        
        virtual bool dataChanged(bool allDataReceived);

        // FIXME: SVGImages are underreporting decoded sizes and will be unable
        // to prune because these functions are not implemented yet.
        virtual void destroyDecodedData(bool) { }
        virtual unsigned decodedSize() const { return 0; }

        virtual NativeImagePtr frameAtIndex(size_t) { return 0; }
        
        SVGImage(ImageObserver*);
        virtual void draw(GraphicsContext*, const FloatRect& fromRect, const FloatRect& toRect, CompositeOperator);
        
        virtual NativeImagePtr nativeImageForCurrentFrame();
        
        SVGDocument* m_document;
        OwnPtr<SVGImageChromeClient> m_chromeClient;
        OwnPtr<Page> m_page;
        RefPtr<Frame> m_frame;
        RefPtr<FrameView> m_frameView;
        IntSize m_minSize;
        OwnPtr<ImageBuffer> m_frameCache;
    };
}

#endif // ENABLE(SVG)

#endif
