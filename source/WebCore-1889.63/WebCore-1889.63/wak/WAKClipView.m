//
//  WAKClipView.m
//  WebCore
//
//  Copyright 2008 Apple. All rights reserved.
//

#import "config.h"
#import "WAKClipView.h"

#import "WAKViewPrivate.h"
#import <wtf/Assertions.h>

@implementation WAKClipView

@synthesize documentView = _documentView;
@synthesize copiesOnScroll = _copiesOnScroll;

- (id)initWithFrame:(CGRect)rect
{
    WKViewRef view = WKViewCreateWithFrame(rect, &viewContext);
    self = [self _initWithViewRef:view];
    WKRelease(view);
    return self;
}

- (void)dealloc
{
    [_documentView release];
    [super dealloc];
}

// WAK internal function for WAKScrollView.
- (void)_setDocumentView:(WAKView *)aView
{
    if (_documentView == aView)
        return;

    [_documentView removeFromSuperview];
    [_documentView release];
    _documentView = [aView retain];
    [self addSubview:_documentView];
}

- (CGRect)documentVisibleRect 
{     
    if (_documentView)
        return WKViewConvertRectFromSuperview([_documentView _viewRef], [self bounds]);
    return CGRectZero;
}

@end
