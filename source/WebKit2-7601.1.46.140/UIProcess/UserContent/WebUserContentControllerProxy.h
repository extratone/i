/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebUserContentControllerProxy_h
#define WebUserContentControllerProxy_h

#include "APIObject.h"
#include "MessageReceiver.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace API {
class Array;
class UserContentExtension;
class UserScript;
}

namespace IPC {
class DataReference;
}

namespace WebCore {
class UserScript;
class UserStyleSheet;
}

namespace WebKit {

class WebProcessProxy;
class WebScriptMessageHandler;
struct SecurityOriginData;

class WebUserContentControllerProxy : public API::ObjectImpl<API::Object::Type::UserContentController>, private IPC::MessageReceiver {
public:
    static Ref<WebUserContentControllerProxy> create()
    { 
        return adoptRef(*new WebUserContentControllerProxy);
    } 
    explicit WebUserContentControllerProxy();
    ~WebUserContentControllerProxy();

    uint64_t identifier() const { return m_identifier; }

    void addProcess(WebProcessProxy&);
    void removeProcess(WebProcessProxy&);

    API::Array& userScripts() { return m_userScripts.get(); }
    void addUserScript(API::UserScript&);
    void removeAllUserScripts();

    void addUserStyleSheet(WebCore::UserStyleSheet);
    void removeAllUserStyleSheets();

    // Returns false if there was a name conflict.
    bool addUserScriptMessageHandler(WebScriptMessageHandler*);
    void removeUserMessageHandlerForName(const String&);

#if ENABLE(CONTENT_EXTENSIONS)
    void addUserContentExtension(API::UserContentExtension&);
    void removeUserContentExtension(const String&);
    void removeAllUserContentExtensions();
#endif

private:
    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    void didPostMessage(IPC::Connection&, uint64_t pageID, uint64_t frameID, const SecurityOriginData&, uint64_t messageHandlerID, const IPC::DataReference&);

    uint64_t m_identifier;
    HashSet<WebProcessProxy*> m_processes;    
    Ref<API::Array> m_userScripts;
    Vector<WebCore::UserStyleSheet> m_userStyleSheets;
    HashMap<uint64_t, RefPtr<WebScriptMessageHandler>> m_scriptMessageHandlers;

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, RefPtr<API::UserContentExtension>> m_userContentExtensions;
#endif
};

} // namespace WebKit

#endif // WebUserContentControllerProxy_h
