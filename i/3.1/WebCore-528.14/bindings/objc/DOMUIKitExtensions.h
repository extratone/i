//
//  DOMUIKitExtensions.h
//  WebCore
//
//  Copyright (C) 2007, 2008, Apple Inc.  All rights reserved.
//


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
- (NSData *)createNSDataRepresentation:(BOOL)rawImageData;
- (NSString *)mimeType;
@end

@interface DOMElement (DOMUIKitComplexityExtensions) 
- (int)structuralComplexityContribution; // Does not include children.
@end

