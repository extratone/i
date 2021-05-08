/*
 * Copyright (C) 2003, 2005 Apple Computer, Inc.  All rights reserved.
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

@class WebCoreTextMarker;
@class WebCoreTextMarkerRange;

@protocol WebCoreViewFactory

- (NSArray *)pluginsInfo; // array of id <WebCorePluginInfo>
- (void)refreshPlugins;

- (NSString *)inputElementAltText;
- (NSString *)resetButtonDefaultLabel;
- (NSString *)searchableIndexIntroduction;
- (NSString *)submitButtonDefaultLabel;
- (NSString *)fileButtonChooseFileLabel;
- (NSString *)fileButtonNoFileSelectedLabel;
- (NSString *)copyImageUnknownFileLabel;


- (NSString *)defaultLanguageCode;

- (NSString *)imageTitleForFilename:(NSString *)filename width:(int)width height:(int)height;



- (NSString *)multipleFileUploadTextForNumberOfFiles:(unsigned)numberOfFiles;
// FTP Directory Related
- (NSString *)unknownFileSizeText;

- (NSString *)htmlSelectMultipleItems:(int)num;
@end

@interface WebCoreViewFactory : NSObject
+ (WebCoreViewFactory *)sharedFactory;
@end

@interface WebCoreViewFactory (SubclassResponsibility) <WebCoreViewFactory>
@end

@protocol WebCorePluginInfo <NSObject>
- (NSString *)name;
- (NSString *)filename;
- (NSString *)pluginDescription;
- (NSEnumerator *)MIMETypeEnumerator;
- (NSString *)descriptionForMIMEType:(NSString *)MIMEType;
- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType;
@end

