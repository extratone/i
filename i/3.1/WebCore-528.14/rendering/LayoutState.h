/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef LayoutState_h
#define LayoutState_h

#include "IntRect.h"
#include "IntSize.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderArena;
class RenderBox;
class RenderObject;

class LayoutState : Noncopyable {
public:
    LayoutState()
        : m_clipped(false)
        , m_next(0)
#ifndef NDEBUG
        , m_renderer(0)
#endif
    {
    }

    LayoutState(LayoutState*, RenderBox*, const IntSize& offset);
    LayoutState(RenderObject*);

    void destroy(RenderArena*);

    // Overloaded new operator.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

private:
    // The normal operator new is disallowed.
    void* operator new(size_t) throw();

public:
    bool m_clipped;
    IntRect m_clipRect;
    IntSize m_offset;       // x/y offset from container.
    IntSize m_layoutDelta;  // Transient offset from the final position of the object
                            // used to ensure that repaints happen in the correct place.
                            // This is a total delta accumulated from the root.
    LayoutState* m_next;
#ifndef NDEBUG
    RenderObject* m_renderer;
#endif
};

} // namespace WebCore

#endif // LayoutState_h
