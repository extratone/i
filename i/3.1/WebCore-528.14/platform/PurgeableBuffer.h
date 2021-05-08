/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef PurgeableBuffer_h
#define PurgeableBuffer_h

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {
    
    class PurgeableBuffer : Noncopyable {
    public:
        static PurgeableBuffer* create(const char* data, size_t);
        static PurgeableBuffer* create(const Vector<char>& v) { return create(v.data(), v.size()); }
        
        ~PurgeableBuffer();

        // Call makePurgeable(false) and check the return value before accessing the data.
        const char* data() const;
        size_t size() const { return m_size; }
        
        enum PurgePriority { PurgeLast, PurgeMiddle, PurgeFirst, PurgeDefault = PurgeMiddle };
        PurgePriority purgePriority() const { return m_purgePriority; }
        void setPurgePriority(PurgePriority);
        
        bool isPurgeable() const { return m_state != NonVolatile; }
        bool wasPurged() const;

        bool makePurgeable(bool purgeable);
        
    private:
        PurgeableBuffer(char* data, size_t);
    
        char* m_data;
        size_t m_size;
        PurgePriority m_purgePriority;

        enum State { NonVolatile, Volatile, Purged };
        mutable State m_state;
    };

#if !PLATFORM(DARWIN) || defined(BUILDING_ON_TIGER) || PLATFORM(QT)
    inline PurgeableBuffer* PurgeableBuffer::create(const char*, size_t) { return 0; }
    inline PurgeableBuffer::~PurgeableBuffer() { }
    inline const char* PurgeableBuffer::data() const { return 0; }
    inline void PurgeableBuffer::setPurgePriority(PurgePriority) { }
    inline bool PurgeableBuffer::wasPurged() const { return false; }
    inline bool PurgeableBuffer::makePurgeable(bool) { return false; }
#endif
    
}

#endif
