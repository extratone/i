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

#ifndef ProcessThrottler_h
#define ProcessThrottler_h

#include "ProcessAssertion.h"

#include <wtf/RefCounter.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
    
enum UserObservablePageTokenType { };
typedef RefCounter::Token<UserObservablePageTokenType> UserObservablePageToken;
enum ProcessSuppressionDisabledTokenType { };
typedef RefCounter::Token<ProcessSuppressionDisabledTokenType> ProcessSuppressionDisabledToken;

class ProcessThrottlerClient;

class ProcessThrottler : private ProcessAssertionClient {
public:
    enum ForegroundActivityTokenType { };
    typedef RefCounter::Token<ForegroundActivityTokenType> ForegroundActivityToken;
    enum BackgroundActivityTokenType { };
    typedef RefCounter::Token<BackgroundActivityTokenType> BackgroundActivityToken;

    ProcessThrottler(ProcessThrottlerClient&);

    inline ForegroundActivityToken foregroundActivityToken() const;
    inline BackgroundActivityToken backgroundActivityToken() const;
    
    void didConnectToProcess(pid_t);
    void processReadyToSuspend();
    void didCancelProcessSuspension();

private:
    AssertionState assertionState();
    void updateAssertion();
    void updateAssertionNow();
    void suspendTimerFired();

    // ProcessAssertionClient
    void assertionWillExpireImminently() override;

    ProcessThrottlerClient& m_process;
    std::unique_ptr<ProcessAndUIAssertion> m_assertion;
    RunLoop::Timer<ProcessThrottler> m_suspendTimer;
    RefCounter m_foregroundCounter;
    RefCounter m_backgroundCounter;
    int m_suspendMessageCount;
};

inline ProcessThrottler::ForegroundActivityToken ProcessThrottler::foregroundActivityToken() const
{
    return ForegroundActivityToken(m_foregroundCounter.token<ForegroundActivityTokenType>());
}

inline ProcessThrottler::BackgroundActivityToken ProcessThrottler::backgroundActivityToken() const
{
    return BackgroundActivityToken(m_backgroundCounter.token<BackgroundActivityTokenType>());
}
    
}

#endif // ProcessThrottler_h
