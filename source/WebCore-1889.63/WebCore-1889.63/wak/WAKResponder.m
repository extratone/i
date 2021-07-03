//
//  WAKResponder.m
//  WebCore
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//

#import "config.h"
#import "WAKResponder.h"

#import "WAKViewPrivate.h"
#import "WKViewPrivate.h"

@implementation WAKResponder

// FIXME: the functions named handleEvent generally do not forward event to the parent chain.
// This method should ideally be removed, or renamed "sendEvent".
- (void)handleEvent:(WebEvent *)event
{
    UNUSED_PARAM(event);
}

- (void)_forwardEvent:(WebEvent *)event
{
    [[self nextResponder] handleEvent:event];
}

- (void)scrollWheel:(WebEvent *)event 
{ 
    [self _forwardEvent:event];
}

- (void)mouseEntered:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}

- (void)mouseExited:(WebEvent *)event 
{ 
    [self _forwardEvent:event];
}

- (void)mouseMoved:(WebEvent *)theEvent
{
    [self _forwardEvent:theEvent];
}

- (void)keyDown:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}
- (void)keyUp:(WebEvent *)event
{ 
    [self _forwardEvent:event];
}

#if ENABLE(TOUCH_EVENTS)
- (void)touch:(WebEvent *)event
{
    [self _forwardEvent:event];
}
#endif

- (WAKResponder *)nextResponder { return nil; }

- (void)insertText:(NSString *)text
{
    UNUSED_PARAM(text);
}

- (void)deleteBackward:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)deleteForward:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)insertParagraphSeparator:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveDown:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveDownAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveLeft:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveLeftAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveRight:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveRightAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveUp:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)moveUpAndModifySelection:(id)sender
{
    UNUSED_PARAM(sender);
}

- (void)mouseUp:(WebEvent *)event 
{
    [self _forwardEvent:event];
}

- (void)mouseDown:(WebEvent *)event 
{
    [self _forwardEvent:event];
}

- (BOOL)acceptsFirstResponder { return true; }
- (BOOL)becomeFirstResponder { return true; }
- (BOOL)resignFirstResponder { return true; }

- (BOOL)tryToPerform:(SEL)anAction with:(id)anObject 
{ 
    if ([self respondsToSelector:anAction]) {
        [self performSelector:anAction withObject:anObject];
        return YES;
    }
    return NO; 
}

@end
