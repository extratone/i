/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef BINDINGS_OBJC_INSTANCE_H_
#define BINDINGS_OBJC_INSTANCE_H_

#include "objc_class.h"
#include "objc_utility.h"

namespace JSC {

namespace Bindings {

class ObjcClass;

class ObjcInstance : public Instance {
public:
    static PassRefPtr<ObjcInstance> create(ObjectStructPtr instance, PassRefPtr<RootObject> rootObject)
    {
        return adoptRef(new ObjcInstance(instance, rootObject));
    }

    static void setGlobalException(NSString*, JSGlobalObject* exceptionEnvironment = 0); // A null exceptionEnvironment means the exception should propogate to any execution environment.

    ~ObjcInstance();
    
    virtual Class *getClass() const;
        
    virtual JSValuePtr valueOf(ExecState*) const;
    virtual JSValuePtr defaultValue(ExecState*, PreferredPrimitiveType) const;
    
    virtual JSValuePtr invokeMethod(ExecState*, const MethodList&, const ArgList&);
    virtual bool supportsInvokeDefaultMethod() const;
    virtual JSValuePtr invokeDefaultMethod(ExecState*, const ArgList&);

    JSValuePtr getValueOfUndefinedField(ExecState*, const Identifier&) const;
    virtual bool setValueOfUndefinedField(ExecState*, const Identifier&, JSValuePtr);

    ObjectStructPtr getObject() const { return _instance.get(); }
    
    JSValuePtr stringValue(ExecState*) const;
    JSValuePtr numberValue(ExecState*) const;
    JSValuePtr booleanValue() const;

protected:
    virtual void virtualBegin();
    virtual void virtualEnd();

private:
    static void moveGlobalExceptionToExecState(ExecState*);

    ObjcInstance(ObjectStructPtr instance, PassRefPtr<RootObject>);
    
    RetainPtr<ObjectStructPtr> _instance;
    mutable ObjcClass *_class;
    ObjectStructPtr _pool;
    int _beginCount;
};

} // namespace Bindings

} // namespace JSC

#endif // BINDINGS_OBJC_INSTANCE_H_
