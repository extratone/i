/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#if BINDINGS

#ifndef RUNTIME_ROOT_H_
#define RUNTIME_ROOT_H_

#include "interpreter.h"
#include "protect.h"

namespace KJS {

namespace Bindings {

class RootObject;

typedef RootObject *(*FindRootObjectForNativeHandleFunctionPtr)(void *);

extern CFMutableDictionaryRef findReferenceDictionary(JSObject *imp);
extern const RootObject *rootForImp (JSObject *imp);
extern const RootObject *rootForInterpreter (Interpreter *interpreter);
extern void addNativeReference (const RootObject *root, JSObject *imp);
extern void removeNativeReference (JSObject *imp);

class RootObject
{
friend class JavaJSObject;
public:
    RootObject(const void *nativeHandle) : _nativeHandle(nativeHandle), _interpreter(0) {}
    
    void setRootObjectImp(JSObject* i) { _imp = i; }
    
    JSObject *rootObjectImp() const { return _imp.get(); }
    
    void setInterpreter (Interpreter *i);
    Interpreter *interpreter() const { return _interpreter; }

    void removeAllNativeReferences ();


    // Must be called from the thread that will be used to access JavaScript.
    static void setFindRootObjectForNativeHandleFunction(FindRootObjectForNativeHandleFunctionPtr aFunc);
    static FindRootObjectForNativeHandleFunctionPtr findRootObjectForNativeHandleFunction() {
        return _findRootObjectForNativeHandleFunctionPtr;
    }
    
    static CFRunLoopRef runLoop() { return _runLoop; }
    static CFRunLoopSourceRef performJavaScriptSource() { return _performJavaScriptSource; }
    
//TODO A.B. check this removal
//    static void dispatchToJavaScriptThread(JSObjectCallContext *context);
    
    const void *nativeHandle() const { return _nativeHandle; }

private:
    const void *_nativeHandle;
    ProtectedPtr<JSObject> _imp;
    Interpreter *_interpreter;

    static FindRootObjectForNativeHandleFunctionPtr _findRootObjectForNativeHandleFunctionPtr;
    static CFRunLoopRef _runLoop;
    static CFRunLoopSourceRef _performJavaScriptSource;
};

} // namespace Bindings

} // namespace KJS

#endif //BINDINGS

#endif
