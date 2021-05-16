//
//  WAKWindow.h
//
//  Copyright (C) 2005, 2006, 2007, Apple Inc.  All rights reserved.
//
//  Objective-C wrapper around a WKWindow

#import <Foundation/Foundation.h>

#import <CoreGraphics/CoreGraphics.h>

#import "WAKAppKitStubs.h"

#import "WAKResponder.h"
#import "WAKView.h"
#import "WKWindow.h"
#import "WKContentObservation.h"

@interface WAKWindow : WAKResponder
{
    WKWindowRef window;
}

- (void)setContentView:(WAKView *)aView;
- (WAKView *)contentView;
- (void)close;
- (WAKResponder *)firstResponder;
- (NSPoint)convertBaseToScreen:(NSPoint)aPoint;
- (NSPoint)convertScreenToBase:(NSPoint)aPoint;
- (void)endEditingFor:(id)anObject;
- (int)windowNumber;
- (GSEventRef)currentEvent;
- (BOOL)isKeyWindow;
- (NSSelectionDirection)keyViewSelectionDirection;
- (BOOL)makeFirstResponder:(NSResponder *)aResponder;
- (WKWindowRef)_windowRef;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)sendEvent:(GSEventRef)event;
- (void)sendEvent:(GSEventRef)anEvent contentChange:(WKContentChange *)aContentChange;

@end
