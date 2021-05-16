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
#import "WebCoreSettings.h"

#import "FoundationExtras.h"
#import "Settings.h"
#import "Page.h"
#import "WebCoreFrameBridge.h"

using namespace WebCore;

@implementation WebCoreSettings

- (void)dealloc
{
    [standardFontFamily release];
    [fixedFontFamily release];
    [serifFontFamily release];
    [sansSerifFontFamily release];
    [cursiveFontFamily release];
    [fantasyFontFamily release];
    [defaultTextEncoding release];
    [userStyleSheetLocation release];

    delete settings;
    
    [super dealloc];
}

- (void)finalize
{
    delete settings;
    [super finalize];
}

- (id)init
{
    // A Frame may not have been created yet, so we initialize the AtomicString hash before we try and use it in Settings.
    AtomicString::init();
    settings = new Settings;
    return [super init];
}

- (void)_updateAllViews
{
    Page::setNeedsReapplyStylesForSettingsChange(settings);
}

- (void)setStandardFontFamily:(NSString *)s
{
    if ([standardFontFamily isEqualToString:s])
        return;
    [standardFontFamily release];
    standardFontFamily = [s copy];
    settings->setStdFontName(s);
    [self _updateAllViews];
}

- (NSString *)standardFontFamily
{
    return standardFontFamily;
}

- (void)setFixedFontFamily:(NSString *)s
{
    if ([fixedFontFamily isEqualToString:s])
        return;
    [fixedFontFamily release];
    fixedFontFamily = [s copy];
    settings->setFixedFontName(s);
    [self _updateAllViews];
}

- (NSString *)fixedFontFamily
{
    return fixedFontFamily;
}

- (void)setSerifFontFamily:(NSString *)s
{
    if ([serifFontFamily isEqualToString:s])
        return;
    [serifFontFamily release];
    serifFontFamily = [s copy];
    settings->setSerifFontName(s);
    [self _updateAllViews];
}

- (NSString *)serifFontFamily
{
    return serifFontFamily;
}

- (void)setSansSerifFontFamily:(NSString *)s
{
    if ([sansSerifFontFamily isEqualToString:s])
        return;
    [sansSerifFontFamily release];
    sansSerifFontFamily = [s copy];
    settings->setSansSerifFontName(s);
    [self _updateAllViews];
}

- (NSString *)sansSerifFontFamily
{
    return sansSerifFontFamily;
}

- (void)setCursiveFontFamily:(NSString *)s
{
    if ([cursiveFontFamily isEqualToString:s])
        return;
    [cursiveFontFamily release];
    cursiveFontFamily = [s copy];
    settings->setCursiveFontName(s);
    [self _updateAllViews];
}

- (NSString *)cursiveFontFamily
{
    return cursiveFontFamily;
}

- (void)setFantasyFontFamily:(NSString *)s
{
    if ([fantasyFontFamily isEqualToString:s])
        return;
    [fantasyFontFamily release];
    fantasyFontFamily = [s copy];
    settings->setFantasyFontName(s);
    [self _updateAllViews];
}

- (NSString *)fantasyFontFamily
{
    return fantasyFontFamily;
}

- (void)setMinimumFontSize:(float)size
{
    if (minimumFontSize == size)
        return;
    minimumFontSize = size;
    settings->setMinFontSize((int)rint(size));
    [self _updateAllViews];
}

- (float)minimumFontSize
{
    return minimumFontSize;
}

- (void)setMinimumLogicalFontSize:(float)size
{
    if (minimumLogicalFontSize == size)
        return;
    minimumLogicalFontSize = size;
    settings->setMinLogicalFontSize((int)rint(size));
    [self _updateAllViews];
}

- (float)minimumLogicalFontSize
{
    return minimumLogicalFontSize;
}

- (void)setDefaultFontSize:(float)size
{
    if (defaultFontSize == size)
        return;
    defaultFontSize = size;
    settings->setMediumFontSize((int)rint(size));
    [self _updateAllViews];
}

- (float)defaultFontSize
{
    return defaultFontSize;
}

- (void)setDefaultFixedFontSize:(float)size
{
    if (defaultFixedFontSize == size)
        return;
    defaultFixedFontSize = size;
    settings->setMediumFixedFontSize((int)rint(size));
    [self _updateAllViews];
}

- (float)defaultFixedFontSize
{
    return defaultFixedFontSize;
}

- (void)setJavaEnabled:(BOOL)enabled
{
    JavaEnabled = enabled;
    settings->setIsJavaEnabled(enabled);
}

- (BOOL)JavaEnabled
{
    return JavaEnabled;
}

- (void)setPluginsEnabled:(BOOL)enabled
{
    pluginsEnabled = enabled;
    settings->setArePluginsEnabled(enabled);
}

- (BOOL)pluginsEnabled
{
    return pluginsEnabled;
}

- (void)setJavaScriptEnabled:(BOOL)enabled
{
    if (JavaScriptEnabled == enabled)
        return;
    
    JavaScriptEnabled = enabled;
    settings->setIsJavaScriptEnabled(enabled);
    [self _updateAllViews];
}

- (BOOL)JavaScriptEnabled
{
    return JavaScriptEnabled;
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)enabled
{
    JavaScriptCanOpenWindowsAutomatically = enabled;
    settings->setJavaScriptCanOpenWindowsAutomatically(enabled);
}

- (BOOL)JavaScriptCanOpenWindowsAutomatically
{
    return JavaScriptCanOpenWindowsAutomatically;
}

- (void)setPrivateBrowsingEnabled:(BOOL)enabled
{
    privateBrowsingEnabled = enabled;
    settings->setPrivateBrowsingEnabled(enabled);
}

- (BOOL)privateBrowsingEnabled
{
    return privateBrowsingEnabled;
}

- (void)setWillLoadImagesAutomatically:(BOOL)load
{
    willLoadImagesAutomatically = load;
    settings->setAutoLoadImages(load);
}

- (BOOL)willLoadImagesAutomatically
{
    return willLoadImagesAutomatically;
}

- (void)setUserStyleSheetLocation:(NSString *)s
{
    if ([userStyleSheetLocation isEqualToString:s])
        return;
    [userStyleSheetLocation release];
    userStyleSheetLocation = [s copy];
    settings->setUserStyleSheetLocation(DeprecatedString::fromNSString(s));
    [self _updateAllViews];
}

- (NSString *)userStyleSheetLocation
{
    return userStyleSheetLocation;
}

- (void)setShouldPrintBackgrounds:(BOOL)enabled
{
    shouldPrintBackgrounds = enabled;
    settings->setShouldPrintBackgrounds(enabled);
}

- (BOOL)shouldPrintBackgrounds
{
    return shouldPrintBackgrounds;
}

- (void)setTextAreasAreResizable:(BOOL)resizable
{
    textAreasAreResizable = resizable;
    settings->setTextAreasAreResizable(resizable);
    [self _updateAllViews];
}

- (BOOL)textAreasAreResizable
{
    return textAreasAreResizable;
}

- (void)setDefaultTextEncoding:(NSString *)s
{
    if ([defaultTextEncoding isEqualToString:s])
        return;
    [defaultTextEncoding release];
    defaultTextEncoding = [s copy];
    settings->setEncoding(DeprecatedString::fromNSString(s));
}

- (NSString *)defaultTextEncoding
{
    return defaultTextEncoding;
}

- (Settings *)settings
{
    return settings;
}

@end
