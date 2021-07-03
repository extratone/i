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

#ifndef SandboxExtension_h
#define SandboxExtension_h

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SANDBOX_EXTENSIONS)
typedef struct __WKSandboxExtension* WKSandboxExtensionRef;
#endif

namespace IPC {
    class ArgumentEncoder;
    class ArgumentDecoder;
}

namespace WebKit {

class SandboxExtension : public RefCounted<SandboxExtension> {
public:
    enum Type {
        ReadOnly,
        ReadWrite
    };

    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    
    public:
        Handle();
        ~Handle();

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, Handle&);

    private:
        friend class SandboxExtension;
#if ENABLE(SANDBOX_EXTENSIONS)
        mutable WKSandboxExtensionRef m_sandboxExtension;
#endif
    };

    class HandleArray {
        WTF_MAKE_NONCOPYABLE(HandleArray);
        
    public:
        HandleArray();
        ~HandleArray();
        void allocate(size_t);
        Handle& operator[](size_t i);
        const Handle& operator[](size_t i) const;
        size_t size() const;
        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, HandleArray&);
       
    private:
#if ENABLE(SANDBOX_EXTENSIONS)
        std::unique_ptr<Handle[]> m_data;
        size_t m_size;
#else
        Handle m_emptyHandle;
#endif
    };
    
    static RefPtr<SandboxExtension> create(const Handle&);
    static bool createHandle(const String& path, Type type, Handle&);
    static bool createHandleForReadWriteDirectory(const String& path, Handle&); // Will attempt to create the directory.
    static String createHandleForTemporaryFile(const String& prefix, Type type, Handle&);
    ~SandboxExtension();

    bool consume();
    bool revoke();

    bool consumePermanently();
    static bool consumePermanently(const Handle&);

private:
    explicit SandboxExtension(const Handle&);
                     
#if ENABLE(SANDBOX_EXTENSIONS)
    mutable WKSandboxExtensionRef m_sandboxExtension;
    size_t m_useCount;
#endif
};

#if !ENABLE(SANDBOX_EXTENSIONS)
inline SandboxExtension::Handle::Handle() { }
inline SandboxExtension::Handle::~Handle() { }
inline void SandboxExtension::Handle::encode(IPC::ArgumentEncoder&) const { }
inline bool SandboxExtension::Handle::decode(IPC::ArgumentDecoder&, Handle&) { return true; }
inline SandboxExtension::HandleArray::HandleArray() { }
inline SandboxExtension::HandleArray::~HandleArray() { }
inline void SandboxExtension::HandleArray::allocate(size_t) { }
inline size_t SandboxExtension::HandleArray::size() const { return 0; }    
inline const SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t) const { return m_emptyHandle; }
inline SandboxExtension::Handle& SandboxExtension::HandleArray::operator[](size_t) { return m_emptyHandle; }
inline void SandboxExtension::HandleArray::encode(IPC::ArgumentEncoder&) const { }
inline bool SandboxExtension::HandleArray::decode(IPC::ArgumentDecoder&, HandleArray&) { return true; }
inline RefPtr<SandboxExtension> SandboxExtension::create(const Handle&) { return nullptr; }
inline bool SandboxExtension::createHandle(const String&, Type, Handle&) { return true; }
inline bool SandboxExtension::createHandleForReadWriteDirectory(const String&, Handle&) { return true; }
inline String SandboxExtension::createHandleForTemporaryFile(const String& /*prefix*/, Type, Handle&) {return String();}
inline SandboxExtension::~SandboxExtension() { }
inline bool SandboxExtension::revoke() { return true; }
inline bool SandboxExtension::consume() { return true; }
inline bool SandboxExtension::consumePermanently() { return true; }
inline bool SandboxExtension::consumePermanently(const Handle&) { return true; }
inline String stringByResolvingSymlinksInPath(const String& path) { return path; }
#else
String stringByResolvingSymlinksInPath(const String& path);
#endif

} // namespace WebKit


#endif // SandboxExtension_h
