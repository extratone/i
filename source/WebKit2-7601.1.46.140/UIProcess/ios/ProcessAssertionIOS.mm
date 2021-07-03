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
#import "ProcessAssertion.h"

#if PLATFORM(IOS)

#import "AssertionServicesSPI.h"
#import <UIKit/UIApplication.h>
#import <wtf/HashSet.h>
#import <wtf/Vector.h>

#if !PLATFORM(IOS_SIMULATOR)

using WebKit::ProcessAssertionClient;

@interface WKProcessAssertionBackgroundTaskManager : NSObject

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)incrementNeedsToRunInBackgroundCount;
- (void)decrementNeedsToRunInBackgroundCount;

- (void)addClient:(ProcessAssertionClient&)client;
- (void)removeClient:(ProcessAssertionClient&)client;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    unsigned _needsToRunInBackgroundCount;
    UIBackgroundTaskIdentifier _backgroundTask;
    HashSet<ProcessAssertionClient*> _clients;
}

+ (WKProcessAssertionBackgroundTaskManager *)shared
{
    static WKProcessAssertionBackgroundTaskManager *shared = [WKProcessAssertionBackgroundTaskManager new];
    return shared;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    _backgroundTask = UIBackgroundTaskInvalid;

    return self;
}

- (void)dealloc
{
    if (_backgroundTask != UIBackgroundTaskInvalid)
        [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];

    [super dealloc];
}

- (void)addClient:(ProcessAssertionClient&)client
{
    _clients.add(&client);
}

- (void)removeClient:(ProcessAssertionClient&)client
{
    _clients.remove(&client);
}

- (void)_updateBackgroundTask
{
    if (_needsToRunInBackgroundCount && _backgroundTask == UIBackgroundTaskInvalid) {
        _backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"com.apple.WebKit.ProcessAssertion" expirationHandler:^{
            NSLog(@"Background task expired while holding WebKit ProcessAssertion.");
            Vector<ProcessAssertionClient*> clientsToNotify;
            copyToVector(_clients, clientsToNotify);
            for (auto* client : clientsToNotify)
                client->assertionWillExpireImminently();
            [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
            _backgroundTask = UIBackgroundTaskInvalid;
        }];
    }

    if (!_needsToRunInBackgroundCount && _backgroundTask != UIBackgroundTaskInvalid) {
        [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
        _backgroundTask = UIBackgroundTaskInvalid;
    }
}

- (void)incrementNeedsToRunInBackgroundCount
{
    ++_needsToRunInBackgroundCount;
    [self _updateBackgroundTask];
}

- (void)decrementNeedsToRunInBackgroundCount
{
    --_needsToRunInBackgroundCount;
    [self _updateBackgroundTask];
}

@end

namespace WebKit {

const BKSProcessAssertionFlags suspendedTabFlags = (BKSProcessAssertionAllowIdleSleep);
const BKSProcessAssertionFlags backgroundTabFlags = (BKSProcessAssertionAllowIdleSleep | BKSProcessAssertionPreventTaskSuspend);
const BKSProcessAssertionFlags foregroundTabFlags = (BKSProcessAssertionAllowIdleSleep | BKSProcessAssertionPreventTaskSuspend | BKSProcessAssertionWantsForegroundResourcePriority | BKSProcessAssertionPreventTaskThrottleDown);

static BKSProcessAssertionFlags flagsForState(AssertionState assertionState)
{
    switch (assertionState) {
    case AssertionState::Suspended:
        return suspendedTabFlags;
    case AssertionState::Background:
        return backgroundTabFlags;
    case AssertionState::Foreground:
        return foregroundTabFlags;
    }
}

ProcessAssertion::ProcessAssertion(pid_t pid, AssertionState assertionState)
{
    m_assertionState = assertionState;
    
    BKSProcessAssertionAcquisitionHandler handler = ^(BOOL acquired) {
        if (!acquired) {
            LOG_ERROR("Unable to acquire assertion for process %d", pid);
            ASSERT_NOT_REACHED();
        }
    };
    m_assertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flagsForState(assertionState) reason:BKSProcessAssertionReasonExtension name:@"Web content visible" withHandler:handler]);
}

ProcessAssertion::~ProcessAssertion()
{
    if (ProcessAssertionClient* client = this->client())
        [[WKProcessAssertionBackgroundTaskManager shared] removeClient:*client];
    [m_assertion invalidate];
}

void ProcessAssertion::setState(AssertionState assertionState)
{
    if (m_assertionState == assertionState)
        return;

    m_assertionState = assertionState;
    [m_assertion setFlags:flagsForState(assertionState)];
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, AssertionState assertionState)
    : ProcessAssertion(pid, assertionState)
{
    if (assertionState != AssertionState::Suspended)
        [[WKProcessAssertionBackgroundTaskManager shared] incrementNeedsToRunInBackgroundCount];
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
    if (state() != AssertionState::Suspended)
        [[WKProcessAssertionBackgroundTaskManager shared] decrementNeedsToRunInBackgroundCount];
}

void ProcessAndUIAssertion::setState(AssertionState assertionState)
{
    if ((state() == AssertionState::Suspended) && (assertionState != AssertionState::Suspended))
        [[WKProcessAssertionBackgroundTaskManager shared] incrementNeedsToRunInBackgroundCount];
    if ((state() != AssertionState::Suspended) && (assertionState == AssertionState::Suspended))
        [[WKProcessAssertionBackgroundTaskManager shared] decrementNeedsToRunInBackgroundCount];

    ProcessAssertion::setState(assertionState);
}

void ProcessAndUIAssertion::setClient(ProcessAssertionClient& newClient)
{
    [[WKProcessAssertionBackgroundTaskManager shared] addClient:newClient];
    if (ProcessAssertionClient* oldClient = this->client())
        [[WKProcessAssertionBackgroundTaskManager shared] removeClient:*oldClient];
    ProcessAssertion::setClient(newClient);
}

} // namespace WebKit

#else // PLATFORM(IOS_SIMULATOR)

namespace WebKit {

ProcessAssertion::ProcessAssertion(pid_t, AssertionState assertionState)
    : m_assertionState(assertionState)
{
}

ProcessAssertion::~ProcessAssertion()
{
}

void ProcessAssertion::setState(AssertionState assertionState)
{
    m_assertionState = assertionState;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, AssertionState assertionState)
    : ProcessAssertion(pid, assertionState)
{
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
}

void ProcessAndUIAssertion::setState(AssertionState assertionState)
{
    ProcessAssertion::setState(assertionState);
}

void ProcessAndUIAssertion::setClient(ProcessAssertionClient& newClient)
{
    ProcessAssertion::setClient(newClient);
}

} // namespace WebKit

#endif // PLATFORM(IOS_SIMULATOR)

#endif // PLATFORM(IOS)
