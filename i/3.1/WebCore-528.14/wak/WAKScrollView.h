//
//  WAKScrollView.h
//  WebCore
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import <Foundation/Foundation.h>

#import "WAKClipView.h"
#import "WAKView.h"
#import "WebCoreFrameView.h"

@interface WAKScroller : WAKView
+ (float)scrollerWidth;
@end

@interface WAKScrollView : WAKView <WebCoreFrameScrollView>
{
    WAKView *_documentView;  // Only here so the ObjC instance stays around.
    id _delegate;
}

- (CGRect)documentVisibleRect;
- (void)setContentView:(WAKClipView *)aView;
- (WAKClipView *)contentView;
- (id)documentView;
- (void)setDocumentView:(WAKView *)aView;
- (void)setHasVerticalScroller:(BOOL)flag;
- (BOOL)hasVerticalScroller;
- (void)setHasHorizontalScroller:(BOOL)flag;
- (BOOL)hasHorizontalScroller;
- (void)setVerticalScroller:(WAKScroller *)anObject;
- (WAKScroller *)verticalScroller;
- (void)setHorizontalScroller:(WAKScroller *)anObject;
- (WAKScroller *)horizontalScroller;
- (void)reflectScrolledClipView:(WAKClipView *)aClipView;
- (void)setDrawsBackground:(BOOL)flag;
- (float)verticalLineScroll;
- (void)setLineScroll:(float)aFloat;
- (BOOL)drawsBackground;
- (float)horizontalLineScroll;

- (void)setAllowsHorizontalScrolling:(BOOL)flag;
- (BOOL)allowsHorizontalScrolling;
- (void)setAllowsVerticalScrolling:(BOOL)flag;
- (BOOL)allowsVerticalScrolling;
- (void)setAllowsScrolling:(BOOL)flag;
- (BOOL)allowsScrolling;

- (void)setDelegate:(id)delegate;
- (id)delegate;

- (CGRect)actualDocumentVisibleRect;

@end

@interface NSObject (WAKScrollViewDelegate)
- (BOOL)scrollView:(WAKScrollView *)scrollView shouldScrollToPoint:(CGPoint)point;
@end
