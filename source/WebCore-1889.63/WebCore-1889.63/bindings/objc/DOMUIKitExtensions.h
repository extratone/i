//
//  DOMUIKitExtensions.h
//  WebCore
//
//  Copyright (C) 2007, 2008, Apple Inc.  All rights reserved.
//

#if PLATFORM(IOS)

#import <WebCore/DOMElement.h>
#import <WebCore/DOMExtensions.h>
#import <WebCore/DOMHTMLAreaElement.h>
#import <WebCore/DOMHTMLImageElement.h>
#import <WebCore/DOMHTMLSelectElement.h>
#import <WebCore/DOMNode.h>
#import <WebCore/DOMRange.h>

typedef enum { 
    // The first four match SelectionDirection.  The last two don't have WebKit counterparts because
    // they aren't necessary until there is support vertical layout.
    WebTextAdjustmentForward,
    WebTextAdjustmentBackward,
    WebTextAdjustmentRight,
    WebTextAdjustmentLeft,
    WebTextAdjustmentUp,
    WebTextAdjustmentDown
} WebTextAdjustmentDirection; 

@interface DOMRange (UIKitExtensions)

- (void)move:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction;
- (void)extend:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction;
- (DOMNode *)firstNode;

@end

@interface DOMNode (UIKitExtensions)
- (NSArray *)borderRadii;
- (NSArray *)boundingBoxes;
- (NSArray *)absoluteQuads;     // return array of WKQuadObjects. takes transforms into account

- (BOOL)containsOnlyInlineObjects;
- (BOOL)isSelectableBlock;
- (DOMRange *)rangeOfContainingParagraph;
- (CGFloat)textHeight;
- (DOMNode *)findExplodedTextNodeAtPoint:(CGPoint)point;  // A second-chance pass to look for text nodes missed by the hit test.
@end

@interface DOMHTMLAreaElement (UIKitExtensions)
- (CGRect)boundingBoxWithOwner:(DOMNode *)anOwner;
- (WKQuad)absoluteQuadWithOwner:(DOMNode *)anOwner;     // takes transforms into account
- (NSArray *)boundingBoxesWithOwner:(DOMNode *)anOwner;
- (NSArray *)absoluteQuadsWithOwner:(DOMNode *)anOwner; // return array of WKQuadObjects. takes transforms into account
@end

@interface DOMHTMLSelectElement (UIKitExtensions)
- (unsigned)completeLength;
- (DOMNode *)listItemAtIndex:(int)anIndex;
@end

@interface DOMHTMLImageElement (WebDOMHTMLImageElementOperationsPrivate)
- (NSData *)dataRepresentation:(BOOL)rawImageData;
- (NSString *)mimeType;
@end

@interface DOMElement (DOMUIKitComplexityExtensions) 
- (int)structuralComplexityContribution; // Does not include children.
@end

#endif
