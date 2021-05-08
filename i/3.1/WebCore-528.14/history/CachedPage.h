/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CachedPage_h
#define CachedPage_h

#include "CachedFrame.h"

namespace WebCore {
    
    class CachedFrame;
    class CachedFramePlatformData;
    class DOMWindow;
    class Document;
    class DocumentLoader;
    class FrameView;
    class KURL;
    class Node;
    class Page;

class CachedPage : public RefCounted<CachedPage> {
public:
    static PassRefPtr<CachedPage> create(Page*);
    ~CachedPage();

    void restore(Page*);
    void clear();

    Document* document() const { return m_cachedMainFrame.document(); }
    DocumentLoader* documentLoader() const { return m_cachedMainFrame.documentLoader(); }
    FrameView* view() const { return m_cachedMainFrame.view(); }
    Node* mousePressNode() const { return m_cachedMainFrame.mousePressNode(); }
    const KURL& url() const { return m_cachedMainFrame.url(); }
    DOMWindow* domWindow() const { return m_cachedMainFrame.domWindow(); }

    double timeStamp() const { return m_timeStamp; }
    
    CachedFrame* cachedMainFrame() { return &m_cachedMainFrame; }

private:
    CachedPage(Page*);

    double m_timeStamp;
    CachedFrame m_cachedMainFrame;
};

} // namespace WebCore

#endif // CachedPage_h

