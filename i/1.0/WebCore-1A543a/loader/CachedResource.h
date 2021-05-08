/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CachedResource_h
#define CachedResource_h

#include "DeprecatedString.h"
#include "CachePolicy.h"
#include "PlatformString.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <time.h>

#if __OBJC__
@class NSData;
@class NSURLResponse;
#else
class NSData;
class NSURLResponse;
#endif

namespace WebCore
{

class Cache;
    class CachedResourceClient;
    class Request;
    
// A resource that is held in the cache. Classes who want to use this object should derive
// from CachedResourceClient, to get the function calls in case the requested data has arrived.
// This class also does the actual communication with the loader to obtain the resource from the network.
class CachedResource {
    public:
        enum Type {
            ImageResource,
            CSSStyleSheet,
            Script
#ifdef KHTML_XSLT
            , XSLStyleSheet
#endif
#ifndef KHTML_NO_XBL
            , XBL
#endif
        };

        enum Status {
            NotCached,    // this URL is not cached
            Unknown,      // let cache decide what to do with it
            New,          // inserting new item
            Pending,      // only partially loaded
        Cached       // regular case
        };

    CachedResource(const String& URL, Type type, CachePolicy cachePolicy);
        virtual ~CachedResource();

        virtual void setCharset(const DeprecatedString&) { }
        virtual Vector<char>& bufferData(const char* bytes, int addedSize, Request*);
        virtual void data(Vector<char>&, bool allDataReceived) = 0;
        virtual void error() = 0;

        const String &url() const { return m_url; }
        Type type() const { return m_type; }

        virtual void ref(CachedResourceClient*);
    void deref(CachedResourceClient*);
    bool referenced() const { return !m_clients.isEmpty(); }
    virtual void allReferencesRemoved() {}; 

    unsigned count() const { return m_clients.size(); }

        Status status() const { return m_status; }

    unsigned size() const { return encodedSize() + decodedSize(); } 
 	unsigned encodedSize() const { return m_encodedSize; } 
 	virtual unsigned decodedSize() const { return 0; } 
    
        bool isLoaded() const { return !m_loading; }
    void setLoading(bool b) { m_loading = b; }

        virtual bool isImage() const { return false; }

    unsigned accessCount() const { return m_accessCount; }
        void increaseAccessCount() { m_accessCount++; }
    
    unsigned liveAccessCount() const { return m_liveAccessCount; }
    void resetLiveAccessCount() { m_liveAccessCount = 0; }
    void increaseLiveAccessCount() { m_liveAccessCount++; }
    void liveResourceAccessed();
    
    // Computes the status of an object after loading.  
    // Updates the expire date on the cache entry file
        void finish();

    // Called by the cache if the object has been removed from the cache
    // while still being referenced. This means the object should delete itself
    // if the number of clients observing it ever drops to 0.
    void setInCache(bool b) { m_inCache = b; }
    bool inCache() const { return m_inCache; }

        CachePolicy cachePolicy() const { return m_cachePolicy; }

        void setRequest(Request*);

#if __APPLE__
        NSURLResponse* response() const { return m_response; }
        void setResponse(NSURLResponse*);
        NSData* allData() const { return m_allData; }
        void setAllData(NSData*);
#endif

    bool canDelete() const { return !referenced() && !m_request; }

        void setExpireDate(time_t expireDate, bool changeHttpCache);

        bool isExpired() const;

        virtual bool schedule() const { return false; }

        // List of acceptable MIME types seperated by ",".
        // A MIME type may contain a wildcard, e.g. "text/*".
    String accept() const { return m_accept; }
    void setAccept(const String& accept) { m_accept = accept; }
    
        virtual void destroyDecodedData() {}; 

    protected:
        void setEncodedSize(unsigned);

        HashSet<CachedResourceClient*> m_clients;

        String m_url;
    String m_accept;
        Request *m_request;
  
#if __APPLE__
        NSURLResponse *m_response;
        NSData *m_allData;
#endif
        Type m_type;
        Status m_status;
  
    private:
    unsigned m_encodedSize;
    unsigned m_accessCount;
    
    unsigned m_liveAccessCount;
    
    protected:
        time_t m_expireDate;
        CachePolicy m_cachePolicy;
    bool m_inCache;
    bool m_loading;
    bool m_expireDateChanged;
#ifndef NDEBUG
    bool m_deleted;
    unsigned m_lruIndex;
    unsigned m_liveLRUIndex;
#endif

private:
    CachedResource* m_nextInAllResourcesList;
    CachedResource* m_prevInAllResourcesList;
    
    CachedResource* m_nextInLiveResourcesList;
    CachedResource* m_prevInLiveResourcesList;

        friend class Cache;
    };

}

#endif
