/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "Lookup.h"

#include "PrototypeFunction.h"

namespace JSC {

void HashTable::createTable(JSGlobalData* globalData) const
{
#if ENABLE(PERFECT_HASH_SIZE)
    ASSERT(!table);
    HashEntry* entries = new HashEntry[hashSizeMask + 1];
    for (int i = 0; i <= hashSizeMask; ++i)
        entries[i].setKey(0);
    for (int i = 0; values[i].key; ++i) {
        UString::Rep* identifier = Identifier::add(globalData, values[i].key).releaseRef();
        int hashIndex = identifier->computedHash() & hashSizeMask;
        ASSERT(!entries[hashIndex].key());
        entries[hashIndex].initialize(identifier, values[i].attributes, values[i].value1, values[i].value2);
    }
    table = entries;
#else
    ASSERT(!table);
    int linkIndex = compactHashSizeMask + 1;
    HashEntry* entries = new HashEntry[compactSize];
    for (int i = 0; i < compactSize; ++i)
        entries[i].setKey(0);
    for (int i = 0; values[i].key; ++i) {
        UString::Rep* identifier = Identifier::add(globalData, values[i].key).releaseRef();
        int hashIndex = identifier->computedHash() & compactHashSizeMask;
        HashEntry* entry = &entries[hashIndex];

        if (entry->key()) {
            while (entry->next()) {
                entry = entry->next();
            }
            ASSERT(linkIndex < compactSize);
            entry->setNext(&entries[linkIndex++]);
            entry = entry->next();
        }

        entry->initialize(identifier, values[i].attributes, values[i].value1, values[i].value2);
    }
    table = entries;
#endif
}

void HashTable::deleteTable() const
{
    if (table) {
#if ENABLE(PERFECT_HASH_SIZE)
        int max = hashSizeMask + 1;
#else
        int max = compactSize;
#endif
        for (int i = 0; i != max; ++i) {
            if (UString::Rep* key = table[i].key())
                key->deref();
        }
        delete [] table;
        table = 0;
    }
}

void setUpStaticFunctionSlot(ExecState* exec, const HashEntry* entry, JSObject* thisObj, const Identifier& propertyName, PropertySlot& slot)
{
    ASSERT(entry->attributes() & Function);
    JSValuePtr* location = thisObj->getDirectLocation(propertyName);

    if (!location) {
        PrototypeFunction* function = new (exec) PrototypeFunction(exec, entry->functionLength(), propertyName, entry->function());
        thisObj->putDirect(propertyName, function, entry->attributes());
        location = thisObj->getDirectLocation(propertyName);
    }

    slot.setValueSlot(thisObj, location, thisObj->offsetForLocation(location));
}

} // namespace JSC
