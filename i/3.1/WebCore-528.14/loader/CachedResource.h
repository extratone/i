/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef CachedResource_h
#define CachedResource_h

#include "CachePolicy.h"
#include "PlatformString.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#include <time.h>

namespace WebCore {

class Cache;
class CachedResourceClient;
class CachedResourceHandleBase;
class DocLoader;
class InspectorResource;
class Request;
class PurgeableBuffer;

// A resource that is held in the cache. Classes who want to use this object should derive
// from CachedResourceClient, to get the function calls in case the requested data has arrived.
// This class also does the actual communication with the loader to obtain the resource from the network.
class CachedResource {
    friend class Cache;
    friend class InspectorResource;
    
public:
    enum Type {
        ImageResource,
        CSSStyleSheet,
        Script,
        FontResource
#if ENABLE(XSLT)
        , XSLStyleSheet
#endif
#if ENABLE(XBL)
        , XBL
#endif
    };

    enum Status {
        NotCached,    // this URL is not cached
        Unknown,      // let cache decide what to do with it
        New,          // inserting new item
        Pending,      // only partially loaded
        Cached        // regular case
    };

    CachedResource(const String& url, Type);
    virtual ~CachedResource();
    
    virtual void load(DocLoader* docLoader)  { load(docLoader, false, false, true); }
    void load(DocLoader*, bool incremental, bool skipCanLoadCheck, bool sendResourceLoadCallbacks);

    virtual void setEncoding(const String&) { }
    virtual String encoding() const { return String(); }
    virtual void data(PassRefPtr<SharedBuffer> data, bool allDataReceived) = 0;
    virtual void error() = 0;

    const String &url() const { return m_url; }
    Type type() const { return m_type; }

    virtual void addClient(CachedResourceClient*);
    void removeClient(CachedResourceClient*);
    bool hasClients() const { return !m_clients.isEmpty(); }
    void deleteIfPossible();

    enum PreloadResult {
        PreloadNotReferenced,
        PreloadReferenced,
        PreloadReferencedWhileLoading,
        PreloadReferencedWhileComplete
    };
    PreloadResult preloadResult() const { return m_preloadResult; }
    void setRequestedFromNetworkingLayer() { m_requestedFromNetworkingLayer = true; }
        
    virtual void allClientsRemoved() { }

    unsigned count() const { return m_clients.size(); }

    Status status() const { return m_status; }

    unsigned size() const { return encodedSize() + decodedSize() + overheadSize(); }
    unsigned encodedSize() const { return m_encodedSize; }
    unsigned decodedSize() const { return m_decodedSize; }
    unsigned overheadSize() const;
    
    bool isLoaded() const { return !m_loading; }
    void setLoading(bool b) { m_loading = b; }

    virtual bool isImage() const { return false; }

    unsigned accessCount() const { return m_accessCount; }
    void increaseAccessCount() { m_accessCount++; }

    // Computes the status of an object after loading.  
    // Updates the expire date on the cache entry file
    void finish();

    // Called by the cache if the object has been removed from the cache
    // while still being referenced. This means the object should delete itself
    // if the number of clients observing it ever drops to 0.
    void setInCache(bool b) { m_inCache = b; }
    bool inCache() const { return m_inCache; }
    
    void setInLiveDecodedResourcesList(bool b) { m_inLiveDecodedResourcesList = b; }
    bool inLiveDecodedResourcesList() { return m_inLiveDecodedResourcesList; }
    
    void setRequest(Request*);

    SharedBuffer* data() const { ASSERT(!m_purgeableData); return m_data.get(); }

    void setResponse(const ResourceResponse&);
    const ResourceResponse& response() const { return m_response; }

    bool canDelete() const { return !hasClients() && !m_request && !m_preloadCount && !m_handleCount && !m_resourceToRevalidate && !m_isBeingRevalidated; }

    bool isExpired() const;

    virtual bool schedule() const { return false; }

    // List of acceptable MIME types seperated by ",".
    // A MIME type may contain a wildcard, e.g. "text/*".
    String accept() const { return m_accept; }
    void setAccept(const String& accept) { m_accept = accept; }

    bool errorOccurred() const { return m_errorOccurred; }
    bool sendResourceLoadCallbacks() const { return m_sendResourceLoadCallbacks; }
    
    virtual void destroyDecodedData() { }

    void setDocLoader(DocLoader* docLoader) { m_docLoader = docLoader; }
    
    bool isPreloaded() const { return m_preloadCount; }
    void increasePreloadCount() { ++m_preloadCount; }
    void decreasePreloadCount() { ASSERT(m_preloadCount); --m_preloadCount; }
    
    void registerHandle(CachedResourceHandleBase* h) { ++m_handleCount; if (m_resourceToRevalidate) m_handlesToRevalidate.add(h); }
    void unregisterHandle(CachedResourceHandleBase* h) { --m_handleCount; if (m_resourceToRevalidate) m_handlesToRevalidate.remove(h); if (!m_handleCount) deleteIfPossible(); }
    
    bool canUseCacheValidator() const;
    bool mustRevalidate(CachePolicy) const;
    bool isCacheValidator() const { return m_resourceToRevalidate; }
    CachedResource* resourceToRevalidate() const { return m_resourceToRevalidate; }
    
    bool isPurgeable() const;
    bool wasPurged() const;
    
protected:
    void setEncodedSize(unsigned);
    void setDecodedSize(unsigned);
    void didAccessDecodedData(double timeStamp);

    bool makePurgeable(bool purgeable);
    bool isSafeToMakePurgeable() const;
    
    DocLoader* docLoader() const { return m_docLoader; }

    HashCountedSet<CachedResourceClient*> m_clients;

    String m_url;
    String m_accept;
    Request* m_request;

    ResourceResponse m_response;
    RefPtr<SharedBuffer> m_data;
    OwnPtr<PurgeableBuffer> m_purgeableData;

    Type m_type;
    Status m_status;

    bool m_errorOccurred;

private:
    // These are called by the friendly Cache only
    void setResourceToRevalidate(CachedResource*);
    void switchClientsToRevalidatedResource();
    void clearResourceToRevalidate();
    void setExpirationDate(time_t expirationDate) { m_expirationDate = expirationDate; }

    unsigned m_encodedSize;
    unsigned m_decodedSize;
    unsigned m_accessCount;
    unsigned m_inLiveDecodedResourcesList;
    double m_lastDecodedAccessTime; // Used as a "thrash guard" in the cache
    
    bool m_sendResourceLoadCallbacks;
    
    unsigned m_preloadCount;
    PreloadResult m_preloadResult;
    bool m_requestedFromNetworkingLayer;

protected:
    bool m_inCache;
    bool m_loading;
    bool m_expireDateChanged;
#ifndef NDEBUG
    bool m_deleted;
    unsigned m_lruIndex;
#endif

private:
    CachedResource* m_nextInAllResourcesList;
    CachedResource* m_prevInAllResourcesList;
    
    CachedResource* m_nextInLiveResourcesList;
    CachedResource* m_prevInLiveResourcesList;

    DocLoader* m_docLoader; // only non-0 for resources that are not in the cache
    
    unsigned m_handleCount;
    // If this field is non-null we are using the resource as a proxy for checking whether an existing resource is still up to date
    // using HTTP If-Modified-Since/If-None-Match headers. If the response is 304 all clients of this resource are moved
    // to to be clients of m_resourceToRevalidate and the resource is deleted. If not, the field is zeroed and this
    // resources becomes normal resource load.
    CachedResource* m_resourceToRevalidate;
    bool m_isBeingRevalidated;
    // These handles will need to be updated to point to the m_resourceToRevalidate in case we get 304 response.
    HashSet<CachedResourceHandleBase*> m_handlesToRevalidate;
    
    time_t m_expirationDate;
};

}

#endif
