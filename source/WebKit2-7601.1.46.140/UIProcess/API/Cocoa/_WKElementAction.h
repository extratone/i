/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#if TARGET_OS_IPHONE

#import <WebKit/_WKActivatedElementInfo.h>

typedef void (^WKElementActionHandler)(_WKActivatedElementInfo *);
typedef BOOL (^WKElementActionDismissalHandler)(void);

typedef NS_ENUM(NSInteger, _WKElementActionType) {
    _WKElementActionTypeCustom,
    _WKElementActionTypeOpen,
    _WKElementActionTypeCopy,
    _WKElementActionTypeSaveImage,
#if !defined(TARGET_OS_IOS) || TARGET_OS_IOS
    _WKElementActionTypeAddToReadingList,
    _WKElementActionTypeOpenInDefaultBrowser WK_ENUM_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA),
    _WKElementActionTypeOpenInExternalApplication WK_ENUM_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA),
#endif
} WK_ENUM_AVAILABLE(10_10, 8_0);

WK_CLASS_AVAILABLE(10_10, 8_0)
@interface _WKElementAction : NSObject

+ (instancetype)elementActionWithType:(_WKElementActionType)type;
+ (instancetype)elementActionWithType:(_WKElementActionType)type customTitle:(NSString *)title;

+ (instancetype)elementActionWithTitle:(NSString *)title actionHandler:(WKElementActionHandler)handler;

- (void)runActionWithElementInfo:(_WKActivatedElementInfo *)info WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, readonly) _WKElementActionType type;
@property (nonatomic, readonly) NSString* title;
@property (nonatomic, copy) WKElementActionDismissalHandler dismissalHandler;

@end

#endif // TARGET_OS_IPHONE

#endif // WK_API_ENABLED
