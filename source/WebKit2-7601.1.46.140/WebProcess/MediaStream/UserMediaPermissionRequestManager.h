/*
 * Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef UserMediaPermissionRequestManager_h
#define UserMediaPermissionRequestManager_h

#if ENABLE(MEDIA_STREAM)

#include <WebCore/UserMediaRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebPage;

class UserMediaPermissionRequestManager {
public:
    explicit UserMediaPermissionRequestManager(WebPage&);

    void startRequest(WebCore::UserMediaRequest&);
    void cancelRequest(WebCore::UserMediaRequest&);

    void didReceiveUserMediaPermissionDecision(uint64_t userMediaID, bool allowed);

private:
    WebPage& m_page;

    HashMap<uint64_t, RefPtr<WebCore::UserMediaRequest>> m_idToRequestMap;
    HashMap<RefPtr<WebCore::UserMediaRequest>, uint64_t> m_requestToIDMap;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)

#endif // UserMediaPermissionRequestManager_h
