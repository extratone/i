/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AccessibilityImageMapLink_h
#define AccessibilityImageMapLink_h

#include "AccessibilityObject.h"
#include "HTMLAreaElement.h"
#include "HTMLMapElement.h"

namespace WebCore {
    
class AccessibilityImageMapLink : public AccessibilityObject {
        
private:
    AccessibilityImageMapLink();
public:
    static PassRefPtr<AccessibilityImageMapLink> create();
    virtual ~AccessibilityImageMapLink();
    
    void setHTMLAreaElement(HTMLAreaElement* element) { m_areaElement = element; }
    void setHTMLMapElement(HTMLMapElement* element) { m_mapElement = element; }    
    void setParent(AccessibilityObject* parent) { m_parent = parent; }
        
    virtual AccessibilityRole roleValue() const { return WebCoreLinkRole; }
    virtual bool accessibilityIsIgnored() const { return false; }

    virtual AccessibilityObject* parentObject() const;
    virtual Element* anchorElement() const;
    virtual Element* actionElement() const;
    
    virtual bool isLink() const { return true; } 
    virtual String title() const;
    virtual String accessibilityDescription() const;
    
    virtual IntSize size() const;
    virtual IntRect elementRect() const;

private:    
    HTMLAreaElement* m_areaElement;
    HTMLMapElement* m_mapElement;
    AccessibilityObject* m_parent;
};
    
} // namespace WebCore

#endif // AccessibilityImageMapLink_h
