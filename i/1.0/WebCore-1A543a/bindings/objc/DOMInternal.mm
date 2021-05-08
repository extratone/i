/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "DOMInternal.h"

#import "Document.h"
#import "Event.h"
#import "FrameMac.h"
#import "Range.h"
#import "kjs_dom.h"
#import "kjs_proxy.h"
#import "WebScriptObjectPrivate.h"
#import "XPathEvaluator.h"

#import "WebCoreThread.h"
#import "ThreadSafeWrapper.h"

using namespace WebCore;

using KJS::ExecState;
using KJS::Interpreter;
using KJS::JSObject;

using KJS::Bindings::RootObject;

//------------------------------------------------------------------------------------------
// Wrapping khtml implementation objects

NSObject* getDOMWrapper(DOMObjectInternal* impl)
{
    ASSERT_WITH_MESSAGE(!WebThreadIsEnabled() || WebThreadIsLocked(), "DOM wrapper cache accessed without web lock");
    WebThreadContext *threadContext = WebThreadCurrentContext();
    if (!threadContext->DOMWrapperCache)
        return nil;
    return ((HashMap<DOMObjectInternal*, NSObject*>*)threadContext->DOMWrapperCache)->get(impl);
}

void addDOMWrapper(NSObject* wrapper, DOMObjectInternal* impl)
{
    ASSERT_WITH_MESSAGE(!WebThreadIsEnabled() || WebThreadIsLocked(), "DOM wrapper cache accessed without web lock");
    WebThreadContext *threadContext = WebThreadCurrentContext();
    if (!threadContext->DOMWrapperCache)
         threadContext->DOMWrapperCache = new HashMap<DOMObjectInternal*, NSObject*>;
    ((HashMap<DOMObjectInternal*, NSObject*>*)threadContext->DOMWrapperCache)->set(impl, wrapper);
}

void removeDOMWrapper(DOMObjectInternal* impl)
{
    ASSERT_WITH_MESSAGE(!WebThreadIsEnabled() || WebThreadIsLocked(), "DOM wrapper cache accessed without web lock");
    WebThreadContext *threadContext = WebThreadCurrentContext();
    if (!threadContext->DOMWrapperCache)
        return;
    ((HashMap<DOMObjectInternal*, NSObject*>*)threadContext->DOMWrapperCache)->remove(impl);
}

//------------------------------------------------------------------------------------------
// Exceptions

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";
NSString * const DOMEventException = @"DOMEventException";
#if XPATH_SUPPORT
NSString * const DOMXPathException = @"DOMXPathException";
#endif // XPATH_SUPPORT

void raiseDOMException(ExceptionCode ec)
{
    ASSERT(ec);

    NSString *name = ::DOMException;

    int code = ec;
    if (ec >= RangeExceptionOffset && ec <= RangeExceptionMax) {
        name = DOMRangeException;
        code -= RangeExceptionOffset;
    } else if (ec >= EventExceptionOffset && ec <= EventExceptionMax) {
        name = DOMEventException;
        code -= EventExceptionOffset;
#if XPATH_SUPPORT
    } else if (ec >= XPathExceptionOffset && ec <= XPathExceptionMax) {
        name = DOMXPathException;
        code -= XPathExceptionOffset;
#endif // XPATH_SUPPORT
    }

    NSString *reason = [NSString stringWithFormat:@"*** Exception received from DOM API: %d", code];
    NSException *exception = [NSException exceptionWithName:name reason:reason
        userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:code] forKey:name]];
    [exception raise];
}

//------------------------------------------------------------------------------------------
// String/NSString bridging

StringImpl::operator NSString *() const
{
    return [NSString stringWithCharacters:m_data length:m_length];
}

String::String(NSString* str)
{
    if (!str)
        return;

    CFIndex size = CFStringGetLength(reinterpret_cast<CFStringRef>(str));
    if (size == 0)
        m_impl = StringImpl::empty();
    else {
        Vector<UChar, 1024> buffer(size);
        CFStringGetCharacters(reinterpret_cast<CFStringRef>(str), CFRangeMake(0, size), buffer.data());
        m_impl = new StringImpl(buffer.data(), size);
    }
}

String::String(CFStringRef str)
{
    if (!str)
        return;

    CFIndex size = CFStringGetLength(str);
    if (size == 0)
        m_impl = StringImpl::empty();
    else {
        Vector<UChar, 1024> buffer(size);
        CFStringGetCharacters(str, CFRangeMake(0, size), buffer.data());
        m_impl = new StringImpl(buffer.data(), size);
    }
}

//------------------------------------------------------------------------------------------

@implementation WebScriptObject (WebScriptObjectInternal)

// Only called by DOMObject subclass.
- _init
{
    self = [super init];

    if (![self isKindOfClass:[DOMObject class]]) {
        [NSException raise:NSGenericException format:@"+%@: _init is an internal initializer", [self class]];
        return nil;
    }

    _private = [[WebScriptObjectPrivate alloc] init];
    _private->isCreatedByDOMWrapper = YES;
    
    return self;
}

- (void)_initializeScriptDOMNodeImp
{
    assert (_private->isCreatedByDOMWrapper);
    
    if (![self isKindOfClass:[DOMNode class]]) {
        // DOMObject can't map back to a document, and thus an interpreter,
        // so for now only create wrappers for DOMNodes.
        NSLog(@"%s:%d:  We don't know how to create ObjC JS wrappers from DOMObjects yet.", __FILE__, __LINE__);
        return;
    }
    
    // Extract the WebCore::Node from the ObjectiveC wrapper.
    DOMNode *n = (DOMNode *)self;
    Node *nodeImpl = [n _node];

    // Dig up Interpreter and ExecState.
    Frame *frame = nodeImpl->document()->frame();
    Interpreter *interpreter = frame->jScript()->interpreter();
    ExecState *exec = interpreter->globalExec();
    
    // Get (or create) a cached JS object for the DOM node.
    JSObject *scriptImp = static_cast<JSObject *>(toJS(exec, nodeImpl));

    const RootObject *executionContext = Mac(frame)->bindingRootObject();

    [self _initializeWithObjectImp:scriptImp originExecutionContext:executionContext executionContext:executionContext];
}

@end
