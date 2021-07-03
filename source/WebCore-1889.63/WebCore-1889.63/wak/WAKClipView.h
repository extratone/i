//
//  WAKClipView.h
//  WebCore
//
//  Copyright 2008, 2009 Apple.  All rights reserved.
//

#ifndef WAKClipView_h
#define WAKClipView_h

#import "WAKView.h"

@interface WAKClipView : WAKView

@property (nonatomic, readonly) WAKView *documentView;
@property (nonatomic, assign) BOOL copiesOnScroll;

- (CGRect)documentVisibleRect;

@end

#endif // WAKClipView_h
