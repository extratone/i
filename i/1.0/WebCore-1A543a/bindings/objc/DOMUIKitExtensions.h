//
//  DOMUIKitExtensions.h
//  WebCore
//
//  Copyright (C) 2007, Apple Inc.  All rights reserved.
//


#import <WebCore/DOMCore.h>
#import <WebCore/DOMHTML.h>

@interface DOMNode (UIKitExtensions)
- (NSArray *)borderRadii;
- (NSArray *)boundingBoxes;
@end

@interface DOMHTMLAreaElement (UIKitExtensions)
- (CGRect)boundingBoxWithOwner:(DOMNode *)anOwner;
- (NSArray *)boundingBoxesWithOwner:(DOMNode *)anOwner;
@end

@interface DOMHTMLSelectElement (UIKitExtensions)
- (unsigned)completeLength;
@end

