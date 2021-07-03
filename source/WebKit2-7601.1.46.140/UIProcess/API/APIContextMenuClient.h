/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef APIContextMenuClient_h
#define APIContextMenuClient_h

#if ENABLE(CONTEXT_MENUS)

#include "WebHitTestResult.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {
class IntPoint;
}

namespace WebKit {
class WebContextMenuItem;
class WebContextMenuItemData;
class WebPageProxy;
}

namespace API {

class ContextMenuClient {
public:
    virtual ~ContextMenuClient() { }

    virtual bool getContextMenuFromProposedMenu(WebKit::WebPageProxy&, const Vector<RefPtr<WebKit::WebContextMenuItem>>& /* proposedMenu */, Vector<RefPtr<WebKit::WebContextMenuItem>>& /* customMenu */, const WebKit::WebHitTestResult::Data&, API::Object* /* userData */) { return false; }
    virtual void customContextMenuItemSelected(WebKit::WebPageProxy&, const WebKit::WebContextMenuItemData&) { }
    virtual void contextMenuDismissed(WebKit::WebPageProxy&) { }
    virtual bool showContextMenu(WebKit::WebPageProxy&, const WebCore::IntPoint&, const Vector<RefPtr<WebKit::WebContextMenuItem>>&) { return false; }
    virtual bool hideContextMenu(WebKit::WebPageProxy&) { return false; }
};

} // namespace API

#endif // ENABLE(CONTEXT_MENUS)
#endif // APIContextMenuClient_h
