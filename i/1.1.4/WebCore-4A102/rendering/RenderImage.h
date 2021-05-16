/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com) 
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RENDER_IMAGE_H
#define RENDER_IMAGE_H

#include "CachedImage.h"
#include "HTMLElement.h"
#include "RenderReplaced.h"

namespace WebCore {

class DocLoader;
class HTMLMapElement;

class RenderImage : public RenderReplaced
{
public:
    RenderImage(Node*);
    virtual ~RenderImage();

    virtual const char* renderName() const { return "RenderImage"; }

    virtual bool isImage() const { return true; }
    virtual bool isImageButton() const { return false; }
    
    virtual void paint(PaintInfo&, int tx, int ty);

    virtual void layout();

    virtual void imageChanged(CachedImage*);
    
    // don't even think about making this method virtual!
    HTMLElement* element() const
        { return static_cast<HTMLElement*>(RenderReplaced::element()); }

    // hook to keep RendeObject::m_inline() up to date
    virtual void setStyle(RenderStyle *style);
    void updateAltText();
    
    void setIsAnonymousImage(bool anon) { m_isAnonymousImage = anon; }
    bool isAnonymousImage() { return m_isAnonymousImage; }
    
    void setCachedImage(CachedImage*);
    CachedImage* cachedImage() const { return m_cachedImage; }
    
    Image* image() { return m_cachedImage ? m_cachedImage->image() : nullImage(); }

    virtual bool nodeAtPoint(NodeInfo&, int x, int y, int tx, int ty, HitTestAction);
    
    virtual int calcReplacedWidth() const;
    virtual int calcReplacedHeight() const;

    int calcAspectRatioWidth() const;
    int calcAspectRatioHeight() const;

    virtual void calcMinMaxWidth();

    // Called to set generated content images (e.g., :before/:after generated images).
    void setContentObject(CachedResource*);
    
    bool errorOccurred() const { return m_cachedImage && m_cachedImage->isErrorImage(); }
    
    HTMLMapElement* imageMap();

    void resetAnimation();

private:
    bool isWidthSpecified() const;
    bool isHeightSpecified() const;

    // The image we are rendering.
    CachedImage* m_cachedImage;

    // True if the image is set through the content: property
    bool m_isAnonymousImage;

    // Text to display as long as the image isn't available.
    String m_altText;

    static Image* nullImage();
};

} //namespace

#endif
