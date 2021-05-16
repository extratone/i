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

#import <Foundation/Foundation.h>
#import "WAKView.h"
#import "WAKWindow.h"

// WebCorePageBridge is used to bridge between Page in WebCore and
// WebView in WebKit. It is a two-way bridge, with subclasses expected
// to implement a protocol of bridging methods.

#ifdef __cplusplus
namespace WebCore { class Page; }
typedef WebCore::Page WebCorePage;
#else
@class WebCorePage;
#endif

#ifdef __OBJC__
@class WebCoreFrameBridge;
#else
class WebCoreFrameBridge;
#endif

// The WebCorePageBridge interface contains methods for use by the
// non-WebCore side of the bridge.

@interface WebCorePageBridge : NSObject
{
    WebCorePage *_page;
    BOOL _closed;
}
- (void)close;

- (void)setMainFrame:(WebCoreFrameBridge *)mainFrame;

- (WebCoreFrameBridge *)mainFrame;

- (void)setGroupName:(NSString *)groupName;
- (NSString *)groupName;

@end

// The WebCorePageBridge protocol contains methods for use by the WebCore side of the bridge.

@protocol WebCorePageBridge

- (NSView *)outerView;

- (void)setWindowFrame:(NSRect)frame;
- (NSRect)windowFrame;

@end

// This interface definition allows those who hold a WebCorePageBridge * to call all the methods
// in the WebCorePageBridge protocol without requiring the base implementation to supply the methods.
// This idiom is appropriate because WebCorePageBridge is an abstract class.

@interface WebCorePageBridge (SubclassResponsibility) <WebCorePageBridge>
@end

// One method for internal use within WebCore itself.
// Could move this to another header, but would be a pity to create an entire header just for that.

@interface WebCorePageBridge (WebCoreInternalUse)
- (WebCorePage *)impl;
@end
