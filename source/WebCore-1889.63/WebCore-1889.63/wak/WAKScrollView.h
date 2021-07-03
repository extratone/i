//
//  WAKScrollView.h
//  WebCore
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//

#ifndef WAKScrollView_h
#define WAKScrollView_h

#import "WAKView.h"
#import "WebCoreFrameView.h"
#import <Foundation/Foundation.h>

@class WAKClipView;

@interface WAKScrollView : WAKView <WebCoreFrameScrollView>
{
    WAKView *_documentView;  // Only here so the ObjC instance stays around.
    WAKClipView *_contentView;
    id _delegate;
    NSPoint _scrollOrigin;
}

- (CGRect)documentVisibleRect;
- (WAKClipView *)contentView;
- (id)documentView;
- (void)setDocumentView:(WAKView *)aView;
- (void)setHasVerticalScroller:(BOOL)flag;
- (BOOL)hasVerticalScroller;
- (void)setHasHorizontalScroller:(BOOL)flag;
- (BOOL)hasHorizontalScroller;
- (void)reflectScrolledClipView:(WAKClipView *)aClipView;
- (void)setDrawsBackground:(BOOL)flag;
- (float)verticalLineScroll;
- (void)setLineScroll:(float)aFloat;
- (BOOL)drawsBackground;
- (float)horizontalLineScroll;

- (void)setDelegate:(id)delegate;
- (id)delegate;

- (CGRect)actualDocumentVisibleRect;
- (void)setActualScrollPosition:(CGPoint)point;

- (CGRect)documentVisibleExtent; // Like actualDocumentVisibleRect, but includes areas possibly covered by translucent UI.

- (BOOL)inProgrammaticScroll;
@end

@interface NSObject (WAKScrollViewDelegate)
- (BOOL)scrollView:(WAKScrollView *)scrollView shouldScrollToPoint:(CGPoint)point;
@end

#endif // WAKScrollView_h
