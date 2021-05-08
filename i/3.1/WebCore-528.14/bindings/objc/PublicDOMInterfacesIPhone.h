/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#if defined(ENABLE_TOUCH_EVENTS)
@interface DOMTouch : DOMObject
@property(readonly, retain) id <DOMEventTarget> target;
@property(readonly) unsigned identifier;
@property(readonly) int clientX;
@property(readonly) int clientY;
@property(readonly) int pageX;
@property(readonly) int pageY;
@property(readonly) int screenX;
@property(readonly) int screenY;
@end

@interface DOMTouchEvent : DOMUIEvent
@property(readonly, retain) DOMTouchList *touches;
@property(readonly, retain) DOMTouchList *targetTouches;
@property(readonly, retain) DOMTouchList *changedTouches;
@property(readonly) float scale;
@property(readonly) float rotation;
@property(readonly) BOOL ctrlKey;
@property(readonly) BOOL shiftKey;
@property(readonly) BOOL altKey;
@property(readonly) BOOL metaKey;

- (void)initTouchEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey touches:(DOMTouchList *)touches targetTouches:(DOMTouchList *)targetTouches changedTouches:(DOMTouchList *)changedTouches scale:(float)scale rotation:(float)rotation;
@end

@interface DOMGestureEvent : DOMUIEvent
@property(readonly, retain) id <DOMEventTarget> target;
@property(readonly) float scale;
@property(readonly) float rotation;
@property(readonly) BOOL ctrlKey;
@property(readonly) BOOL shiftKey;
@property(readonly) BOOL altKey;
@property(readonly) BOOL metaKey;

- (void)initGestureEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey target:(id <DOMEventTarget>)target scale:(float)scale rotation:(float)rotation;
@end
#endif // ENABLE(TOUCH_EVENTS)
