/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef StyleImage_h
#define StyleImage_h

#include "IntSize.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSValue;
class Image;
class RenderObject;

typedef void* WrappedImagePtr;

class StyleImage : public RefCounted<StyleImage> {
public:
    virtual ~StyleImage() { }

    bool operator==(const StyleImage& other)
    {
        return data() == other.data();
    }
    
    virtual PassRefPtr<CSSValue> cssValue() = 0;

    virtual bool canRender(float /*multiplier*/) const { return true; }
    virtual bool isLoaded() const { return true; }
    virtual bool errorOccurred() const { return false; }
    virtual IntSize imageSize(const RenderObject*, float multiplier) const = 0;
    virtual bool imageHasRelativeWidth() const = 0;
    virtual bool imageHasRelativeHeight() const = 0;
    virtual bool usesImageContainerSize() const = 0;
    virtual void setImageContainerSize(const IntSize&) = 0;
    virtual void addClient(RenderObject*) = 0;
    virtual void removeClient(RenderObject*) = 0;
    virtual Image* image(RenderObject*, const IntSize&) const = 0;
    virtual WrappedImagePtr data() const = 0;
    virtual bool isCachedImage() const { return false; }
    virtual bool isGeneratedImage() const { return false; }
    
    static  bool imagesEquivalent(StyleImage* image1, StyleImage* image2)
    {
        if (image1 != image2) {
            if (!image1 || !image2)
                return false;
            return *image1 == *image2;
        }
        return true;
    }

protected:
    StyleImage() { }
};

}
#endif
