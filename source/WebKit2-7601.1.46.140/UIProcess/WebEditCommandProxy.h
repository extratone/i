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

#ifndef WebEditCommandProxy_h
#define WebEditCommandProxy_h

#include "APIObject.h"
#include <WebCore/EditAction.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebPageProxy;

class WebEditCommandProxy : public API::ObjectImpl<API::Object::Type::EditCommandProxy> {
public:
    static PassRefPtr<WebEditCommandProxy> create(uint64_t commandID, WebCore::EditAction editAction, WebPageProxy* page)
    {
        return adoptRef(new WebEditCommandProxy(commandID, editAction, page));
    }
    ~WebEditCommandProxy();

    uint64_t commandID() const { return m_commandID; }
    WebCore::EditAction editAction() const { return m_editAction; }

    void invalidate() { m_page = 0; }

    void unapply();
    void reapply();

    static String nameForEditAction(WebCore::EditAction);

private:
    WebEditCommandProxy(uint64_t commandID, WebCore::EditAction, WebPageProxy*);

    uint64_t m_commandID;
    WebCore::EditAction m_editAction;
    WebPageProxy* m_page;
};

} // namespace WebKit

#endif // WebEditCommandProxy_h
