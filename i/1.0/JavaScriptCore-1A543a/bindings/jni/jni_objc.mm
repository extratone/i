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

#if BINDINGS_JAVA

#include "config.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/jni_utility.h>
#import <JavaScriptCore/objc_utility.h>

using namespace KJS::Bindings;

@interface NSObject (WebScriptingPrivate)
- (jvalue)webPlugInCallJava:(jobject)object method:(jmethodID)method returnType:(JNIType)returnType arguments:(jvalue*)args;
- (jvalue)webPlugInCallJava:(jobject)object
                   isStatic:(BOOL)isStatic
                 returnType:(JNIType)returnType
                     method:(jmethodID)method
                  arguments:(jvalue*)args
                 callingURL:(NSURL *)url
       exceptionDescription:(NSString **)exceptionString;
@end

bool KJS::Bindings::dispatchJNICall (const void *targetAppletView, jobject obj, bool isStatic, JNIType returnType, jmethodID methodID, jvalue *args, jvalue &result, const char*, JSValue *&exceptionDescription)
{
    id view = (id)targetAppletView;
    
    if ([view respondsToSelector:@selector(webPlugInCallJava:isStatic:returnType:method:arguments:callingURL:exceptionDescription:)]) {
        NSString *_exceptionDescription = 0;

        // Passing nil as the calling URL will cause the Java plugin to use the URL
        // of the page that contains the applet. The execution restrictions 
        // implemented in WebCore will guarantee that only appropriate JavaScript
        // can reference the applet.
        result = [view webPlugInCallJava:obj isStatic:isStatic returnType:returnType method:methodID arguments:args callingURL:nil exceptionDescription:&_exceptionDescription];
        if (_exceptionDescription != 0) {
            exceptionDescription = convertNSStringToString(_exceptionDescription);
        }
        return true;
    }
    else if ([view respondsToSelector:@selector(webPlugInCallJava:method:returnType:arguments:)]) {
        result = [view webPlugInCallJava:obj method:methodID returnType:returnType arguments:args];
        return true;
    }

    bzero (&result, sizeof(jvalue));
    return false;
}

#endif //BINDINGS_JAVA
