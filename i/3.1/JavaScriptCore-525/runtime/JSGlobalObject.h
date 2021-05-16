/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObject_h
#define JSGlobalObject_h

#include "JSGlobalData.h"
#include "JSVariableObject.h"
#include "NumberPrototype.h"
#include "StringPrototype.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

namespace JSC {

    class ArrayPrototype;
    class BooleanPrototype;
    class DatePrototype;
    class Debugger;
    class ErrorConstructor;
    class FunctionPrototype;
    class GlobalEvalFunction;
    class NativeErrorConstructor;
    class ProgramCodeBlock;
    class RegExpConstructor;
    class RegExpPrototype;
    class RegisterFile;

    struct ActivationStackNode;
    struct HashTable;

    typedef Vector<ExecState*, 16> ExecStateStack;

    class JSGlobalObject : public JSVariableObject {
    protected:
        using JSVariableObject::JSVariableObjectData;

        struct JSGlobalObjectData : public JSVariableObjectData {
            JSGlobalObjectData()
                : JSVariableObjectData(&symbolTable, 0)
                , registerArraySize(0)
                , globalScopeChain(NoScopeChain())
                , regExpConstructor(0)
                , errorConstructor(0)
                , evalErrorConstructor(0)
                , rangeErrorConstructor(0)
                , referenceErrorConstructor(0)
                , syntaxErrorConstructor(0)
                , typeErrorConstructor(0)
                , URIErrorConstructor(0)
                , evalFunction(0)
                , objectPrototype(0)
                , functionPrototype(0)
                , arrayPrototype(0)
                , booleanPrototype(0)
                , stringPrototype(0)
                , numberPrototype(0)
                , datePrototype(0)
                , regExpPrototype(0)
            {
            }
            
            virtual ~JSGlobalObjectData()
            {
            }

            size_t registerArraySize;

            JSGlobalObject* next;
            JSGlobalObject* prev;

            Debugger* debugger;
            
            ScopeChain globalScopeChain;
            Register globalCallFrame[RegisterFile::CallFrameHeaderSize];

            int recursion;

            RegExpConstructor* regExpConstructor;
            ErrorConstructor* errorConstructor;
            NativeErrorConstructor* evalErrorConstructor;
            NativeErrorConstructor* rangeErrorConstructor;
            NativeErrorConstructor* referenceErrorConstructor;
            NativeErrorConstructor* syntaxErrorConstructor;
            NativeErrorConstructor* typeErrorConstructor;
            NativeErrorConstructor* URIErrorConstructor;

            GlobalEvalFunction* evalFunction;

            ObjectPrototype* objectPrototype;
            FunctionPrototype* functionPrototype;
            ArrayPrototype* arrayPrototype;
            BooleanPrototype* booleanPrototype;
            StringPrototype* stringPrototype;
            NumberPrototype* numberPrototype;
            DatePrototype* datePrototype;
            RegExpPrototype* regExpPrototype;

            RefPtr<Structure> argumentsStructure;
            RefPtr<Structure> arrayStructure;
            RefPtr<Structure> booleanObjectStructure;
            RefPtr<Structure> callbackConstructorStructure;
            RefPtr<Structure> callbackFunctionStructure;
            RefPtr<Structure> callbackObjectStructure;
            RefPtr<Structure> dateStructure;
            RefPtr<Structure> emptyObjectStructure;
            RefPtr<Structure> errorStructure;
            RefPtr<Structure> functionStructure;
            RefPtr<Structure> numberObjectStructure;
            RefPtr<Structure> prototypeFunctionStructure;
            RefPtr<Structure> regExpMatchesArrayStructure;
            RefPtr<Structure> regExpStructure;
            RefPtr<Structure> stringObjectStructure;

            SymbolTable symbolTable;
            unsigned profileGroup;

            RefPtr<JSGlobalData> globalData;

            HashSet<ProgramCodeBlock*> codeBlocks;
        };

    public:
        void* operator new(size_t, JSGlobalData*);

        explicit JSGlobalObject()
            : JSVariableObject(JSGlobalObject::createStructure(jsNull()), new JSGlobalObjectData)
        {
            init(this);
        }

    protected:
        JSGlobalObject(PassRefPtr<Structure> structure, JSGlobalObjectData* data, JSObject* thisValue)
            : JSVariableObject(structure, data)
        {
            init(thisValue);
        }

    public:
        virtual ~JSGlobalObject();

        virtual void mark();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);
        virtual void put(ExecState*, const Identifier&, JSValuePtr, PutPropertySlot&);
        virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValuePtr value, unsigned attributes);

        virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunc);
        virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunc);

        // Linked list of all global objects that use the same JSGlobalData.
        JSGlobalObject*& head() { return d()->globalData->head; }
        JSGlobalObject* next() { return d()->next; }

        // The following accessors return pristine values, even if a script 
        // replaces the global object's associated property.

        RegExpConstructor* regExpConstructor() const { return d()->regExpConstructor; }

        ErrorConstructor* errorConstructor() const { return d()->errorConstructor; }
        NativeErrorConstructor* evalErrorConstructor() const { return d()->evalErrorConstructor; }
        NativeErrorConstructor* rangeErrorConstructor() const { return d()->rangeErrorConstructor; }
        NativeErrorConstructor* referenceErrorConstructor() const { return d()->referenceErrorConstructor; }
        NativeErrorConstructor* syntaxErrorConstructor() const { return d()->syntaxErrorConstructor; }
        NativeErrorConstructor* typeErrorConstructor() const { return d()->typeErrorConstructor; }
        NativeErrorConstructor* URIErrorConstructor() const { return d()->URIErrorConstructor; }

        GlobalEvalFunction* evalFunction() const { return d()->evalFunction; }

        ObjectPrototype* objectPrototype() const { return d()->objectPrototype; }
        FunctionPrototype* functionPrototype() const { return d()->functionPrototype; }
        ArrayPrototype* arrayPrototype() const { return d()->arrayPrototype; }
        BooleanPrototype* booleanPrototype() const { return d()->booleanPrototype; }
        StringPrototype* stringPrototype() const { return d()->stringPrototype; }
        NumberPrototype* numberPrototype() const { return d()->numberPrototype; }
        DatePrototype* datePrototype() const { return d()->datePrototype; }
        RegExpPrototype* regExpPrototype() const { return d()->regExpPrototype; }

        Structure* argumentsStructure() const { return d()->argumentsStructure.get(); }
        Structure* arrayStructure() const { return d()->arrayStructure.get(); }
        Structure* booleanObjectStructure() const { return d()->booleanObjectStructure.get(); }
        Structure* callbackConstructorStructure() const { return d()->callbackConstructorStructure.get(); }
        Structure* callbackFunctionStructure() const { return d()->callbackFunctionStructure.get(); }
        Structure* callbackObjectStructure() const { return d()->callbackObjectStructure.get(); }
        Structure* dateStructure() const { return d()->dateStructure.get(); }
        Structure* emptyObjectStructure() const { return d()->emptyObjectStructure.get(); }
        Structure* errorStructure() const { return d()->errorStructure.get(); }
        Structure* functionStructure() const { return d()->functionStructure.get(); }
        Structure* numberObjectStructure() const { return d()->numberObjectStructure.get(); }
        Structure* prototypeFunctionStructure() const { return d()->prototypeFunctionStructure.get(); }
        Structure* regExpMatchesArrayStructure() const { return d()->regExpMatchesArrayStructure.get(); }
        Structure* regExpStructure() const { return d()->regExpStructure.get(); }
        Structure* stringObjectStructure() const { return d()->stringObjectStructure.get(); }

        void setProfileGroup(unsigned value) { d()->profileGroup = value; }
        unsigned profileGroup() const { return d()->profileGroup; }

        void setTimeoutTime(unsigned timeoutTime);
        void startTimeoutCheck();
        void stopTimeoutCheck();

        Debugger* debugger() const { return d()->debugger; }
        void setDebugger(Debugger* debugger) { d()->debugger = debugger; }
        
        virtual bool supportsProfiling() const { return false; }
        
        int recursion() { return d()->recursion; }
        void incRecursion() { ++d()->recursion; }
        void decRecursion() { --d()->recursion; }
        
        ScopeChain& globalScopeChain() { return d()->globalScopeChain; }

        virtual bool isGlobalObject() const { return true; }

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScriptBeforeTimeout() const { return false; }
        virtual bool shouldInterruptScript() const { return true; }

        virtual bool allowsAccessFrom(const JSGlobalObject*) const { return true; }

        virtual bool isDynamicScope() const;

        HashSet<ProgramCodeBlock*>& codeBlocks() { return d()->codeBlocks; }

        void copyGlobalsFrom(RegisterFile&);
        void copyGlobalsTo(RegisterFile&);
        
        void resetPrototype(JSValuePtr prototype);

        JSGlobalData* globalData() { return d()->globalData.get(); }
        JSGlobalObjectData* d() const { return static_cast<JSGlobalObjectData*>(JSVariableObject::d); }

        static PassRefPtr<Structure> createStructure(JSValuePtr prototype)
        {
            return Structure::create(prototype, TypeInfo(ObjectType));
        }

    protected:
        struct GlobalPropertyInfo {
            GlobalPropertyInfo(const Identifier& i, JSValuePtr v, unsigned a)
                : identifier(i)
                , value(v)
                , attributes(a)
            {
            }

            const Identifier identifier;
            JSValuePtr value;
            unsigned attributes;
        };
        void addStaticGlobals(GlobalPropertyInfo*, int count);

    private:
        // FIXME: Fold reset into init.
        void init(JSObject* thisValue);
        void reset(JSValuePtr prototype);

        void setRegisters(Register* registers, Register* registerArray, size_t count);

        void* operator new(size_t); // can only be allocated with JSGlobalData
    };

    JSGlobalObject* asGlobalObject(JSValuePtr);

    inline JSGlobalObject* asGlobalObject(JSValuePtr value)
    {
        ASSERT(asObject(value)->isGlobalObject());
        return static_cast<JSGlobalObject*>(asObject(value));
    }

    inline void JSGlobalObject::setRegisters(Register* registers, Register* registerArray, size_t count)
    {
        JSVariableObject::setRegisters(registers, registerArray);
        d()->registerArraySize = count;
    }

    inline void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
    {
        size_t oldSize = d()->registerArraySize;
        size_t newSize = oldSize + count;
        Register* registerArray = new Register[newSize];
        if (d()->registerArray)
            memcpy(registerArray + count, d()->registerArray.get(), oldSize * sizeof(Register));
        setRegisters(registerArray + newSize, registerArray, newSize);

        for (int i = 0, index = -static_cast<int>(oldSize) - 1; i < count; ++i, --index) {
            GlobalPropertyInfo& global = globals[i];
            ASSERT(global.attributes & DontDelete);
            SymbolTableEntry newEntry(index, global.attributes);
            symbolTable().add(global.identifier.ustring().rep(), newEntry);
            registerAt(index) = global.value;
        }
    }

    inline bool JSGlobalObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
    {
        if (JSVariableObject::getOwnPropertySlot(exec, propertyName, slot))
            return true;
        return symbolTableGet(propertyName, slot);
    }

    inline bool JSGlobalObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
    {
        if (JSVariableObject::getOwnPropertySlotForWrite(exec, propertyName, slot, slotIsWriteable))
            return true;
        return symbolTableGet(propertyName, slot, slotIsWriteable);
    }

    inline JSGlobalObject* ScopeChainNode::globalObject() const
    {
        const ScopeChainNode* n = this;
        while (n->next)
            n = n->next;
        return asGlobalObject(n->object);
    }

    inline JSValuePtr Structure::prototypeForLookup(ExecState* exec) const
    {
        if (typeInfo().type() == ObjectType)
            return m_prototype;

        if (typeInfo().type() == StringType)
            return exec->lexicalGlobalObject()->stringPrototype();

        ASSERT(typeInfo().type() == NumberType);
        return exec->lexicalGlobalObject()->numberPrototype();
    }

    inline StructureChain* Structure::prototypeChain(ExecState* exec) const
    {
        // We cache our prototype chain so our clients can share it.
        if (!isValid(exec, m_cachedPrototypeChain.get())) {
            JSValuePtr prototype = prototypeForLookup(exec);
            m_cachedPrototypeChain = StructureChain::create(prototype.isNull() ? 0 : asObject(prototype)->structure());
        }
        return m_cachedPrototypeChain.get();
    }

    inline bool Structure::isValid(ExecState* exec, StructureChain* cachedPrototypeChain) const
    {
        if (!cachedPrototypeChain)
            return false;

        JSValuePtr prototype = prototypeForLookup(exec);
        RefPtr<Structure>* cachedStructure = cachedPrototypeChain->head();
        while(*cachedStructure && !prototype.isNull()) {
            if (asObject(prototype)->structure() != *cachedStructure)
                return false;
            ++cachedStructure;
            prototype = asObject(prototype)->prototype();
        }
        return prototype.isNull() && !*cachedStructure;
    }

    inline JSGlobalObject* ExecState::dynamicGlobalObject()
    {
        if (this == lexicalGlobalObject()->globalExec())
            return lexicalGlobalObject();

        // For any ExecState that's not a globalExec, the 
        // dynamic global object must be set since code is running
        ASSERT(globalData().dynamicGlobalObject);
        return globalData().dynamicGlobalObject;
    }

    class DynamicGlobalObjectScope : Noncopyable {
    public:
        DynamicGlobalObjectScope(CallFrame* callFrame, JSGlobalObject* dynamicGlobalObject) 
            : m_dynamicGlobalObjectSlot(callFrame->globalData().dynamicGlobalObject)
            , m_savedDynamicGlobalObject(m_dynamicGlobalObjectSlot)
        {
            m_dynamicGlobalObjectSlot = dynamicGlobalObject;
        }

        ~DynamicGlobalObjectScope()
        {
            m_dynamicGlobalObjectSlot = m_savedDynamicGlobalObject;
        }

    private:
        JSGlobalObject*& m_dynamicGlobalObjectSlot;
        JSGlobalObject* m_savedDynamicGlobalObject;
    };

} // namespace JSC

#endif // JSGlobalObject_h
