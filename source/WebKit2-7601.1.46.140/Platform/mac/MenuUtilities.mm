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

#import "config.h"
#import "MenuUtilities.h"

#if PLATFORM(MAC)

#import "StringUtilities.h"
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/LocalizedStrings.h>
#import <objc/runtime.h>

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
#import <WebCore/TUCallSPI.h>
#endif

namespace WebKit {

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

NSString *menuItemTitleForTelephoneNumberGroup()
{
    if ([getTUCallClass() respondsToSelector:@selector(supplementalDialTelephonyCallString)])
        return [getTUCallClass() supplementalDialTelephonyCallString];
    return WEB_UI_STRING("Call Using iPhone:", "menu item title for phone number");
}

NSMenuItem *menuItemForTelephoneNumber(const String& telephoneNumber)
{
    NSArray *proposedMenu = [[getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:getDDBinderPhoneNumberKey() service:nil context:nil];
    for (NSMenuItem *item in proposedMenu) {
        NSDictionary *representedObject = item.representedObject;
        if (![representedObject isKindOfClass:[NSDictionary class]])
            continue;

        DDAction *actionObject = [representedObject objectForKey:@"DDAction"];
        if (![actionObject isKindOfClass:getDDActionClass()])
            continue;

        if ([actionObject.actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = formattedPhoneNumberString(telephoneNumber);
            return item;
        }
    }

    return nil;
}

RetainPtr<NSMenu> menuForTelephoneNumber(const String& telephoneNumber)
{
    RetainPtr<NSMenu> menu = adoptNS([[NSMenu alloc] init]);
    NSMutableArray *faceTimeItems = [NSMutableArray array];
    NSMenuItem *dialItem = nil;

    NSArray *proposedMenu = [[getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:getDDBinderPhoneNumberKey() service:nil context:nil];
    for (NSMenuItem *item in proposedMenu) {
        NSDictionary *representedObject = item.representedObject;
        if (![representedObject isKindOfClass:[NSDictionary class]])
            continue;

        DDAction *actionObject = [representedObject objectForKey:@"DDAction"];
        if (![actionObject isKindOfClass:getDDActionClass()])
            continue;

        if ([actionObject.actionUTI hasPrefix:@"com.apple.dial"]) {
            dialItem = item;
            continue;
        }

        if ([actionObject.actionUTI hasPrefix:@"com.apple.facetime"])
            [faceTimeItems addObject:item];
    }

    if (dialItem)
        [menu addItem:dialItem];

    if (faceTimeItems.count) {
        if ([menu numberOfItems])
            [menu addItem:[NSMenuItem separatorItem]];
        for (NSMenuItem *item in faceTimeItems)
            [menu addItem:item];
    }

    return menu;
}

#endif

} // namespace WebKit

#endif
