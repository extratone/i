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

#import <UIKit/UIKit.h>
#import <WebCore/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)

#import <DataDetectorsUI/DDAction.h>
#import <DataDetectorsUI/DDDetectionController.h>

#else

@interface DDAction : NSObject
@end

@interface DDAction (Details)
- (BOOL)hasUserInterface;
- (NSString *)localizedName;
@property (readonly) NSString *actionUTI;
@end

@protocol DDDetectionControllerInteractionDelegate <NSObject>
@end

@interface DDDetectionController : NSObject <UIActionSheetDelegate>
@end

@interface DDDetectionController (Details)
+ (DDDetectionController *)sharedController;
+ (NSArray *)tapAndHoldSchemes;
- (void)performAction:(DDAction *)action fromAlertController:(UIAlertController *)alertController interactionDelegate:(id <DDDetectionControllerInteractionDelegate>)interactionDelegate;
@end

#endif

@interface DDDetectionController (DetailsToBeRemoved)
// FIXME: This will be removed as soon as <rdar://problem/16346913> is fixed.
- (NSArray *)actionsForAnchor:(id)anchor url:(NSURL *)targetURL forFrame:(id)frame;
@end

SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectorsUI)
SOFT_LINK_CLASS(DataDetectorsUI, DDDetectionController)
