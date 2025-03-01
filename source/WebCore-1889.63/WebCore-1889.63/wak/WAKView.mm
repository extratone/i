//
//  WAKView.mm
//
//  Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc.  All rights reserved.
//

#import "config.h"
#import "WAKViewPrivate.h"

#import "GraphicsContext.h"
#import "WAKClipView.h"
#import "WAKScrollView.h"
#import "WAKViewPrivate.h"
#import "WAKWindow.h"
#import "WKGraphics.h"
#import "WKUtilities.h"
#import "WKViewPrivate.h"
#import "WebCoreThreadMessage.h"
#import "WebEvent.h"
#import <wtf/Assertions.h>

NSString *WAKViewFrameSizeDidChangeNotification =   @"WAKViewFrameSizeDidChangeNotification";
NSString *WAKViewDidScrollNotification =            @"WAKViewDidScrollNotification";

static WAKView *globalFocusView = nil;
static CGInterpolationQuality sInterpolationQuality;

static void setGlobalFocusView(WAKView *view)
{
    if (view == globalFocusView)
        return;

    [view retain];
    [globalFocusView release];
    globalFocusView = view;
}

static WAKScrollView *enclosingScrollView(WAKView *view)
{
    view = [view superview];
    while (view && ![view isKindOfClass:[WAKScrollView class]])
        view = [view superview];
    return (WAKScrollView *)view;
}

@interface WAKScrollView()
- (void)_adjustScrollers;
@end

@interface WAKView (WAKInternal)
- (id)_initWithViewRef:(WKViewRef)viewRef;
@end

@implementation WAKView

static NSInvocation* invocationForPostNotification(NSString *name, id object, id userInfo)
{
    NSNotificationCenter *target = [NSNotificationCenter defaultCenter];
    NSInvocation *invocation = WebThreadMakeNSInvocation(target, @selector(postNotificationName:object:userInfo:));
    [invocation setArgument:&name atIndex:2];
    [invocation setArgument:&object atIndex:3];
    [invocation setArgument:&userInfo atIndex:4];
    return invocation;
}

static void notificationCallback (WKViewRef v, WKViewNotificationType type, void *userInfo)
{
    UNUSED_PARAM(v);
    WAKView *view = (WAKView *)userInfo;
    switch (type){
        case WKViewNotificationViewDidMoveToWindow: {
            [view viewDidMoveToWindow];
            break;
        }
        case WKViewNotificationViewFrameSizeChanged: {
            [view frameSizeChanged];
            if (WAKScrollView *scrollView = enclosingScrollView(view))
                [scrollView _adjustScrollers];

            // Posting a notification to the main thread can cause the WebThreadLock to be
            // relinquished, which gives the main thread a change to run (and possible try
            // to paint). We don't want this to happen if we've updating view sizes in the middle
            // of layout, so use an async notification. <rdar://problem/6745974>
            NSInvocation *invocation = invocationForPostNotification(WAKViewFrameSizeDidChangeNotification, view, nil);
            WebThreadCallDelegateAsync(invocation);
            break;
        }
        case WKViewNotificationViewDidScroll: {
            WebThreadRunOnMainThread(^ {
                 [[NSNotificationCenter defaultCenter] postNotificationName:WAKViewDidScrollNotification object:view userInfo:nil];
            });
            break;
        }            
        default: {
            break;
        }
    }
}

- (void)handleEvent:(WebEvent *)event
{
    ASSERT(event);
    WAKView *view = self;
    while (view) {
        if ([view _selfHandleEvent:event])
            break;
        view = [view superview];
    }
}

- (BOOL)_selfHandleEvent:(WebEvent *)event
{
    WebEventType type = event.type;
    
    switch (type) {
        case WebEventMouseDown: {
            [self mouseDown:event];
            break;
        }
        case WebEventMouseUp: {
            [self mouseUp:event];
            break;
        }
        case WebEventMouseMoved: {
            [self mouseMoved:event];
            break;
        }
        case WebEventKeyDown: {
            [self keyDown:event];
            break;
        }
        case WebEventKeyUp: {
            [self keyUp:event];
            break;
        }
        case WebEventScrollWheel: {
            [self scrollWheel:event];
            break;
        }
#if ENABLE(TOUCH_EVENTS)
        case WebEventTouchBegin:
        case WebEventTouchChange:
        case WebEventTouchEnd:
        case WebEventTouchCancel: {
            [self touch:event];
            break;
        }
#endif
        default:
            ASSERT_NOT_REACHED();
            break;
    }
    return YES;
}

- (NSResponder *)nextResponder
{
    return [self superview];
}

static bool responderCallback(WKViewRef, WKViewResponderCallbackType type, void *userInfo)
{
    return [(WAKView *)userInfo _handleResponderCall:type];
}

- (BOOL)_handleResponderCall:(WKViewResponderCallbackType)type
{
    switch (type) {
        case WKViewResponderAcceptsFirstResponder:
            return [self acceptsFirstResponder];
        case WKViewResponderBecomeFirstResponder:
            return [self becomeFirstResponder];
        case WKViewResponderResignFirstResponder:
            return [self resignFirstResponder];
    }
    return NO;
}

static void willRemoveSubviewCallback(WKViewRef view, WKViewRef subview)
{
    [WAKViewForWKViewRef(view) willRemoveSubview:WAKViewForWKViewRef(subview)];
}

static void invalidateGStateCallback(WKViewRef view)
{
    [WAKViewForWKViewRef(view) invalidateGState];
}

+ (WAKView *)_wrapperForViewRef:(WKViewRef)_viewRef
{
    ASSERT(_viewRef);
    if (_viewRef->isa.classInfo == &WKViewClassInfo)
        return [[[WAKView alloc] _initWithViewRef:_viewRef] autorelease];
    WKError ("unable to create wrapper for %s\n", _viewRef->isa.classInfo->name);
    return nil;
}

- (id)_initWithViewRef:(WKViewRef)viewR
{
    self = [super init];
    if (!self)
        return nil;

    viewRef = (WKViewRef)WKRetain (viewR);
    viewRef->wrapper = (void *)self;

    return self;
}

- (id)init
{
    return [self initWithFrame:CGRectZero];
}

- (id)initWithFrame:(CGRect)rect
{
    WKViewRef view = WKViewCreateWithFrame(rect, &viewContext);
    self = [self _initWithViewRef:view];
    if (self) {
        viewContext.notificationCallback = notificationCallback;
        viewContext.notificationUserInfo = self;
        viewContext.responderCallback = responderCallback;
        viewContext.responderUserInfo = self;
        viewContext.willRemoveSubviewCallback = willRemoveSubviewCallback;
        viewContext.invalidateGStateCallback = invalidateGStateCallback;
    }
    WKRelease(view);
    return self;
}

- (void)dealloc
{
    [[[subviewReferences copy] autorelease] makeObjectsPerformSelector:@selector(removeFromSuperview)];

    if (viewRef) {
        _WKViewSetViewContext (viewRef, 0);
        viewRef->wrapper = NULL;
        WKRelease (viewRef);
    }
    
    [subviewReferences release];
    
    [super dealloc];
}

- (WAKWindow *)window
{
    return WKViewGetWindow(viewRef);
}

- (WKViewRef)_viewRef
{
    return viewRef;
}

- (NSMutableSet *)_subviewReferences
{
    if (!subviewReferences)
        subviewReferences = [[NSMutableSet alloc] init];
    return subviewReferences;
}

static void _WAKCopyWrapper(const void *value, void *context)
{
    if (!value)
        return;
    if (!context)
        return;
    
    NSMutableArray *array = (NSMutableArray *)context;
    WAKView *view = WAKViewForWKViewRef((WKViewRef)value);
    if (view)
        [array addObject:view];
}

- (NSArray *)subviews
{
    CFArrayRef subviews = WKViewGetSubviews([self _viewRef]);
    if (!subviews)
        return [NSArray array];
    
    CFIndex count = CFArrayGetCount(subviews);
    if (count == 0)
        return [NSArray array];
    
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:count];
    if (!result)
        return [NSArray array];
    
    CFArrayApplyFunction(subviews, CFRangeMake(0, count), _WAKCopyWrapper, (void*)result);
    
    return result;
}

- (WAKView *)superview
{
    return WAKViewForWKViewRef(viewRef->superview);
}

- (WAKView *)lastScrollableAncestor
{
    WAKView *view = nil;
    WAKScrollView *scrollView = enclosingScrollView(self);
    
    while (scrollView) {
        
        CGSize scrollViewSize = WKViewGetFrame((WKViewRef)scrollView).size;

        WAKView *documentView = [scrollView documentView];
        scrollView = enclosingScrollView(scrollView);
                                         
        // If this document view can be scrolled, and this is NOT the last scroll view.
        // Our last scroll view is non-existent as far as we're concerned.

        if (scrollView && !CGSizeEqualToSize(scrollViewSize, [documentView frame].size))
            view = documentView;
    }
    
    return view;
}

- (void)addSubview:(WAKView *)subview
{
    [subview retain];
    [subview removeFromSuperview];
    WKViewAddSubview (viewRef, [subview _viewRef]);
    
    // Keep a reference to subview so it sticks around.
    [[self _subviewReferences] addObject:subview];
    [subview release];
}

- (void)willRemoveSubview:(WAKView *)subview
{
    UNUSED_PARAM(subview);
}

- (void)removeFromSuperview
{
    WAKView *oldSuperview = [[self superview] retain];
    WKViewRemoveFromSuperview (viewRef);
    [[oldSuperview _subviewReferences] removeObject:self];
    [oldSuperview release];
}

- (void)viewDidMoveToWindow
{
}

- (void)frameSizeChanged
{
}

- (void)setNeedsDisplay:(BOOL)flag
{
    if (!flag)
        return;
    [self setNeedsDisplayInRect:WKViewGetBounds(viewRef)];
}

- (void)setNeedsDisplayInRect:(CGRect)invalidRect
{
    if (_isHidden)
        return;

    WAKWindow *window = [self window];
    if (!window || CGRectIsEmpty(invalidRect))
        return;

    CGRect rect = CGRectIntersection(invalidRect, [self bounds]);
    if (CGRectIsEmpty(rect))
        return;

    CGRect baseRect = WKViewConvertRectToBase(viewRef, rect);
    [window setNeedsDisplayInRect:baseRect];
}

- (BOOL)needsDisplay 
{
    return NO;
}

- (void)display
{
    [self displayRect:WKViewGetBounds(viewRef)];
}

- (void)displayIfNeeded
{
}

- (void)drawRect:(CGRect)rect
{
    // Do nothing.  Override in subclass.
    UNUSED_PARAM(rect);
}

- (void)viewWillDraw
{
    [[self subviews] makeObjectsPerformSelector:@selector(viewWillDraw)];
}

+ (WAKView *)focusView
{
    return globalFocusView;
}

- (NSRect)bounds
{ 
    return WKViewGetBounds (viewRef);
}

- (NSRect)frame 
{ 
    return WKViewGetFrame (viewRef);
}

- (void)setFrame:(NSRect)frameRect
{
    WKViewSetFrameOrigin (viewRef, frameRect.origin);
    WKViewSetFrameSize (viewRef, frameRect.size);
}

- (void)setFrameOrigin:(NSPoint)newOrigin
{
    WKViewSetFrameOrigin (viewRef, newOrigin);
}

- (void)setFrameSize:(NSSize)newSize
{
    WKViewSetFrameSize (viewRef, newSize);
}

- (void)setBoundsSize:(NSSize)size
{
    WKViewSetBoundsSize (viewRef, size);
}

- (void)setBoundsOrigin:(NSPoint)newOrigin
{
    WKViewSetBoundsOrigin(viewRef, newOrigin);
}

- (void)_lockFocusViewInContext:(CGContextRef)context
{
    ASSERT(context);
    setGlobalFocusView(self);
    CGContextSaveGState(context);

    WAKView *superview = [self superview];
    if (superview)
        CGContextClipToRect(context, [superview bounds]);

    CGContextConcatCTM(context, _WKViewGetTransform(viewRef));
}

- (void)_unlockFocusViewInContext:(CGContextRef)context
{
    ASSERT(context);
    CGContextRestoreGState(context);
    setGlobalFocusView(nil);
}

static CGInterpolationQuality toCGInterpolationQuality(WebCore::InterpolationQuality quality)
{
    switch (quality) {
    case WebCore::InterpolationDefault:
        return kCGInterpolationDefault;
    case WebCore::InterpolationNone:
        return kCGInterpolationNone;
    case WebCore::InterpolationLow:
        return kCGInterpolationLow;
    case WebCore::InterpolationMedium:
        return kCGInterpolationMedium;
    case WebCore::InterpolationHigh:
        return kCGInterpolationHigh;
    default:
        ASSERT_NOT_REACHED();
        return kCGInterpolationLow;
    }
}

+ (void)_setInterpolationQuality:(int)quality
{
    sInterpolationQuality = toCGInterpolationQuality((WebCore::InterpolationQuality)quality);
}

- (void)_drawRect:(NSRect)dirtyRect context:(CGContextRef)context lockFocus:(bool)lockFocus
{
    if (_isHidden)
        return;

    ASSERT(viewRef);
    ASSERT(context);
    CGRect localRect = WKViewGetBounds(viewRef);
    dirtyRect = CGRectIntersection(localRect, dirtyRect);
    if (CGRectIsEmpty(dirtyRect))
        return;

    if (lockFocus)
        [self _lockFocusViewInContext:context];

    if (viewRef->scale != 1)
        CGContextSetInterpolationQuality(context, sInterpolationQuality);

    CGContextClipToRect(context, dirtyRect);
    [self drawRect:dirtyRect];

    if (!_drawsOwnDescendants) {
        NSArray *subViews = [self subviews];
        for (WAKView *subView in subViews) {
            NSRect childDirtyRect = [self convertRect:dirtyRect toView:subView];
            [subView _drawRect:CGRectIntegral(childDirtyRect) context:context lockFocus:YES];
        }
    }

    if (lockFocus)
        [self _unlockFocusViewInContext:context];
}

- (void)displayRect:(NSRect)rect
{
    CGContextRef context = WKGetCurrentGraphicsContext();
    if (!context) {
        WKError ("unable to get context for view");
        return;
    }

    [self _drawRect:rect context:context lockFocus:YES];
}

- (void)displayRectIgnoringOpacity:(NSRect)rect
{
    [self displayRect:rect];
}

- (void)displayRectIgnoringOpacity:(NSRect)rect inContext:(CGContextRef)context
{
    if (!context) {
        WKError ("invalid parameter: context must not be NULL");
        return;
    }

    CGContextRef previousContext = WKGetCurrentGraphicsContext();
    if (context != previousContext)
        WKSetCurrentGraphicsContext(context);

    [self _drawRect:rect context:context lockFocus:NO];

    if (context != previousContext)
        WKSetCurrentGraphicsContext (previousContext);
}

- (NSRect)visibleRect
{
    return WKViewGetVisibleRect (viewRef);
}

- (NSPoint)convertPoint:(NSPoint)aPoint toView:(WAKView *)aView 
{
    CGPoint p = WKViewConvertPointToBase (viewRef, aPoint);
    if (aView)
        return WKViewConvertPointFromBase ([aView _viewRef], p);
    return p;
}

- (NSPoint)convertPoint:(NSPoint)aPoint fromView:(WAKView *)aView
{
    if (aView)
        aPoint = WKViewConvertPointToBase ([aView _viewRef], aPoint);
    return WKViewConvertPointFromBase (viewRef, aPoint);
}

- (NSSize)convertSize:(NSSize)size toView:(WAKView *)aView
{
    return [self convertRect:NSMakeRect(0.0f, 0.0f, size.width, size.height) toView:aView].size;
}

- (NSRect)convertRect:(NSRect)aRect fromView:(WAKView *)aView
{
    if (aView)
        aRect = WKViewConvertRectToBase ([aView _viewRef], aRect);
    return WKViewConvertRectFromBase (viewRef, aRect);
}

- (NSRect)convertRect:(NSRect)aRect toView:(WAKView *)aView
{
    CGRect r = WKViewConvertRectToBase (viewRef, aRect);
    if (aView)
        return WKViewConvertRectFromBase ([aView _viewRef], r);
    return r;
}

- (void)lockFocus
{
    [self _lockFocusViewInContext:WKGetCurrentGraphicsContext()];
}

- (void)unlockFocus
{
    [self _unlockFocusViewInContext:WKGetCurrentGraphicsContext()];
}

- (WAKView *)hitTest:(NSPoint)point
{
    if (!CGRectContainsPoint([self frame], point))
        return nil;

    CGPoint subviewPoint = WKViewConvertPointFromSuperview(viewRef, point);
    for (WAKView *subview in [self subviews]) {
        if (WAKView *hitView = [subview hitTest: subviewPoint])
            return hitView;
    }

    return self;
}

- (void)setHidden:(BOOL)flag 
{
    ASSERT(viewRef);
    if (_isHidden != flag) {
        // setNeedsDisplay does nothing if the view is hidden so call it at the right time.
        if (flag) {
            [self setNeedsDisplay:YES];
            _isHidden = flag;
        } else {
            _isHidden = flag;
            [self setNeedsDisplay:YES];
        }
    }
}

- (BOOL)isDescendantOf:(NSView *)aView
{
    if (aView == self)
        return YES;
    else if (![self superview])
        return NO;
    else
        return [[self superview] isDescendantOf:aView];
}

- (BOOL)isHiddenOrHasHiddenAncestor
{
    if (_isHidden)
        return YES;

    if (![self superview])
        return NO;

    return [[self superview] isHiddenOrHasHiddenAncestor];
}

- (BOOL)mouse:(NSPoint)aPoint inRect:(NSRect)aRect 
{ 
    return WKMouseInRect (aPoint, aRect);
}

- (BOOL)needsPanelToBecomeKey
{
    return true;
}

- (void)setNextKeyView:(NSView *)aView
{
    UNUSED_PARAM(aView);
}

- (WAKView *)previousValidKeyView { return nil; }
- (WAKView *)nextKeyView { return nil; }
- (WAKView *)nextValidKeyView { return nil; }
- (WAKView *)previousKeyView { return nil; }

- (void)invalidateGState { }
- (void)releaseGState { }
- (BOOL)inLiveResize { return NO; }

- (void)setAutoresizingMask:(unsigned int)mask
{
    WKViewSetAutoresizingMask(viewRef, mask);
}

- (unsigned int)autoresizingMask
{
    return WKViewGetAutoresizingMask(viewRef);
}

- (void)scrollPoint:(NSPoint)point 
{
    if (WAKScrollView *scrollView = enclosingScrollView(self)) {
        CGPoint scrollViewPoint = [self convertPoint:point toView:scrollView];
        [scrollView scrollPoint:scrollViewPoint];
    }
}

- (BOOL)scrollRectToVisible:(NSRect)rect
{
    // Scroll if the rect is not already visible.
    CGRect visibleRect = [self visibleRect];
    if (!CGRectContainsRect(visibleRect, rect) && !CGRectContainsRect(rect, visibleRect))
        [self scrollPoint:rect.origin];

    // Return value ignored by WebCore.

    return NO;
}

// Overridden by subclasses (only WebHTMLView for now).
- (void)setNeedsLayout:(BOOL)flag
{
    UNUSED_PARAM(flag);
}

- (void)layout { }
- (void)layoutIfNeeded { }

- (void)setScale:(float)scale
{
    WKViewSetScale(viewRef, scale);
}

- (float)scale
{
    return WKViewGetScale(viewRef);
}

- (void)_setDrawsOwnDescendants:(BOOL)draw
{
    _drawsOwnDescendants = draw;
}

- (NSString *)description
{
    NSMutableString *description = [NSMutableString stringWithFormat:@"<%@: WAK: %p (WK: %p); ", [self class], self, viewRef];

    CGRect frame = [self frame];
    [description appendFormat:@"frame = (%g %g; %g %g)>", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height];

    return description;
}

- (void)_appendDescriptionToString:(NSMutableString *)info atLevel:(int)level
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if ([info length] != 0)
        [info appendString:@"\n"];

    for (int i = 1; i <= level; i++)
        [info appendString:@"   | "];

    [info appendString:[self description]];

    for (WAKView *subview in [self subviews])
        [subview _appendDescriptionToString:info atLevel:level + 1];

    [pool release];
}

@end

