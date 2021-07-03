//
//  WAKResponder.h
//  WebCore
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//

#ifndef WAKResponder_h
#define WAKResponder_h

#import "WKTypes.h"
#import <Foundation/Foundation.h>

@class WebEvent;

@interface WAKResponder : NSObject
{

}

- (void)handleEvent:(WebEvent *)event;

- (void)scrollWheel:(WebEvent *)theEvent;
- (BOOL)tryToPerform:(SEL)anAction with:(id)anObject;
- (void)mouseEntered:(WebEvent *)theEvent;
- (void)mouseExited:(WebEvent *)theEvent;
- (void)keyDown:(WebEvent *)event;
- (void)keyUp:(WebEvent *)event;
#if ENABLE(TOUCH_EVENTS)
- (void)touch:(WebEvent *)event;
#endif

- (void)insertText:(NSString *)text;

- (void)deleteBackward:(id)sender;
- (void)deleteForward:(id)sender;
- (void)insertParagraphSeparator:(id)sender;

- (void)moveDown:(id)sender;
- (void)moveDownAndModifySelection:(id)sender;
- (void)moveLeft:(id)sender;
- (void)moveLeftAndModifySelection:(id)sender;
- (void)moveRight:(id)sender;
- (void)moveRightAndModifySelection:(id)sender;
- (void)moveUp:(id)sender;
- (void)moveUpAndModifySelection:(id)sender;

- (WAKResponder *)nextResponder;
- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (BOOL)resignFirstResponder;

- (void)mouseUp:(WebEvent *)theEvent;
- (void)mouseDown:(WebEvent *)theEvent;
- (void)mouseMoved:(WebEvent *)theEvent;

@end

#endif // WAKResponder_h
