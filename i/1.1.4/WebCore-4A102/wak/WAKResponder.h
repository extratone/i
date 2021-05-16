//
//  WAKResponder.h
//  WebCore
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//

#import <Foundation/Foundation.h>

#import "WKTypes.h"
#import "GraphicsServices/GSBase.h"

@interface WAKResponder : NSObject
{

}

- (void)handleEvent:(GSEventRef)event;

- (void)scrollWheel:(GSEventRef)theEvent;
- (BOOL)tryToPerform:(SEL)anAction with:(id)anObject;
- (void)mouseEntered:(GSEventRef)theEvent;
- (void)mouseExited:(GSEventRef)theEvent;
//- (void)interpretKeyEvents:(NSArray *)eventArray;
- (void)_wk_interpretKeyEvent:(GSEventRef)event;
- (void)keyDown:(GSEventRef)event;
- (void)keyUp:(GSEventRef)event;

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

- (void)mouseDragged:(GSEventRef)theEvent;
- (void)mouseUp:(GSEventRef)theEvent;
- (void)mouseDown:(GSEventRef)theEvent;
- (void)mouseMoved:(GSEventRef)theEvent;

@end
