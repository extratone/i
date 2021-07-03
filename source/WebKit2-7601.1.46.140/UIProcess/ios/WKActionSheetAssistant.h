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

#if PLATFORM(IOS)

#import "DataDetectorsUISPI.h"
#import "GestureTypes.h"
#import "WKActionSheet.h"
#import <UIKit/UIPopoverController.h>
#import <wtf/RetainPtr.h>

namespace WebKit {
class WebPageProxy;
struct InteractionInformationAtPosition;
}

@class WKActionSheetAssistant;
@class _WKActivatedElementInfo;
@protocol WKActionSheetDelegate;

@protocol WKActionSheetAssistantDelegate <NSObject>
@required
- (const WebKit::InteractionInformationAtPosition&)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location;
#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element;
#endif
- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions;

@optional
- (void)updatePositionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant;
- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant willStartInteractionWithElement:(_WKActivatedElementInfo *)element;
- (void)actionSheetAssistantDidStopInteraction:(WKActionSheetAssistant *)assistant;

@end

@interface WKActionSheetAssistant : NSObject <WKActionSheetDelegate, DDDetectionControllerInteractionDelegate>
@property (nonatomic, weak) id <WKActionSheetAssistantDelegate> delegate;
- (id)initWithView:(UIView *)view;
- (void)showLinkSheet;
- (void)showImageSheet;
- (void)showDataDetectorsSheet;
- (void)cleanupSheet;
- (void)updateSheetPosition;
- (RetainPtr<NSArray>)defaultActionsForLinkSheet:(_WKActivatedElementInfo *)elementInfo;
- (RetainPtr<NSArray>)defaultActionsForImageSheet:(_WKActivatedElementInfo *)elementInfo;
- (BOOL)isShowingSheet;
@end

#endif // PLATFORM(IOS)
