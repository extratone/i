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

#ifndef DataReference_h
#define DataReference_h

#include <WebCore/SharedBuffer.h>
#include <wtf/Vector.h>

namespace IPC {

class ArgumentDecoder;
class ArgumentEncoder;
    
class DataReference {
public:
    DataReference()
        : m_data(0)
        , m_size(0)
    {
    }

    DataReference(const uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    template<size_t inlineCapacity>
    DataReference(const Vector<uint8_t, inlineCapacity>& vector)
        : m_data(vector.data())
        , m_size(vector.size())
    {
    }

    bool isEmpty() const { return size() == 0; }

    size_t size() const { return m_size; }
    const uint8_t* data() const 
    { 
        if (isEmpty())
            return 0;
        return m_data; 
    }

    Vector<uint8_t> vector() const
    {
        Vector<uint8_t> result;
        result.append(m_data, m_size);

        return result;
    }

    virtual void encode(ArgumentEncoder&) const;
    static bool decode(ArgumentDecoder&, DataReference&);

    virtual ~DataReference() { }

private:
    const uint8_t* m_data;
    size_t m_size;
};

class SharedBufferDataReference : public DataReference {
public:
    // FIXME: This class doesn't handle null, so the argument should be a reference or PassRef.
    SharedBufferDataReference(WebCore::SharedBuffer* buffer)
        : m_buffer(buffer)
    {
    }

private:
    // FIXME: It is a bad idea to violate the Liskov Substitution Principle as we do here.
    // Since we are using DataReference as a polymoprhic base class in this fashion,
    // then we need it to be a base class that does not have functions such as isEmpty,
    // size, data, and vector, all of which will do the wrong thing if they are called.
    // Deleting these functions here does not prevent them from being called.
    bool isEmpty() const = delete;
    size_t size() const = delete;
    const uint8_t* data() const = delete;
    Vector<uint8_t> vector() const = delete;

    virtual void encode(ArgumentEncoder&) const override;

    RefPtr<WebCore::SharedBuffer> m_buffer;
};

} // namespace IPC

#endif // DataReference_h
