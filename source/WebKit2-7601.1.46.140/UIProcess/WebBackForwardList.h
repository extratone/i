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

#ifndef WebBackForwardList_h
#define WebBackForwardList_h

#include "APIObject.h"
#include "WebBackForwardListItem.h"
#include "WebPageProxy.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#if USE(CF)
#include <CoreFoundation/CFDictionary.h>
#endif

namespace WebKit {

struct BackForwardListState;

class WebBackForwardList : public API::ObjectImpl<API::Object::Type::BackForwardList> {
public:
    static Ref<WebBackForwardList> create(WebPageProxy& page)
    {
        return adoptRef(*new WebBackForwardList(page));
    }
    void pageClosed();

    virtual ~WebBackForwardList();

    void addItem(WebBackForwardListItem*);
    void goToItem(WebBackForwardListItem*);
    void removeAllItems();
    void clear();

    WebBackForwardListItem* currentItem() const;
    WebBackForwardListItem* backItem() const;
    WebBackForwardListItem* forwardItem() const;
    WebBackForwardListItem* itemAtIndex(int) const;

    const BackForwardListItemVector& entries() const { return m_entries; }

    uint32_t currentIndex() const { return m_currentIndex; }
    int backListCount() const;
    int forwardListCount() const;

    Ref<API::Array> backList() const;
    Ref<API::Array> forwardList() const;

    Ref<API::Array> backListAsAPIArrayWithLimit(unsigned limit) const;
    Ref<API::Array> forwardListAsAPIArrayWithLimit(unsigned limit) const;

    BackForwardListState backForwardListState(const std::function<bool (WebBackForwardListItem&)>&) const;
    void restoreFromState(BackForwardListState);

    Vector<BackForwardListItemState> itemStates() const;

private:
    explicit WebBackForwardList(WebPageProxy&);

    void didRemoveItem(WebBackForwardListItem&);

    WebPageProxy* m_page;
    BackForwardListItemVector m_entries;
    
    bool m_hasCurrentIndex;
    unsigned m_currentIndex;
    unsigned m_capacity;
};

} // namespace WebKit

#endif // WebBackForwardList_h
