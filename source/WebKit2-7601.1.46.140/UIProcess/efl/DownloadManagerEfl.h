/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef DownloadManagerEfl_h
#define DownloadManagerEfl_h

#include "ewk_context.h"
#include "ewk_download_job_private.h"
#include <WebKit/WKRetainPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebKit {

struct ClientDownloadCallbacks {
    Ewk_Download_Requested_Cb m_requested;
    Ewk_Download_Failed_Cb m_failed;
    Ewk_Download_Cancelled_Cb m_cancelled;
    Ewk_Download_Finished_Cb m_finished;
    void* m_userData;
};

class DownloadManagerEfl {
public:
    explicit DownloadManagerEfl(WKContextRef);
    ~DownloadManagerEfl();

    void registerDownloadJob(WKDownloadRef);
    void setClientCallbacks(Ewk_Download_Requested_Cb, Ewk_Download_Failed_Cb, Ewk_Download_Cancelled_Cb, Ewk_Download_Finished_Cb, void* userData);

private:
    EwkDownloadJob* ewkDownloadJob(WKDownloadRef);
    void unregisterDownloadJob(WKDownloadRef);

    static WKStringRef decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef, WKStringRef filename, bool* allowOverwrite, const void* clientInfo);
    static void didReceiveResponse(WKContextRef, WKDownloadRef, WKURLResponseRef, const void* clientInfo);
    static void didCreateDestination(WKContextRef, WKDownloadRef, WKStringRef path, const void* clientInfo);
    static void didReceiveData(WKContextRef, WKDownloadRef, uint64_t length, const void* clientInfo);
    static void didFail(WKContextRef, WKDownloadRef, WKErrorRef, const void* clientInfo);
    static void didStart(WKContextRef, WKDownloadRef, const void* clientInfo);
    static void didCancel(WKContextRef, WKDownloadRef, const void* clientInfo);
    static void didFinish(WKContextRef, WKDownloadRef, const void* clientInfo);

    WKRetainPtr<WKContextRef> m_context;
    HashMap<uint64_t, RefPtr<EwkDownloadJob> > m_downloadJobs;

    ClientDownloadCallbacks m_clientCallbacks;
};

} // namespace WebKit

#endif // DownloadManagerEfl_h
