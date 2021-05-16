/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Arguments_h
#define Arguments_h

#include "JSActivation.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Interpreter.h"

namespace JSC {

    struct ArgumentsData : Noncopyable {
        JSActivation* activation;

        unsigned numParameters;
        ptrdiff_t firstParameterIndex;
        unsigned numArguments;

        Register* registers;
        OwnArrayPtr<Register> registerArray;

        Register* extraArguments;
        OwnArrayPtr<bool> deletedArguments;
        Register extraArgumentsFixedBuffer[4];

        JSFunction* callee;
        bool overrodeLength : 1;
        bool overrodeCallee : 1;
    };


    class Arguments : public JSObject {
    public:
        enum NoParametersType { NoParameters };

        Arguments(CallFrame*);
        Arguments(CallFrame*, NoParametersType);
        virtual ~Arguments();

        static const ClassInfo info;

        virtual void mark();

        void fillArgList(ExecState*, ArgList&);

        void copyRegisters();
        bool isTornOff() const { return d->registerArray; }
        void setActivation(JSActivation* activation)
        {
            d->activation = activation;
            d->registers = &activation->registerAt(0);
        }

        static PassRefPtr<Structure> createStructure(JSValuePtr prototype) 
        { 
            return Structure::create(prototype, TypeInfo(ObjectType)); 
        }

    private:
        void getArgumentsData(CallFrame*, JSFunction*&, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        virtual bool getOwnPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValuePtr, PutPropertySlot&);
        virtual void put(ExecState*, unsigned propertyName, JSValuePtr, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
        virtual bool deleteProperty(ExecState*, unsigned propertyName);

        virtual const ClassInfo* classInfo() const { return &info; }

        void init(CallFrame*);

        OwnPtr<ArgumentsData> d;
    };

    Arguments* asArguments(JSValuePtr);

    inline Arguments* asArguments(JSValuePtr value)
    {
        ASSERT(asObject(value)->inherits(&Arguments::info));
        return static_cast<Arguments*>(asObject(value));
    }

    ALWAYS_INLINE void Arguments::getArgumentsData(CallFrame* callFrame, JSFunction*& function, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc)
    {
        function = callFrame->callee();
    
        CodeBlock* codeBlock = &function->body()->generatedBytecode();
        int numParameters = codeBlock->m_numParameters;
        argc = callFrame->argumentCount();

        if (argc <= numParameters)
            argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numParameters + 1; // + 1 to skip "this"
        else
            argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numParameters - argc + 1; // + 1 to skip "this"

        argc -= 1; // - 1 to skip "this"
        firstParameterIndex = -RegisterFile::CallFrameHeaderSize - numParameters + 1; // + 1 to skip "this"
    }

    inline Arguments::Arguments(CallFrame* callFrame)
        : JSObject(callFrame->lexicalGlobalObject()->argumentsStructure())
        , d(new ArgumentsData)
    {
        JSFunction* callee;
        ptrdiff_t firstParameterIndex;
        Register* argv;
        int numArguments;
        getArgumentsData(callFrame, callee, firstParameterIndex, argv, numArguments);

        d->numParameters = callee->body()->parameterCount();
        d->firstParameterIndex = firstParameterIndex;
        d->numArguments = numArguments;

        d->activation = 0;
        d->registers = callFrame->registers();

        Register* extraArguments;
        if (d->numArguments <= d->numParameters)
            extraArguments = 0;
        else {
            unsigned numExtraArguments = d->numArguments - d->numParameters;
            if (numExtraArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(Register))
                extraArguments = new Register[numExtraArguments];
            else
                extraArguments = d->extraArgumentsFixedBuffer;
            for (unsigned i = 0; i < numExtraArguments; ++i)
                extraArguments[i] = argv[d->numParameters + i];
        }

        d->extraArguments = extraArguments;

        d->callee = callee;
        d->overrodeLength = false;
        d->overrodeCallee = false;
    }

    inline Arguments::Arguments(CallFrame* callFrame, NoParametersType)
        : JSObject(callFrame->lexicalGlobalObject()->argumentsStructure())
        , d(new ArgumentsData)
    {
        ASSERT(!callFrame->callee()->body()->parameterCount());

        unsigned numArguments = callFrame->argumentCount() - 1;

        d->numParameters = 0;
        d->numArguments = numArguments;
        d->activation = 0;

        Register* extraArguments;
        if (numArguments > sizeof(d->extraArgumentsFixedBuffer) / sizeof(Register))
            extraArguments = new Register[numArguments];
        else
            extraArguments = d->extraArgumentsFixedBuffer;

        Register* argv = callFrame->registers() - RegisterFile::CallFrameHeaderSize - numArguments - 1;
        for (unsigned i = 0; i < numArguments; ++i)
            extraArguments[i] = argv[i];

        d->extraArguments = extraArguments;

        d->callee = callFrame->callee();
        d->overrodeLength = false;
        d->overrodeCallee = false;
    }

    inline void Arguments::copyRegisters()
    {
        ASSERT(!isTornOff());

        if (!d->numParameters)
            return;

        int registerOffset = d->numParameters + RegisterFile::CallFrameHeaderSize;
        size_t registerArraySize = d->numParameters;

        Register* registerArray = new Register[registerArraySize];
        memcpy(registerArray, d->registers - registerOffset, registerArraySize * sizeof(Register));
        d->registerArray.set(registerArray);
        d->registers = registerArray + registerOffset;
    }

    // This JSActivation function is defined here so it can get at Arguments::setRegisters.
    inline void JSActivation::copyRegisters(Arguments* arguments)
    {
        ASSERT(!d()->registerArray);

        size_t numParametersMinusThis = d()->functionBody->generatedBytecode().m_numParameters - 1;
        size_t numVars = d()->functionBody->generatedBytecode().m_numVars;
        size_t numLocals = numVars + numParametersMinusThis;

        if (!numLocals)
            return;

        int registerOffset = numParametersMinusThis + RegisterFile::CallFrameHeaderSize;
        size_t registerArraySize = numLocals + RegisterFile::CallFrameHeaderSize;

        Register* registerArray = copyRegisterArray(d()->registers - registerOffset, registerArraySize);
        setRegisters(registerArray + registerOffset, registerArray);
        if (arguments && !arguments->isTornOff())
            static_cast<Arguments*>(arguments)->setActivation(this);
    }

} // namespace JSC

#endif // Arguments_h
