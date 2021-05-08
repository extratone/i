// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "JavaScriptCore.h"
#include "JSNode.h"
#include "UnusedParam.h"

static char* createStringWithContentsOfFile(const char* fileName);
static JSValueRef print(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

int main(int argc, char* argv[])
{
	const char *scriptPath = "minicom.js";
	if (argc > 1) {
		scriptPath = argv[1];
	}
    
    JSGlobalContextRef context = JSGlobalContextCreate(NULL);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    
    JSStringRef printIString = JSStringCreateWithUTF8CString("print");
    JSObjectSetProperty(context, globalObject, printIString, JSObjectMakeFunctionWithCallback(context, printIString, print), kJSPropertyAttributeNone, NULL);
    JSStringRelease(printIString);
    
    JSStringRef node = JSStringCreateWithUTF8CString("Node");
    JSObjectSetProperty(context, globalObject, node, JSObjectMakeConstructor(context, JSNode_class(context), JSNode_construct), kJSPropertyAttributeNone, NULL);
    JSStringRelease(node);
    
    char* scriptUTF8 = createStringWithContentsOfFile(scriptPath);
    JSStringRef script = JSStringCreateWithUTF8CString(scriptUTF8);
    JSValueRef exception;
    JSValueRef result = JSEvaluateScript(context, script, NULL, NULL, 0, &exception);
    if (result)
        printf("PASS: Test script executed successfully.\n");
    else {
        printf("FAIL: Test script threw exception:\n");
        JSStringRef exceptionIString = JSValueToStringCopy(context, exception, NULL);
        CFStringRef exceptionCF = JSStringCopyCFString(kCFAllocatorDefault, exceptionIString);
        CFShow(exceptionCF);
        CFRelease(exceptionCF);
        JSStringRelease(exceptionIString);
    }
    JSStringRelease(script);
    free(scriptUTF8);

    globalObject = 0;
    JSGlobalContextRelease(context);
    JSGarbageCollect(context);
    printf("PASS: Program exited normally.\n");
    return 0;
}

static JSValueRef print(JSContextRef context, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount > 0) {
        JSStringRef string = JSValueToStringCopy(context, arguments[0], NULL);
        size_t numChars = JSStringGetMaximumUTF8CStringSize(string);
        char stringUTF8[numChars];
        JSStringGetUTF8CString(string, stringUTF8, numChars);
        printf("%s\n", stringUTF8);
    }
    
    return JSValueMakeUndefined(context);
}

static char* createStringWithContentsOfFile(const char* fileName)
{
    char* buffer;
    
    int buffer_size = 0;
    int buffer_capacity = 1024;
    buffer = (char*)malloc(buffer_capacity);
    
    FILE* f = fopen(fileName, "r");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName);
        return 0;
    }
    
    while (!feof(f) && !ferror(f)) {
        buffer_size += fread(buffer + buffer_size, 1, buffer_capacity - buffer_size, f);
        if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
            buffer_capacity *= 2;
            buffer = (char*)realloc(buffer, buffer_capacity);
            assert(buffer);
        }
        
        assert(buffer_size < buffer_capacity);
    }
    fclose(f);
    buffer[buffer_size] = '\0';
    
    return buffer;
}
