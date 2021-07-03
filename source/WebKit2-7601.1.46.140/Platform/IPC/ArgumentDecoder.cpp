/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ArgumentDecoder.h"

#include "DataReference.h"
#include <stdio.h>

namespace IPC {

ArgumentDecoder::ArgumentDecoder(const uint8_t* buffer, size_t bufferSize)
{
    initialize(buffer, bufferSize);
}

ArgumentDecoder::ArgumentDecoder(const uint8_t* buffer, size_t bufferSize, Vector<Attachment> attachments)
{
    initialize(buffer, bufferSize);

    m_attachments = WTF::move(attachments);
}

ArgumentDecoder::~ArgumentDecoder()
{
    ASSERT(m_buffer);
    free(m_buffer);
    // FIXME: We need to dispose of the mach ports in cases of failure.
}

static inline uint8_t* roundUpToAlignment(uint8_t* ptr, unsigned alignment)
{
    // Assert that the alignment is a power of 2.
    ASSERT(alignment && !(alignment & (alignment - 1)));

    uintptr_t alignmentMask = alignment - 1;
    return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(ptr) + alignmentMask) & ~alignmentMask);
}

void ArgumentDecoder::initialize(const uint8_t* buffer, size_t bufferSize)
{
    m_buffer = static_cast<uint8_t*>(malloc(bufferSize));

    ASSERT(!(reinterpret_cast<uintptr_t>(m_buffer) % alignof(uint64_t)));

    m_bufferPos = m_buffer;
    m_bufferEnd = m_buffer + bufferSize;
    memcpy(m_buffer, buffer, bufferSize);
}

static inline bool alignedBufferIsLargeEnoughToContain(const uint8_t* alignedPosition, const uint8_t* bufferEnd, size_t size)
{
    return bufferEnd >= alignedPosition && static_cast<size_t>(bufferEnd - alignedPosition) >= size;
}

bool ArgumentDecoder::alignBufferPosition(unsigned alignment, size_t size)
{
    uint8_t* alignedPosition = roundUpToAlignment(m_bufferPos, alignment);
    if (!alignedBufferIsLargeEnoughToContain(alignedPosition, m_bufferEnd, size)) {
        // We've walked off the end of this buffer.
        markInvalid();
        return false;
    }
    
    m_bufferPos = alignedPosition;
    return true;
}

bool ArgumentDecoder::bufferIsLargeEnoughToContain(unsigned alignment, size_t size) const
{
    return alignedBufferIsLargeEnoughToContain(roundUpToAlignment(m_bufferPos, alignment), m_bufferEnd, size);
}

bool ArgumentDecoder::decodeFixedLengthData(uint8_t* data, size_t size, unsigned alignment)
{
    if (!alignBufferPosition(alignment, size))
        return false;

    memcpy(data, m_bufferPos, size);
    m_bufferPos += size;

    return true;
}

bool ArgumentDecoder::decodeVariableLengthByteArray(DataReference& dataReference)
{
    uint64_t size;
    if (!decode(size))
        return false;
    
    if (!alignBufferPosition(1, size))
        return false;

    uint8_t* data = m_bufferPos;
    m_bufferPos += size;

    dataReference = DataReference(data, size);
    return true;
}

template<typename Type>
static void decodeValueFromBuffer(Type& value, uint8_t*& bufferPosition)
{
    memcpy(&value, bufferPosition, sizeof(value));
    bufferPosition += sizeof(Type);
}

bool ArgumentDecoder::decode(bool& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;
    
    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(uint8_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;

    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(uint16_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;

    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(uint32_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;

    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(uint64_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;
    
    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(int32_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;
    
    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(int64_t& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;

    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(float& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;

    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::decode(double& result)
{
    if (!alignBufferPosition(sizeof(result), sizeof(result)))
        return false;
    
    decodeValueFromBuffer(result, m_bufferPos);
    return true;
}

bool ArgumentDecoder::removeAttachment(Attachment& attachment)
{
    if (m_attachments.isEmpty())
        return false;

    attachment = m_attachments.takeLast();
    return true;
}

} // namespace IPC
