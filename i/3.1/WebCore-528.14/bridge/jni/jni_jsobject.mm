/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "jni_jsobject.h"

#if ENABLE(MAC_JAVA_BRIDGE)

#include "Frame.h"
#include "WebCoreFrameView.h"
#include "jni_runtime.h"
#include "jni_utility.h"
#include "ScriptController.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include <interpreter/CallFrame.h>
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <runtime/Completion.h>
#include <runtime/Completion.h>
#include <wtf/Assertions.h>
#include <parser/SourceProvider.h>

using WebCore::Frame;

using namespace JSC::Bindings;
using namespace JSC;

#ifdef NDEBUG
#define JS_LOG(formatAndArgs...) ((void)0)
#else
#define JS_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s(%p,%p):  ", __PRETTY_FUNCTION__, _performJavaScriptRunLoop, CFRunLoopGetCurrent()); \
    fprintf(stderr, formatAndArgs); \
}
#endif

#define UndefinedHandle 1

static CFRunLoopSourceRef _performJavaScriptSource;
static CFRunLoopRef _performJavaScriptRunLoop;

// May only be set by dispatchToJavaScriptThread().
static CFRunLoopSourceRef completionSource;

static void completedJavaScriptAccess (void *i)
{
    assert (CFRunLoopGetCurrent() != _performJavaScriptRunLoop);
    
    JSObjectCallContext *callContext = (JSObjectCallContext *)i;
    CFRunLoopRef runLoop = (CFRunLoopRef)callContext->originatingLoop;
    
    assert (CFRunLoopGetCurrent() == runLoop);
    
    CFRunLoopStop(runLoop);
}

static pthread_once_t javaScriptAccessLockOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t javaScriptAccessLock;
static int javaScriptAccessLockCount = 0;

static void initializeJavaScriptAccessLock()
{
    pthread_mutexattr_t attr;
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    
    pthread_mutex_init(&javaScriptAccessLock, &attr);
}

static inline void lockJavaScriptAccess()
{
    // Perhaps add deadlock detection?
    pthread_once(&javaScriptAccessLockOnce, initializeJavaScriptAccessLock);
    pthread_mutex_lock(&javaScriptAccessLock);
    javaScriptAccessLockCount++;
}

static inline void unlockJavaScriptAccess()
{
    javaScriptAccessLockCount--;
    pthread_mutex_unlock(&javaScriptAccessLock);
}

static void dispatchToJavaScriptThread(JSObjectCallContext *context)
{
    // This lock guarantees that only one thread can invoke
    // at a time, and also guarantees that completionSource;
    // won't get clobbered.
    lockJavaScriptAccess();
    
    CFRunLoopRef currentRunLoop = CFRunLoopGetCurrent();
    
    assert (currentRunLoop != _performJavaScriptRunLoop);
    
    // Setup a source to signal once the invocation of the JavaScript
    // call completes.
    //
    // FIXME:  This could be a potential performance issue.  Creating and
    // adding run loop sources is expensive.  We could create one source 
    // per thread, as needed, instead.
    context->originatingLoop = currentRunLoop;
    CFRunLoopSourceContext sourceContext = {0, context, NULL, NULL, NULL, NULL, NULL, NULL, NULL, completedJavaScriptAccess};
    completionSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(currentRunLoop, completionSource, kCFRunLoopDefaultMode);
    
    // Wakeup JavaScript access thread and make it do it's work.
    CFRunLoopSourceSignal(_performJavaScriptSource);
    if (CFRunLoopIsWaiting(_performJavaScriptRunLoop))
        CFRunLoopWakeUp(_performJavaScriptRunLoop);
    
    // Wait until the JavaScript access thread is done.
    CFRunLoopRun ();
    
    CFRunLoopRemoveSource(currentRunLoop, completionSource, kCFRunLoopDefaultMode);
    CFRelease (completionSource);
    
    unlockJavaScriptAccess();
}

static void performJavaScriptAccess(void*)
{
    assert (CFRunLoopGetCurrent() == _performJavaScriptRunLoop);
    
    // Dispatch JavaScript calls here.
    CFRunLoopSourceContext sourceContext;
    CFRunLoopSourceGetContext (completionSource, &sourceContext);
    JSObjectCallContext *callContext = (JSObjectCallContext *)sourceContext.info;    
    CFRunLoopRef originatingLoop = callContext->originatingLoop;
    
    JavaJSObject::invoke (callContext);
    
    // Signal the originating thread that we're done.
    CFRunLoopSourceSignal (completionSource);
    if (CFRunLoopIsWaiting(originatingLoop))
        CFRunLoopWakeUp(originatingLoop);
}

// Must be called from the thread that will be used to access JavaScript.
void JavaJSObject::initializeJNIThreading() {
    // Should only be called once.
    ASSERT(!_performJavaScriptRunLoop);
    
    // Assume that we can retain this run loop forever.  It'll most 
    // likely (always?) be the main loop.
    _performJavaScriptRunLoop = (CFRunLoopRef)CFRetain(CFRunLoopGetCurrent());
    
    // Setup a source the other threads can use to signal the _runLoop
    // thread that a JavaScript call needs to be invoked.
    CFRunLoopSourceContext sourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, performJavaScriptAccess};
    _performJavaScriptSource = CFRunLoopSourceCreate(NULL, 0, &sourceContext);
    CFRunLoopAddSource(_performJavaScriptRunLoop, _performJavaScriptSource, kCFRunLoopDefaultMode);
}

static bool isJavaScriptThread()
{
    return (_performJavaScriptRunLoop == CFRunLoopGetCurrent());
}

jvalue JavaJSObject::invoke(JSObjectCallContext *context)
{
    jvalue result;

    bzero ((void *)&result, sizeof(jvalue));
    
    if (!isJavaScriptThread()) {        
        // Send the call context to the thread that is allowed to
        // call JavaScript.
        dispatchToJavaScriptThread(context);
        result = context->result;
    }
    else {
        jlong nativeHandle = context->nativeHandle;
        if (nativeHandle == UndefinedHandle || nativeHandle == 0) {
            return result;
        }

        if (context->type == CreateNative) {
            result.j = JavaJSObject::createNative(nativeHandle);
        }
        else {
            JSObject *imp = jlong_to_impptr(nativeHandle);
            if (!findProtectingRootObject(imp)) {
                fprintf (stderr, "%s:%d:  Attempt to access JavaScript from destroyed applet, type %d.\n", __FILE__, __LINE__, context->type);
                return result;
            }

            switch (context->type){            
                case Call: {
                    result.l = JavaJSObject(nativeHandle).call(context->string, context->args);
                    break;
                }
                
                case Eval: {
                    result.l = JavaJSObject(nativeHandle).eval(context->string);
                    break;
                }
            
                case GetMember: {
                    result.l = JavaJSObject(nativeHandle).getMember(context->string);
                    break;
                }
                
                case SetMember: {
                    JavaJSObject(nativeHandle).setMember(context->string, context->value);
                    break;
                }
                
                case RemoveMember: {
                    JavaJSObject(nativeHandle).removeMember(context->string);
                    break;
                }
            
                case GetSlot: {
                    result.l = JavaJSObject(nativeHandle).getSlot(context->index);
                    break;
                }
                
                case SetSlot: {
                    JavaJSObject(nativeHandle).setSlot(context->index, context->value);
                    break;
                }
            
                case ToString: {
                    result.l = (jobject) JavaJSObject(nativeHandle).toString();
                    break;
                }
    
                case Finalize: {
                    JavaJSObject(nativeHandle).finalize();
                    break;
                }
                
                default: {
                    fprintf (stderr, "%s:  invalid JavaScript call\n", __PRETTY_FUNCTION__);
                }
            }
        }
        context->result = result;
    }

    return result;
}


JavaJSObject::JavaJSObject(jlong nativeJSObject)
{
    _imp = jlong_to_impptr(nativeJSObject);
    
    ASSERT(_imp);
    _rootObject = findProtectingRootObject(_imp);
    ASSERT(_rootObject);
}

RootObject* JavaJSObject::rootObject() const
{ 
    return _rootObject && _rootObject->isValid() ? _rootObject.get() : 0; 
}

jobject JavaJSObject::call(jstring methodName, jobjectArray args) const
{
    JS_LOG ("methodName = %s\n", JavaString(methodName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;
    
    // Lookup the function object.
    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock(false);
    
    Identifier identifier(exec, JavaString(methodName));
    JSValuePtr function = _imp->get(exec, identifier);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return 0;

    // Call the function object.
    ArgList argList;
    getListFromJArray(exec, args, argList);
    rootObject->globalObject()->startTimeoutCheck();
    JSValuePtr result = JSC::call(exec, function, callType, callData, _imp, argList);
    rootObject->globalObject()->stopTimeoutCheck();

    return convertValueToJObject(result);
}

jobject JavaJSObject::eval(jstring script) const
{
    JS_LOG ("script = %s\n", JavaString(script).UTF8String());
    
    JSValuePtr result;

    JSLock lock(false);
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    rootObject->globalObject()->startTimeoutCheck();
    Completion completion = JSC::evaluate(rootObject->globalObject()->globalExec(), rootObject->globalObject()->globalScopeChain(), makeSource(JavaString(script)));
    rootObject->globalObject()->stopTimeoutCheck();
    ComplType type = completion.complType();
    
    if (type == Normal) {
        result = completion.value();
        if (!result)
            result = jsUndefined();
    } else
        result = jsUndefined();
    
    return convertValueToJObject (result);
}

jobject JavaJSObject::getMember(jstring memberName) const
{
    JS_LOG ("(%p) memberName = %s\n", _imp, JavaString(memberName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();
    
    JSLock lock(false);
    JSValuePtr result = _imp->get(exec, Identifier(exec, JavaString(memberName)));

    return convertValueToJObject(result);
}

void JavaJSObject::setMember(jstring memberName, jobject value) const
{
    JS_LOG ("memberName = %s, value = %p\n", JavaString(memberName).UTF8String(), value);

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();

    JSLock lock(false);
    PutPropertySlot slot;
    _imp->put(exec, Identifier(exec, JavaString(memberName)), convertJObjectToValue(exec, value), slot);
}


void JavaJSObject::removeMember(jstring memberName) const
{
    JS_LOG ("memberName = %s\n", JavaString(memberName).UTF8String());

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock(false);
    _imp->deleteProperty(exec, Identifier(exec, JavaString(memberName)));
}


jobject JavaJSObject::getSlot(jint index) const
{
#ifdef __LP64__
    JS_LOG ("index = %d\n", index);
#else
    JS_LOG ("index = %ld\n", index);
#endif

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();

    JSLock lock(false);
    JSValuePtr result = _imp->get(exec, index);

    return convertValueToJObject(result);
}


void JavaJSObject::setSlot(jint index, jobject value) const
{
#ifdef __LP64__
    JS_LOG ("index = %d, value = %p\n", index, value);
#else
    JS_LOG ("index = %ld, value = %p\n", index, value);
#endif

    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JSLock lock(false);
    _imp->put(exec, (unsigned)index, convertJObjectToValue(exec, value));
}


jstring JavaJSObject::toString() const
{
    JS_LOG ("\n");
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    JSLock lock(false);
    JSObject *thisObj = const_cast<JSObject*>(_imp);
    ExecState* exec = rootObject->globalObject()->globalExec();
    
    return (jstring)convertValueToJValue (exec, thisObj, object_type, "java.lang.String").l;
}

void JavaJSObject::finalize() const
{
    if (RootObject* rootObject = this->rootObject())
        rootObject->gcUnprotect(_imp);
}

static PassRefPtr<RootObject> createRootObject(void* nativeHandle)
{
    Frame* frame = 0;
    for (NSView *view = (NSView *)nativeHandle; view; view = [view superview]) {
        if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
            NSView<WebCoreFrameView> *webCoreFrameView = static_cast<NSView<WebCoreFrameView>*>(view);
            frame = [webCoreFrameView _web_frame];
            break;
        }
    }
    if (!frame)
        return 0;
    return frame->script()->createRootObject(nativeHandle);
}

// We're either creating a 'Root' object (via a call to JavaJSObject.getWindow()), or
// another JavaJSObject.
jlong JavaJSObject::createNative(jlong nativeHandle)
{
    JS_LOG ("nativeHandle = %d\n", (int)nativeHandle);

    if (nativeHandle == UndefinedHandle)
        return nativeHandle;

    if (findProtectingRootObject(jlong_to_impptr(nativeHandle)))
        return nativeHandle;

    RefPtr<RootObject> rootObject = createRootObject(jlong_to_ptr(nativeHandle));

    // If rootObject is !NULL We must have been called via netscape.javascript.JavaJSObject.getWindow(),
    // otherwise we are being called after creating a JavaJSObject in
    // JavaJSObject::convertValueToJObject().
    if (rootObject) {
        JSObject* globalObject = rootObject->globalObject();
        // We call gcProtect here to get the object into the root object's "protect set" which
        // is used to test if a native handle is valid as well as getting the root object given the handle.
        rootObject->gcProtect(globalObject);
        return ptr_to_jlong(globalObject);
    }
    
    return nativeHandle;
}

jobject JavaJSObject::convertValueToJObject(JSValuePtr value) const
{
    JSLock lock(false);
    
    RootObject* rootObject = this->rootObject();
    if (!rootObject)
        return 0;

    ExecState* exec = rootObject->globalObject()->globalExec();
    JNIEnv *env = getJNIEnv();
    jobject result = 0;
    
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-5.
    // number -> java.lang.Double
    // string -> java.lang.String
    // boolean -> java.lang.Boolean
    // Java instance -> Java instance
    // Everything else -> JavaJSObject
    
    if (value.isNumber()) {
        jclass JSObjectClass = env->FindClass ("java/lang/Double");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(D)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jdouble)value.toNumber(exec));
        }
    } else if (value.isString()) {
        UString stringValue = value.toString(exec);
        JNIEnv *env = getJNIEnv();
        result = env->NewString ((const jchar *)stringValue.data(), stringValue.size());
    } else if (value.isBoolean()) {
        jclass JSObjectClass = env->FindClass ("java/lang/Boolean");
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(Z)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, (jboolean)value.toBoolean(exec));
        }
    }
    else {
        // Create a JavaJSObject.
        jlong nativeHandle;
        
        if (value.isObject()) {
            JSObject* imp = asObject(value);
            
            // We either have a wrapper around a Java instance or a JavaScript
            // object.  If we have a wrapper around a Java instance, return that
            // instance, otherwise create a new Java JavaJSObject with the JSObject*
            // as it's nativeHandle.
            if (imp->classInfo() && strcmp(imp->classInfo()->className, "RuntimeObject") == 0) {
                RuntimeObjectImp* runtimeImp = static_cast<RuntimeObjectImp*>(imp);
                JavaInstance *runtimeInstance = static_cast<JavaInstance *>(runtimeImp->getInternalInstance());
                if (!runtimeInstance)
                    return 0;
                
                return runtimeInstance->javaInstance();
            }
            else {
                nativeHandle = ptr_to_jlong(imp);
                rootObject->gcProtect(imp);
            }
        }
        // All other types will result in an undefined object.
        else {
            nativeHandle = UndefinedHandle;
        }
        
        // Now create the Java JavaJSObject.  Look for the JavaJSObject in it's new (Tiger)
        // location and in the original Java 1.4.2 location.
        jclass JSObjectClass;
        
        JSObjectClass = env->FindClass ("sun/plugin/javascript/webkit/JSObject");
        if (!JSObjectClass) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            JSObjectClass = env->FindClass ("apple/applet/JSObject");
        }
            
        jmethodID constructorID = env->GetMethodID (JSObjectClass, "<init>", "(J)V");
        if (constructorID != NULL) {
            result = env->NewObject (JSObjectClass, constructorID, nativeHandle);
        }
    }
    
    return result;
}

JSValuePtr JavaJSObject::convertJObjectToValue(ExecState* exec, jobject theObject) const
{
    // Instances of netscape.javascript.JSObject get converted back to
    // JavaScript objects.  All other objects are wrapped.  It's not
    // possible to pass primitive types from the Java to JavaScript.
    // See section 22.7 of 'JavaScript:  The Definitive Guide, 4th Edition',
    // figure 22-4.
    jobject classOfInstance = callJNIMethod<jobject>(theObject, "getClass", "()Ljava/lang/Class;");
    jstring className = (jstring)callJNIMethod<jobject>(classOfInstance, "getName", "()Ljava/lang/String;");
    
    // Only the sun.plugin.javascript.webkit.JSObject has a member called nativeJSObject. This class is
    // created above to wrap internal browser objects. The constructor of this class takes the native
    // pointer and stores it in this object, so that it can be retrieved below.
    if (strcmp(JavaString(className).UTF8String(), "sun.plugin.javascript.webkit.JSObject") == 0) {
        // Pull the nativeJSObject value from the Java instance.  This is a
        // pointer to the JSObject.
        JNIEnv *env = getJNIEnv();
        jfieldID fieldID = env->GetFieldID((jclass)classOfInstance, "nativeJSObject", "J");
        if (fieldID == NULL) {
            return jsUndefined();
        }
        jlong nativeHandle = env->GetLongField(theObject, fieldID);
        if (nativeHandle == UndefinedHandle) {
            return jsUndefined();
        }
        JSObject *imp = static_cast<JSObject*>(jlong_to_impptr(nativeHandle));
        return imp;
    }

    JSLock lock(false);

    return JavaInstance::create(theObject, _rootObject)->createRuntimeObject(exec);
}

void JavaJSObject::getListFromJArray(ExecState* exec, jobjectArray jArray, ArgList& list) const
{
    JNIEnv *env = getJNIEnv();
    int numObjects = jArray ? env->GetArrayLength(jArray) : 0;
    
    for (int i = 0; i < numObjects; i++) {
        jobject anObject = env->GetObjectArrayElement ((jobjectArray)jArray, i);
        if (anObject) {
            list.append(convertJObjectToValue(exec, anObject));
            env->DeleteLocalRef (anObject);
        }
        else {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }
}

extern "C" {

jlong KJS_JSCreateNativeJSObject (JNIEnv*, jclass, jstring, jlong nativeHandle, jboolean)
{
    JSObjectCallContext context;
    context.type = CreateNative;
    context.nativeHandle = nativeHandle;
    return JavaJSObject::invoke (&context).j;
}

void KJS_JSObject_JSFinalize (JNIEnv*, jclass, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = Finalize;
    context.nativeHandle = nativeHandle;
    JavaJSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectCall (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring methodName, jobjectArray args, jboolean)
{
    JSObjectCallContext context;
    context.type = Call;
    context.nativeHandle = nativeHandle;
    context.string = methodName;
    context.args = args;
    return JavaJSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectEval (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jscript, jboolean)
{
    JSObjectCallContext context;
    context.type = Eval;
    context.nativeHandle = nativeHandle;
    context.string = jscript;
    return JavaJSObject::invoke (&context).l;
}

jobject KJS_JSObject_JSObjectGetMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jboolean)
{
    JSObjectCallContext context;
    context.type = GetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    return JavaJSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jobject value, jboolean)
{
    JSObjectCallContext context;
    context.type = SetMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    context.value = value;
    JavaJSObject::invoke (&context);
}

void KJS_JSObject_JSObjectRemoveMember (JNIEnv*, jclass, jlong nativeHandle, jstring, jstring jname, jboolean)
{
    JSObjectCallContext context;
    context.type = RemoveMember;
    context.nativeHandle = nativeHandle;
    context.string = jname;
    JavaJSObject::invoke (&context);
}

jobject KJS_JSObject_JSObjectGetSlot (JNIEnv*, jclass, jlong nativeHandle, jstring, jint jindex, jboolean)
{
    JSObjectCallContext context;
    context.type = GetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    return JavaJSObject::invoke (&context).l;
}

void KJS_JSObject_JSObjectSetSlot (JNIEnv*, jclass, jlong nativeHandle, jstring, jint jindex, jobject value, jboolean)
{
    JSObjectCallContext context;
    context.type = SetSlot;
    context.nativeHandle = nativeHandle;
    context.index = jindex;
    context.value = value;
    JavaJSObject::invoke (&context);
}

jstring KJS_JSObject_JSObjectToString (JNIEnv*, jclass, jlong nativeHandle)
{
    JSObjectCallContext context;
    context.type = ToString;
    context.nativeHandle = nativeHandle;
    return (jstring)JavaJSObject::invoke (&context).l;
}

}

#endif // ENABLE(MAC_JAVA_BRIDGE)
