/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef StorageEvent_h
#define StorageEvent_h

#include "Event.h"

namespace WebCore {

    class DOMWindow;

    class StorageEvent : public Event {
    public:
        static PassRefPtr<StorageEvent> create()
        {
            return adoptRef(new StorageEvent);
        }
        static PassRefPtr<StorageEvent> create(const AtomicString& type, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source)
        {
            return adoptRef(new StorageEvent(type, key, oldValue, newValue, uri, source));
        }

        const String& key() const { return m_key; }
        const String& oldValue() const { return m_oldValue; }
        const String& newValue() const { return m_newValue; }
        const String& uri() const { return m_uri; }
        DOMWindow* source() const { return m_source.get(); }
        
        void initStorageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source);
        
        // Needed once we support init<blank>EventNS
        // void initStorageEventNS(in DOMString namespaceURI, in DOMString typeArg, in boolean canBubbleArg, in boolean cancelableArg, in DOMString keyArg, in DOMString oldValueArg, in DOMString newValueArg, in DOMString uriArg, in Window sourceArg);

        virtual bool isStorageEvent() const { return true; }
        
    private:    
        StorageEvent() { }
        StorageEvent(const AtomicString& type, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source);
        
        String m_key;
        String m_oldValue;
        String m_newValue;
        String m_uri;
        RefPtr<DOMWindow> m_source;
    };

} // namespace WebCore

#endif // StorageEvent_h
