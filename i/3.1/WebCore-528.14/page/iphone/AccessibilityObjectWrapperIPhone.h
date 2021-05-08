/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
 *
 * No license or rights are granted by Apple expressly or by implication,
 * estoppel, or otherwise, to Apple copyrights, patents, trademarks, trade
 * secrets or other rights.
 */

#ifndef AccessibilityObjectWrapperIPhone_h
#define AccessibilityObjectWrapperIPhone_h


#include "AccessibilityObject.h"
#include "AXObjectCache.h"

@class WAKView;

@interface AccessibilityObjectWrapper : NSObject
{
    WebCore::AccessibilityObject* m_object;
}

- (id)initWithAccessibilityObject:(WebCore::AccessibilityObject*)axObject;
- (void)detach;
- (WebCore::AccessibilityObject*)accessibilityObject;

- (id)accessibilityHitTest:(CGPoint)point;
- (BOOL)isAccessibilityElement;
- (NSString *)accessibilityLabel;
- (CGRect)accessibilityFrame;
- (NSString *)accessibilityValue;

- (NSInteger)accessibilityElementCount;
- (id)accessibilityElementAtIndex:(NSInteger)index;
- (NSInteger)indexOfAccessibilityElement:(id)element;

- (BOOL)isAttachment;
- (WAKView *)attachmentView;

@end


#endif // AccessibilityObjectWrapperIPhone_h
