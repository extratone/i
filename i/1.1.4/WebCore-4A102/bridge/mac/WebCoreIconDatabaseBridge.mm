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
#import "WebCoreIconDatabaseBridge.h"

#import "Logging.h"
#import "IconDatabase.h"
#import "Image.h"
#import "PlatformString.h"

using namespace WebCore;

@implementation WebCoreIconDatabaseBridge

+ (WebCoreIconDatabaseBridge *)sharedBridgeInstance;
{
    static WebCoreIconDatabaseBridge *sharedBridgeInstance = nil;
    if (sharedBridgeInstance) 
        return sharedBridgeInstance;
    return sharedBridgeInstance = [[WebCoreIconDatabaseBridge alloc] init];
}

- (BOOL)openSharedDatabaseWithPath:(NSString *)path;
{
    assert(path);
    
    _iconDB = IconDatabase::sharedIconDatabase();
    if (_iconDB) {
        _iconDB->open((String([path stringByStandardizingPath])));
        return _iconDB->isOpen() ? YES : NO;
    }
    return NO;
}

- (void)closeSharedDatabase;
{
    if (_iconDB) {
        _iconDB->close();
        _iconDB = 0;
    }
}

- (BOOL)isOpen;
{
    return _iconDB != 0;
}

- (void)setPrivateBrowsingEnabled:(BOOL)flag;
{
    if (_iconDB)
        _iconDB->setPrivateBrowsingEnabled(flag);
}

- (BOOL)privateBrowsingEnabled;
{
    if (_iconDB)
        return _iconDB->getPrivateBrowsingEnabled();
    return false;
}

- (NSString *)iconURLForPageURL:(NSString *)url;
{
    ASSERT(_iconDB);
    ASSERT(url);
    
    String iconURL = _iconDB->iconURLForPageURL(String(url));
    return (NSString*)iconURL;
}
- (void)retainIconForURL:(NSString *)url;
{
    ASSERT(_iconDB);
    ASSERT(url);
    _iconDB->retainIconForURL(String(url));
}

- (void)releaseIconForURL:(NSString *)url;
{
    ASSERT(_iconDB);
    ASSERT(url);
    _iconDB->releaseIconForURL(String(url));
}

- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL;
{
    ASSERT(_iconDB);
    ASSERT(data);
    ASSERT(iconURL);
    
    _iconDB->setIconDataForIconURL([data bytes], [data length], String(iconURL));
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL;
{
    ASSERT(_iconDB);
    ASSERT(iconURL);
    
    _iconDB->setHaveNoIconForIconURL(String(iconURL));
}

- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)url;
{
    ASSERT(_iconDB);
    ASSERT(iconURL);
    ASSERT(url);
    
    _iconDB->setIconURLForPageURL(String(iconURL), String(url));
}

- (BOOL)_hasIconForIconURL:(NSString *)iconURL;
{
    ASSERT(_iconDB);
    ASSERT(iconURL);
    
    return _iconDB->hasIconForIconURL(String(iconURL));
}

@end
