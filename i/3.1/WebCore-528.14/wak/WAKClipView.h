//
//  WAKClipView.h
//  WebCore
//
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "WAKView.h"

@interface WAKClipView : WAKView
{
}
- (void)setDocumentView:(WAKView *)aView;
- (id)documentView;
- (BOOL)copiesOnScroll;
- (void)setCopiesOnScroll:(BOOL)flag;
- (CGRect)documentVisibleRect;
@end
