/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef ApplicationStateTracker_h
#define ApplicationStateTracker_h

#if PLATFORM(IOS)

#import "WeakObjCPtr.h"
#import <wtf/Forward.h>
#import <wtf/WeakPtr.h>

OBJC_CLASS BKSApplicationStateMonitor;
OBJC_CLASS UIView;

namespace WebKit {

class ApplicationStateTracker {
public:
    ApplicationStateTracker(UIView *, SEL didEnterBackgroundSelector, SEL willEnterForegroundSelector);
    ~ApplicationStateTracker();

    bool isInBackground() const { return m_isInBackground; }

private:
    void applicationDidEnterBackground();
    void applicationWillEnterForeground();

    WeakObjCPtr<UIView> m_view;
    SEL m_didEnterBackgroundSelector;
    SEL m_willEnterForegroundSelector;

    bool m_isInBackground;

    WeakPtrFactory<ApplicationStateTracker> m_weakPtrFactory;

    RetainPtr<BKSApplicationStateMonitor> m_applicationStateMonitor;

    id m_didEnterBackgroundObserver;
    id m_willEnterForegroundObserver;
};

}

#endif

#endif // ApplicationStateTracker_h
