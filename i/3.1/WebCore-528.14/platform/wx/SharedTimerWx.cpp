/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "SharedTimer.h"
#include "NotImplemented.h"
#include "Widget.h"

#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <stdio.h>

#include "wx/defs.h"
#include "wx/timer.h"

namespace WebCore {

static void (*sharedTimerFiredFunction)();

class WebKitTimer: public wxTimer
{
    public:
        WebKitTimer();
        ~WebKitTimer();
        virtual void Notify();
};

WebKitTimer::WebKitTimer()
{
    wxTimer::wxTimer();
}

WebKitTimer::~WebKitTimer()
{
}

void WebKitTimer::Notify()
{
    sharedTimerFiredFunction();
}

static WebKitTimer* wkTimer; 

void setSharedTimerFiredFunction(void (*f)())
{
    sharedTimerFiredFunction = f;
}

void setSharedTimerFireTime(double fireTime)
{
    ASSERT(sharedTimerFiredFunction);
    
    double interval = fireTime - currentTime();
    
    if (!wkTimer)
        wkTimer = new WebKitTimer();
        
    unsigned int intervalInMS = interval * 1000;
    if (interval < 0) {
#ifndef NDEBUG
        // TODO: We may eventually want to assert here, to track 
        // what calls are leading to this condition. It seems to happen
        // mostly with repeating timers.
        fprintf(stderr, "WARNING: setSharedTimerFireTime: fire time is < 0 ms\n");
#endif
        intervalInMS = 0;
    }

    // FIXME: We should mimic the Windows port's behavior and add the timer fired
    // event to the event queue directly rather than making an artifical delay.
    // However, wx won't allow us to assign a simple callback function - we'd have
    // to create a fake wxEvtHandler-derived class with a timer event handler
    // function. Until there's a better way, this way is at least far less
    // hacky.
    if (intervalInMS < 10)
#if __WXMSW__
        intervalInMS = 10;
#else
        intervalInMS = 1;
#endif

    wkTimer->Start(intervalInMS, wxTIMER_ONE_SHOT);
}

void stopSharedTimer()
{
    if (wkTimer)
        wkTimer->Stop();
}

}
