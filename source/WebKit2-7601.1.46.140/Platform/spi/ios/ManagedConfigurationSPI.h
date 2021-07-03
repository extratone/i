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

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

// FIXME: We conditionally enclose the ManagedConfiguration headers in an extern "C" linkage
// block to make them suitable for C++ use.
#ifdef __cplusplus
extern "C" {
#endif

#import <ManagedConfiguration/MCFeatures.h>
#import <ManagedConfiguration/MCProfileConnection.h>

#ifdef __cplusplus
}
#endif

#else

typedef enum MCRestrictedBoolType {
    MCRestrictedBoolExplicitNo = 1 << 1,
} MCRestrictedBoolType;

@interface MCProfileConnection : NSObject
@end

@interface MCProfileConnection (Details)
+ (MCProfileConnection *)sharedConnection;
- (MCRestrictedBoolType)effectiveBoolValueForSetting:(NSString *)feature;
@end

#endif

WTF_EXTERN_C_BEGIN

extern NSString * const MCFeatureDefinitionLookupAllowed;

WTF_EXTERN_C_END

SOFT_LINK_PRIVATE_FRAMEWORK(ManagedConfiguration);
SOFT_LINK_CLASS(ManagedConfiguration, MCProfileConnection);
SOFT_LINK_CONSTANT(ManagedConfiguration, MCFeatureDefinitionLookupAllowed, NSString *)

#define MCFeatureDefinitionLookupAllowed getMCFeatureDefinitionLookupAllowed()
