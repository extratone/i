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

namespace KJS {
    class SavedBuiltins;
    class SavedProperties;
    class PausedTimeouts;
}

namespace WebCore {
    class Document;
    class KURL;
    class Node;
}

@interface WebCorePageState : NSObject
{
    WebCore::Document *document;
    WebCore::Node *mousePressNode;
    WebCore::KURL *URL;
    KJS::SavedProperties *windowProperties;
    KJS::SavedProperties *locationProperties;
    KJS::SavedBuiltins *interpreterBuiltins;
    KJS::PausedTimeouts *pausedTimeouts;
    BOOL closed;
}

- initWithDocument:(WebCore::Document *)doc URL:(const WebCore::KURL &)u windowProperties:(KJS::SavedProperties *)wp locationProperties:(KJS::SavedProperties *)lp interpreterBuiltins:(KJS::SavedBuiltins *)ib pausedTimeouts:(KJS::PausedTimeouts *)pt;

- (WebCore::Document *)document;
- (WebCore::Node *)mousePressNode;
- (WebCore::KURL *)URL;
- (KJS::SavedProperties *)windowProperties;
- (KJS::SavedProperties *)locationProperties;
- (KJS::SavedBuiltins *)interpreterBuiltins;
- (KJS::PausedTimeouts *)pausedTimeouts;
- (void)invalidate;
- (void)close;

@end
