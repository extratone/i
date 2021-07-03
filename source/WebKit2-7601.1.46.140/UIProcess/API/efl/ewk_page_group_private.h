/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef ewk_page_group_private_h
#define ewk_page_group_private_h

#include "EflTypedefs.h"
#include "ewk_object_private.h"
#include <WebKit/WKBase.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKURL.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

class EwkSettings;

class EwkPageGroup : public EwkObject {
public:
    EWK_OBJECT_DECLARE(EwkPageGroup)

    static PassRefPtr<EwkPageGroup> findOrCreateWrapper(WKPageGroupRef pageGroupRef);
    static PassRefPtr<EwkPageGroup> create(const char*);

    ~EwkPageGroup();

    WKPageGroupRef wkPageGroup() const { return m_pageGroupRef.get(); }
    EwkSettings* settings() const { return m_settings.get(); }

    void addUserStyleSheet(const String& source, const String& baseURL, Eina_List* whitelist, Eina_List* blacklist, bool mainFrameOnly);
    void removeAllUserStyleSheets();

private:
    explicit EwkPageGroup(WKPageGroupRef pageGroupRef);    

    WKRetainPtr<WKPageGroupRef> m_pageGroupRef;
    std::unique_ptr<EwkSettings> m_settings;
};

#endif // ewk_page_group_private_h
