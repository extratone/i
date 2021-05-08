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

#import "config.h"
#import "WebCorePageBridge.h"

#import "FrameMac.h"
#import "Page.h"
#import "WebCoreFrameBridge.h"
#import "Logging.h"

using namespace WebCore;

@implementation WebCorePageBridge

static inline void initializeLogChannel(WTFLogChannel &channel)
{
    channel.state = WTFLogChannelOff;
    NSString *logLevelString = [[NSUserDefaults standardUserDefaults] objectForKey:[NSString stringWithUTF8String:channel.defaultName]];
    if (logLevelString) {
        unsigned logLevel;
        if (![[NSScanner scannerWithString:logLevelString] scanHexInt:&logLevel])
            NSLog(@"unable to parse hex value for %s (%@), logging is off", channel.defaultName, logLevelString);
        if ((logLevel & channel.mask) == channel.mask)
            channel.state = WTFLogChannelOn;
    }
}

static void initializeLoggingChannelsIfNecessary()
{
    static bool haveInitializedLoggingChannels = false;
    if (haveInitializedLoggingChannels)
        return;
    haveInitializedLoggingChannels = true;
    
    initializeLogChannel(LogNotYetImplemented);
    initializeLogChannel(LogFrames);
    initializeLogChannel(LogLoading);
    initializeLogChannel(LogPopupBlocking);
    initializeLogChannel(LogEvents);
    initializeLogChannel(LogEditing);
    initializeLogChannel(LogTextConversion);
    initializeLogChannel(LogIconDatabase);
}

- (id)init
{
    initializeLoggingChannelsIfNecessary();
    self = [super init];
    if (self)
        _page = new Page(self);
    return self;
}

- (void)setMainFrame:(WebCoreFrameBridge *)mainFrame
{
    if (_page)
        _page->setMainFrame(adoptRef([mainFrame impl]));
}

- (void)dealloc
{
    ASSERT(_closed);
    [super dealloc];
}

- (void)finalize
{
    ASSERT(_closed);
    [super finalize];
}

- (void)close
{
    delete _page;
    _page = 0;
    _closed = YES;
}

- (WebCoreFrameBridge *)mainFrame
{
    if (!_page)
        return nil;
    return Mac(_page->mainFrame())->bridge();
}

- (void)setGroupName:(NSString *)groupName
{
    if (_page)
        _page->setGroupName(groupName);
}

- (NSString *)groupName
{
    if (!_page)
        return nil;
    return _page->groupName();
}

@end

@implementation WebCorePageBridge (WebCoreInternalUse)

- (Page*)impl
{
    return _page;
}

@end
