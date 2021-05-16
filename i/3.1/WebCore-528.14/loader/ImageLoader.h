/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef ImageLoader_h
#define ImageLoader_h

#include "AtomicString.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"

namespace WebCore {

class Element;

class ImageLoader : public CachedResourceClient {
public:
    ImageLoader(Element*);
    virtual ~ImageLoader();

    void updateFromElement();

    // This method should be called after the 'src' attribute
    // is set (even when it is not modified) to force the update
    // and match Firefox and Opera.
    void updateFromElementIgnoringPreviousError();

    virtual void dispatchLoadEvent() = 0;
    virtual String sourceURI(const AtomicString&) const = 0;

    Element* element() const { return m_element; }
    bool imageComplete() const { return m_imageComplete; }

    CachedImage* image() const { return m_image.get(); }
    void setImage(CachedImage*);

    void setLoadManually(bool loadManually) { m_loadManually = loadManually; }

    // CachedResourceClient API
    virtual void notifyFinished(CachedResource*);

    virtual bool memoryLimitReached();

    bool haveFiredLoadEvent() const { return m_firedLoad; }
protected:
    void setLoadingImage(CachedImage*);
    
    void setHaveFiredLoadEvent(bool firedLoad) { m_firedLoad = firedLoad; }

private:
    Element* m_element;
    CachedResourceHandle<CachedImage> m_image;
    AtomicString m_failedLoadURL;
    bool m_firedLoad : 1;
    bool m_imageComplete : 1;
    bool m_loadManually : 1;
};

}

#endif
