/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ArgumentDecoder_h
#define ArgumentDecoder_h

#include "ArgumentCoder.h"
#include "Attachment.h"
#include <wtf/Vector.h>

namespace IPC {

class DataReference;
    
class ArgumentDecoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ArgumentDecoder(const uint8_t* buffer, size_t bufferSize);
    virtual ~ArgumentDecoder();

    size_t length() const { return m_bufferEnd - m_buffer; }

    bool isInvalid() const { return m_bufferPos > m_bufferEnd; }
    void markInvalid() { m_bufferPos = m_bufferEnd + 1; }

    bool decodeFixedLengthData(uint8_t*, size_t, unsigned alignment);

    // The data in the data reference here will only be valid for the lifetime of the ArgumentDecoder object.
    bool decodeVariableLengthByteArray(DataReference&);

    bool decode(bool&);
    bool decode(uint8_t&);
    bool decode(uint16_t&);
    bool decode(uint32_t&);
    bool decode(uint64_t&);
    bool decode(int32_t&);
    bool decode(int64_t&);
    bool decode(float&);
    bool decode(double&);

    template<typename T> bool decodeEnum(T& result)
    {
        static_assert(sizeof(T) <= 8, "Enum type T must not be larger than 64 bits!");

        uint64_t value;
        if (!decode(value))
            return false;
        
        result = static_cast<T>(value);
        return true;
    }

    template<typename T>
    bool bufferIsLargeEnoughToContain(size_t numElements) const
    {
        static_assert(std::is_arithmetic<T>::value, "Type T must have a fixed, known encoded size!");

        if (numElements > std::numeric_limits<size_t>::max() / sizeof(T))
            return false;

        return bufferIsLargeEnoughToContain(alignof(T), numElements * sizeof(T));
    }

    // Generic type decode function.
    template<typename T> bool decode(T& t)
    {
        return ArgumentCoder<T>::decode(*this, t);
    }

    bool removeAttachment(Attachment&);

protected:
    ArgumentDecoder(const uint8_t* buffer, size_t bufferSize, Vector<Attachment>);

    void initialize(const uint8_t* buffer, size_t bufferSize);

    bool alignBufferPosition(unsigned alignment, size_t size);
    bool bufferIsLargeEnoughToContain(unsigned alignment, size_t size) const;

private:
    uint8_t* m_buffer;
    uint8_t* m_bufferPos;
    uint8_t* m_bufferEnd;

    Vector<Attachment> m_attachments;
};

} // namespace IPC

#endif // ArgumentDecoder_h
