/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef JSFunction_h
#define JSFunction_h

#include "InternalFunction.h"
#include "JSVariableObject.h"
#include "SymbolTable.h"
#include "Nodes.h"
#include "JSObject.h"

namespace JSC {

    class FunctionBodyNode;
    class FunctionPrototype;
    class JSActivation;
    class JSGlobalObject;

    class JSFunction : public InternalFunction {
        friend class JIT;
        friend class Interpreter;

        typedef InternalFunction Base;

        JSFunction(PassRefPtr<Structure> structure)
            : InternalFunction(structure)
            , m_scopeChain(NoScopeChain())
        {
        }

    public:
        JSFunction(ExecState*, const Identifier&, FunctionBodyNode*, ScopeChainNode*);
        ~JSFunction();

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValuePtr, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        JSObject* construct(ExecState*, const ArgList&);
        JSValuePtr call(ExecState*, JSValuePtr thisValue, const ArgList&);

        void setScope(const ScopeChain& scopeChain) { m_scopeChain = scopeChain; }
        ScopeChain& scope() { return m_scopeChain; }

        void setBody(FunctionBodyNode* body) { m_body = body; }
        void setBody(PassRefPtr<FunctionBodyNode> body) { m_body = body; }
        FunctionBodyNode* body() const { return m_body.get(); }

        virtual void mark();

        static const ClassInfo info;

        static PassRefPtr<Structure> createStructure(JSValuePtr prototype) 
        { 
            return Structure::create(prototype, TypeInfo(ObjectType, ImplementsHasInstance)); 
        }

    private:
        virtual const ClassInfo* classInfo() const { return &info; }

        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);

        static JSValuePtr argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
        static JSValuePtr callerGetter(ExecState*, const Identifier&, const PropertySlot&);
        static JSValuePtr lengthGetter(ExecState*, const Identifier&, const PropertySlot&);

        RefPtr<FunctionBodyNode> m_body;
        ScopeChain m_scopeChain;
    };

    JSFunction* asFunction(JSValuePtr);

    inline JSFunction* asFunction(JSValuePtr value)
    {
        ASSERT(asObject(value)->inherits(&JSFunction::info));
        return static_cast<JSFunction*>(asObject(value));
    }

} // namespace JSC

#endif // JSFunction_h
