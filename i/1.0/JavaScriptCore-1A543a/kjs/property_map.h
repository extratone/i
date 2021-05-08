// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef KJS_PROPERTY_MAP_H_
#define KJS_PROPERTY_MAP_H_

#include "identifier.h"
#include <wtf/OwnArrayPtr.h>

namespace KJS {

    class PropertyNameArray;
    class JSObject;
    class JSValue;
    
    class SavedProperty;
    
    struct PropertyMapHashTable;
    
/**
* Saved Properties
*/
    class SavedProperties {
    friend class PropertyMap;
    public:
        SavedProperties();
        ~SavedProperties();
        
    private:
        int _count;
        OwnArrayPtr<SavedProperty> _properties;
    };
    
/**
* A hashtable entry for the @ref PropertyMap.
*/
    struct PropertyMapHashTableEntry
    {
        PropertyMapHashTableEntry() : key(0) { }
        UString::Rep *key;
        JSValue *value;
        short attributes;
        short globalGetterSetterFlag;
        int index;
    };

/**
* Javascript Property Map.
*/
    class PropertyMap {
    public:
        PropertyMap();
        ~PropertyMap();

        void clear();
        
        void put(const Identifier &name, JSValue *value, int attributes, bool roCheck = false);
        void remove(const Identifier &name);
        JSValue *get(const Identifier &name) const;
        JSValue *get(const Identifier &name, unsigned &attributes) const;
        JSValue **getLocation(const Identifier &name);

        void mark() const;
        void getEnumerablePropertyNames(PropertyNameArray&) const;
        void getSparseArrayPropertyNames(PropertyNameArray&) const;

        void save(SavedProperties &) const;
        void restore(const SavedProperties &p);

        bool hasGetterSetterProperties() const { return _singleEntry.globalGetterSetterFlag; }
        void setHasGetterSetterProperties(bool f) { _singleEntry.globalGetterSetterFlag = f; }

        bool containsGettersOrSetters() const;
    private:
        static bool keysMatch(const UString::Rep *, const UString::Rep *);
        void expand();
        void rehash();
        void rehash(int newTableSize);
        
        void insert(UString::Rep *, JSValue *value, int attributes, int index);
        
        void checkConsistency();
        
        typedef PropertyMapHashTableEntry Entry;
        typedef PropertyMapHashTable Table;

        Table *_table;
        
        Entry _singleEntry;
    };

inline PropertyMap::PropertyMap() : _table(0)
{
    _singleEntry.globalGetterSetterFlag = 0;
}

} // namespace

#endif // _KJS_PROPERTY_MAP_H_
