/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PageThrottler.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "Page.h"
#include "PageActivityAssertionToken.h"

namespace WebCore {

static const double kThrottleHysteresisSeconds = 2.0;

PageThrottler::PageThrottler(Page* page)
    : m_page(page)
    , m_activeThrottleBlockers(0)
    , m_throttleState(PageNotThrottledState)
    , m_throttleHysteresisTimer(this, &PageThrottler::throttleHysteresisTimerFired)
{
    if (ChromeClient* chromeClient = m_page->chrome().client())
        chromeClient->incrementActivePageCount();
}

PageThrottler::~PageThrottler()
{
    for (HashSet<PageActivityAssertionToken*>::iterator i = m_activityTokens.begin(); i != m_activityTokens.end(); ++i)
        (*i)->invalidate();

    if (m_throttleState != PageThrottledState && m_page) {
        if (ChromeClient* chromeClient = m_page->chrome().client())
            chromeClient->decrementActivePageCount();
    }
}

void PageThrottler::clearPage()
{
    setThrottled(false);
    m_page = 0;
}

void PageThrottler::throttlePage()
{
    m_throttleState = PageThrottledState;

    if (!m_page)
        return;

    if (ChromeClient* chromeClient = m_page->chrome().client())
        chromeClient->decrementActivePageCount();

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            frame->document()->scriptedAnimationControllerSetThrottled(true);
    }

    m_page->throttleTimers();
}

void PageThrottler::unthrottlePage()
{
    PageThrottleState oldState = m_throttleState;
    m_throttleState = PageNotThrottledState;

    if (!m_page || oldState == PageNotThrottledState)
        return;

    if (oldState == PageThrottledState) {
        if (ChromeClient* chromeClient = m_page->chrome().client())
            chromeClient->incrementActivePageCount();
    }
    
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            frame->document()->scriptedAnimationControllerSetThrottled(false);
    }

    m_page->unthrottleTimers();
}

void PageThrottler::setThrottled(bool isThrottled)
{
    if (isThrottled) {
        m_throttleState = PageWaitingToThrottleState;
        startThrottleHysteresisTimer();
    } else {
        unthrottlePage();
        stopThrottleHysteresisTimer();
    }
}

void PageThrottler::preventThrottling()
{
    // If we've already got events that block throttling we can increment
    // and return early
    if (m_activeThrottleBlockers++)
        return;

    if (m_throttleState == PageNotThrottledState)
        return;

    if (m_throttleState == PageThrottledState)
        unthrottlePage();

    m_throttleState = PageWaitingToThrottleState;
    stopThrottleHysteresisTimer();
}

void PageThrottler::allowThrottling()
{
    ASSERT(m_activeThrottleBlockers > 0);
    m_activeThrottleBlockers--;
    if (m_activeThrottleBlockers)
        return;

    if (m_throttleState == PageNotThrottledState)
        return;

    ASSERT(m_throttleState == PageWaitingToThrottleState);
    startThrottleHysteresisTimer();
}

void PageThrottler::stopThrottleHysteresisTimer()
{
    m_throttleHysteresisTimer.stop();
}

void PageThrottler::reportInterestingEvent()
{
    if (m_throttleState == PageNotThrottledState)
        return;
    if (m_throttleState == PageThrottledState)
        unthrottlePage();
    m_throttleState = PageWaitingToThrottleState;
    startThrottleHysteresisTimer();
}

void PageThrottler::startThrottleHysteresisTimer()
{
    if (m_throttleHysteresisTimer.isActive())
        m_throttleHysteresisTimer.stop();
    if (!m_activeThrottleBlockers)
        m_throttleHysteresisTimer.startOneShot(kThrottleHysteresisSeconds);
}

void PageThrottler::throttleHysteresisTimerFired(Timer<PageThrottler>*)
{
    ASSERT(!m_activeThrottleBlockers);
    throttlePage();
}

void PageThrottler::addActivityToken(PageActivityAssertionToken* token)
{
    if (!token || m_activityTokens.contains(token))
        return;

    m_activityTokens.add(token);
    preventThrottling();
}

void PageThrottler::removeActivityToken(PageActivityAssertionToken* token)
{
    if (!token || !m_activityTokens.contains(token))
        return;

    m_activityTokens.remove(token);
    allowThrottling();
}

}
