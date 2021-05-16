/*
 *  Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */


#ifndef Protect_h
#define Protect_h

#include "JSCell.h"
#include "Collector.h"

namespace JSC {

    inline void gcProtect(JSCell* val) 
    {
        Heap::heap(val)->protect(val);
    }

    inline void gcUnprotect(JSCell* val)
    {
        Heap::heap(val)->unprotect(val);
    }

    inline void gcProtectNullTolerant(JSCell* val) 
    {
        if (val) 
            gcProtect(val);
    }

    inline void gcUnprotectNullTolerant(JSCell* val) 
    {
        if (val) 
            gcUnprotect(val);
    }
    
    inline void gcProtect(JSValuePtr value)
    {
        if (value && value.isCell())
            gcProtect(asCell(value));
    }

    inline void gcUnprotect(JSValuePtr value)
    {
        if (value && value.isCell())
            gcUnprotect(asCell(value));
    }

    // FIXME: Share more code with RefPtr template? The only differences are the ref/deref operation
    // and the implicit conversion to raw pointer
    template <class T> class ProtectedPtr {
    public:
        ProtectedPtr() : m_ptr(0) {}
        ProtectedPtr(T* ptr);
        ProtectedPtr(const ProtectedPtr&);
        ~ProtectedPtr();

        template <class U> ProtectedPtr(const ProtectedPtr<U>&);
        
        T* get() const { return m_ptr; }
        operator T*() const { return m_ptr; }
        operator JSValuePtr() const { return JSValuePtr(m_ptr); }
        T* operator->() const { return m_ptr; }
        
        operator bool() const { return m_ptr; }
        bool operator!() const { return !m_ptr; }

        ProtectedPtr& operator=(const ProtectedPtr&);
        ProtectedPtr& operator=(T*);
        
    private:
        T* m_ptr;
    };

    class ProtectedJSValuePtr {
    public:
        ProtectedJSValuePtr() {}
        ProtectedJSValuePtr(JSValuePtr value);
        ProtectedJSValuePtr(const ProtectedJSValuePtr&);
        ~ProtectedJSValuePtr();

        template <class U> ProtectedJSValuePtr(const ProtectedPtr<U>&);
        
        JSValuePtr get() const { return m_value; }
        operator JSValuePtr() const { return m_value; }
        JSValuePtr operator->() const { return m_value; }
        
        operator bool() const { return m_value; }
        bool operator!() const { return !m_value; }

        ProtectedJSValuePtr& operator=(const ProtectedJSValuePtr&);
        ProtectedJSValuePtr& operator=(JSValuePtr);
        
    private:
        JSValuePtr m_value;
    };

    template <class T> inline ProtectedPtr<T>::ProtectedPtr(T* ptr)
        : m_ptr(ptr)
    {
        gcProtectNullTolerant(m_ptr);
    }

    template <class T> inline ProtectedPtr<T>::ProtectedPtr(const ProtectedPtr& o)
        : m_ptr(o.get())
    {
        gcProtectNullTolerant(m_ptr);
    }

    template <class T> inline ProtectedPtr<T>::~ProtectedPtr()
    {
        gcUnprotectNullTolerant(m_ptr);
    }

    template <class T> template <class U> inline ProtectedPtr<T>::ProtectedPtr(const ProtectedPtr<U>& o)
        : m_ptr(o.get())
    {
        gcProtectNullTolerant(m_ptr);
    }

    template <class T> inline ProtectedPtr<T>& ProtectedPtr<T>::operator=(const ProtectedPtr<T>& o) 
    {
        T* optr = o.m_ptr;
        gcProtectNullTolerant(optr);
        gcUnprotectNullTolerant(m_ptr);
        m_ptr = optr;
        return *this;
    }

    template <class T> inline ProtectedPtr<T>& ProtectedPtr<T>::operator=(T* optr)
    {
        gcProtectNullTolerant(optr);
        gcUnprotectNullTolerant(m_ptr);
        m_ptr = optr;
        return *this;
    }

    inline ProtectedJSValuePtr::ProtectedJSValuePtr(JSValuePtr value)
        : m_value(value)
    {
        gcProtect(m_value);
    }

    inline ProtectedJSValuePtr::ProtectedJSValuePtr(const ProtectedJSValuePtr& o)
        : m_value(o.get())
    {
        gcProtect(m_value);
    }

    inline ProtectedJSValuePtr::~ProtectedJSValuePtr()
    {
        gcUnprotect(m_value);
    }

    template <class U> ProtectedJSValuePtr::ProtectedJSValuePtr(const ProtectedPtr<U>& o)
        : m_value(o.get())
    {
        gcProtect(m_value);
    }

    inline ProtectedJSValuePtr& ProtectedJSValuePtr::operator=(const ProtectedJSValuePtr& o) 
    {
        JSValuePtr ovalue = o.m_value;
        gcProtect(ovalue);
        gcUnprotect(m_value);
        m_value = ovalue;
        return *this;
    }

    inline ProtectedJSValuePtr& ProtectedJSValuePtr::operator=(JSValuePtr ovalue)
    {
        gcProtect(ovalue);
        gcUnprotect(m_value);
        m_value = ovalue;
        return *this;
    }

    template <class T> inline bool operator==(const ProtectedPtr<T>& a, const ProtectedPtr<T>& b) { return a.get() == b.get(); }
    template <class T> inline bool operator==(const ProtectedPtr<T>& a, const T* b) { return a.get() == b; }
    template <class T> inline bool operator==(const T* a, const ProtectedPtr<T>& b) { return a == b.get(); }

    template <class T> inline bool operator!=(const ProtectedPtr<T>& a, const ProtectedPtr<T>& b) { return a.get() != b.get(); }
    template <class T> inline bool operator!=(const ProtectedPtr<T>& a, const T* b) { return a.get() != b; }
    template <class T> inline bool operator!=(const T* a, const ProtectedPtr<T>& b) { return a != b.get(); }

    inline bool operator==(const ProtectedJSValuePtr& a, const ProtectedJSValuePtr& b) { return a.get() == b.get(); }
    inline bool operator==(const ProtectedJSValuePtr& a, const JSValuePtr b) { return a.get() == b; }
    template <class T> inline bool operator==(const ProtectedJSValuePtr& a, const ProtectedPtr<T>& b) { return a.get() == JSValuePtr(b.get()); }
    inline bool operator==(const JSValuePtr a, const ProtectedJSValuePtr& b) { return a == b.get(); }
    template <class T> inline bool operator==(const ProtectedPtr<T>& a, const ProtectedJSValuePtr& b) { return JSValuePtr(a.get()) == b.get(); }

    inline bool operator!=(const ProtectedJSValuePtr& a, const ProtectedJSValuePtr& b) { return a.get() != b.get(); }
    inline bool operator!=(const ProtectedJSValuePtr& a, const JSValuePtr b) { return a.get() != b; }
    template <class T> inline bool operator!=(const ProtectedJSValuePtr& a, const ProtectedPtr<T>& b) { return a.get() != JSValuePtr(b.get()); }
    inline bool operator!=(const JSValuePtr a, const ProtectedJSValuePtr& b) { return a != b.get(); }
    template <class T> inline bool operator!=(const ProtectedPtr<T>& a, const ProtectedJSValuePtr& b) { return JSValuePtr(a.get()) != b.get(); }
 
} // namespace JSC

#endif // Protect_h
