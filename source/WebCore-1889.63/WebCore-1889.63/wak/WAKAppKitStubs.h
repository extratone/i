//
//  WAKAppKitStubs.h
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//
/* Unicodes we reserve for function keys on the keyboard,  OpenStep reserves the range 0xF700-0xF8FF for this purpose.  The availability of various keys will be system dependent. */

#ifndef WAKAppKitStubs_h
#define WAKAppKitStubs_h

#import "WKTypes.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
// FIXME: <rdar://problem/6669434> Switch from using NSGeometry methods to CGGeometry methods
#import <Foundation/NSGeometry.h>

#ifndef NSClipView
#define NSClipView WAKClipView
#endif
#ifndef NSView
#define NSView WAKView
#endif
#ifndef NSScrollView
#define NSScrollView WAKScrollView
#endif
#ifndef WebDynamicScrollBarsView
#define WebDynamicScrollBarsView WAKScrollView
#endif
#ifndef NSWindow
#define NSWindow WAKWindow
#endif
#ifndef NSResponder
#define NSResponder WAKResponder
#endif

/* Device-independent bits found in event modifier flags */
enum {
	NSAlphaShiftKeyMask =		1 << 16,
	NSShiftKeyMask =		1 << 17,
	NSControlKeyMask =		1 << 18,
	NSAlternateKeyMask =		1 << 19,
	NSCommandKeyMask =		1 << 20,
	NSNumericPadKeyMask =		1 << 21,
	NSHelpKeyMask =			1 << 22,
	NSFunctionKeyMask =		1 << 23,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
	NSDeviceIndependentModifierFlagsMask = 0xffff0000U
#endif
};

typedef enum _WKWritingDirection {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
    WKWritingDirectionNatural = -1, /* Determines direction using the Unicode Bidi Algorithm rules P2 and P3 */
#endif /* MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4 */
    WKWritingDirectionLeftToRight = 0,	/* Left to right writing direction */
    WKWritingDirectionRightToLeft	/* Right to left writing direction */
} WKWritingDirection;

typedef enum _NSSelectionAffinity {
    NSSelectionAffinityUpstream = 0,
    NSSelectionAffinityDownstream = 1
} NSSelectionAffinity;

typedef enum _NSCellState {
    NSMixedState = -1,
    NSOffState   =  0,
    NSOnState    =  1    
} NSCellStateValue;

typedef enum _NSCompositingOperation {
    NSCompositeClear		= 0,
    NSCompositeCopy		= 1,
    NSCompositeSourceOver	= 2,
    NSCompositeSourceIn		= 3,
    NSCompositeSourceOut	= 4,
    NSCompositeSourceAtop	= 5,
    NSCompositeDestinationOver	= 6,
    NSCompositeDestinationIn	= 7,
    NSCompositeDestinationOut	= 8,
    NSCompositeDestinationAtop	= 9,
    NSCompositeXOR		= 10,
    NSCompositePlusDarker	= 11,
    NSCompositeHighlight	= 12,
    NSCompositePlusLighter	= 13
} NSCompositingOperation;

typedef enum _NSSelectionDirection {
    NSDirectSelection = 0,
    NSSelectingNext,
    NSSelectingPrevious
} NSSelectionDirection;

void *WKAutorelease(id object);

@interface NSCursor : NSObject
+ (void)setHiddenUntilMouseMoves:(BOOL)flag;
@end

#ifdef __cplusplus
extern "C" {
#endif

BOOL WKMouseInRect(CGPoint aPoint, CGRect aRect);
void WKBeep(void);

#ifdef __cplusplus
}
#endif

#endif // WAKAppKitStubs_h
