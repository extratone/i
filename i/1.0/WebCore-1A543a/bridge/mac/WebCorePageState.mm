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

#import "config.h"
#import "WebCorePageState.h"

#import "Document.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "kjs_window.h"
#import <kjs/SavedBuiltins.h>

using namespace WebCore;
using namespace KJS;

@implementation WebCorePageState

- (id)initWithDocument:(Document *)doc URL:(const KURL &)u windowProperties:(SavedProperties *)wp locationProperties:(SavedProperties *)lp interpreterBuiltins:(SavedBuiltins *)ib pausedTimeouts:(PausedTimeouts *)pt
{
    [super init];

    doc->ref();
    document = doc;
    doc->setInPageCache(YES);
    
    FrameMac *frame = static_cast<FrameMac *>(doc->frame());
    mousePressNode = frame ? frame->mousePressNode() : 0;
    if (mousePressNode)
        mousePressNode->ref();
    
    URL = new KURL(u);
    windowProperties = wp;
    locationProperties = lp;
    interpreterBuiltins = ib;
    pausedTimeouts = pt;

    doc->view()->ref();

    return self;
}

- (PausedTimeouts *)pausedTimeouts
{
    return pausedTimeouts;
}

- (void)clear
{
    if (mousePressNode)
        mousePressNode->deref();        
    mousePressNode = 0;

    delete URL;
    URL = 0;

    JSLock lock;

    delete windowProperties;
    windowProperties = 0;
    delete locationProperties;
    locationProperties = 0;
    delete interpreterBuiltins;
    interpreterBuiltins = 0;

    delete pausedTimeouts;
    pausedTimeouts = 0;

    Collector::collect();
}

- (void)invalidate
{
    // Should only ever invalidate once.
    ASSERT(document);
    ASSERT(document->view());
    ASSERT(!document->inPageCache());

    if (document) {
        FrameView *view = document->view();
        if (view)
            view->deref();
        document->deref();
        document = 0;
    }

    [self clear];
}

- (void)dealloc
{
    // This assertion is nice, but it turns out that in reality, -close is never called on WebCorePageState.  We need to do it in -dealloc
    // until we figure out a better place.
    [self close];
    [super dealloc];
}

- (void)finalize
{
    ASSERT(closed);
    [super finalize];
}

- (void)close
{
    if (closed)
        return;
    if (document) {
        ASSERT(document->inPageCache());
        ASSERT(document->view());

        FrameView *view = document->view();

        FrameMac::clearTimers(view);

        bool detached = document->renderer() == 0;
        document->setInPageCache(NO);
        if (detached) {
            document->detach();
            document->removeAllEventListenersFromAllNodes();
        }
        document->deref();
        document = 0;

        if (view) {
            view->clearPart();
            view->deref();
        }
    }

    [self clear];
    closed = YES;
}

- (Document *)document
{
    return document;
}

- (WebCore::Node *)mousePressNode
{
    return mousePressNode;
}

- (KURL *)URL
{
    return URL;
}

- (SavedProperties *)windowProperties
{
    return windowProperties;
}

- (SavedProperties *)locationProperties
{
    return locationProperties;
}

- (SavedBuiltins *)interpreterBuiltins
{
    return interpreterBuiltins;
}

@end
