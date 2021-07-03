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

#include "config.h"

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestServer.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace EWK2UnitTest;
using namespace WTF;

extern EWK2UnitTestEnvironment* environment;

static const char serverSuggestedFilename[] = "webkit-downloaded-file";
static const char testFilePath[] = "/test.pdf";

class EWK2DownloadJobTest : public EWK2UnitTestBase {
public:
    struct DownloadTestData {
        const char* fileUrl;
        const char* destinationPath;
    };

    static bool fileExists(const char* path)
    {
        struct stat buf;
        return !stat(path, &buf);
    }

    static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, void*)
    {
        if (message->method != SOUP_METHOD_GET) {
            soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
            return;
        }

        Eina_Strbuf* filePath = eina_strbuf_new();
        eina_strbuf_append(filePath, TEST_RESOURCES_DIR);
        eina_strbuf_append(filePath, path);

        Eina_File* f = eina_file_open(eina_strbuf_string_get(filePath), false);
        eina_strbuf_free(filePath);
        if (!f) {
            soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
            soup_message_body_complete(message->response_body);
            return;
        }

        size_t fileSize = eina_file_size_get(f);

        void* contents = eina_file_map_all(f, EINA_FILE_POPULATE);
        if (!contents) {
            soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
            soup_message_body_complete(message->response_body);
            return;
        }

        soup_message_set_status(message, SOUP_STATUS_OK);

        Eina_Strbuf* contentDisposition = eina_strbuf_new();
        eina_strbuf_append_printf(contentDisposition, "filename=%s", serverSuggestedFilename);
        soup_message_headers_append(message->response_headers, "Content-Disposition", eina_strbuf_string_get(contentDisposition));
        eina_strbuf_free(contentDisposition);

        soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, contents, fileSize);
        soup_message_body_complete(message->response_body);

        eina_file_map_free(f, contents);
        eina_file_close(f);
    }

    static void on_download_requested(EwkObject* eventInfo, void* userData)
    {
        DownloadTestData* testData = static_cast<DownloadTestData*>(userData);
        Ewk_Download_Job* download = static_cast<Ewk_Download_Job*>(eventInfo);
        ASSERT_EQ(EWK_DOWNLOAD_JOB_STATE_NOT_STARTED, ewk_download_job_state_get(download));
        ASSERT_EQ(0, ewk_download_job_estimated_progress_get(download));
        ASSERT_EQ(0, ewk_download_job_elapsed_time_get(download));
        ASSERT_EQ(0, ewk_download_job_received_data_size_get(download));

        Ewk_Url_Request* request = ewk_download_job_request_get(download);
        ASSERT_TRUE(request);
        EXPECT_STREQ(testData->fileUrl, ewk_url_request_url_get(request));

        Ewk_Url_Response* response = ewk_download_job_response_get(download);
        ASSERT_TRUE(response);
        EXPECT_STREQ("application/pdf", ewk_url_response_mime_type_get(response));

        EXPECT_STREQ(serverSuggestedFilename, ewk_download_job_suggested_filename_get(download));

        ASSERT_FALSE(fileExists(testData->destinationPath));
        ewk_download_job_destination_set(download, testData->destinationPath);
        EXPECT_STREQ(testData->destinationPath, ewk_download_job_destination_get(download));
    }

    static void on_download_cancelled(EwkObject*, void*)
    {
        fprintf(stderr, "Download was cancelled.\n");
        ecore_main_loop_quit();
        FAIL();
    }

    static void on_download_failed(Ewk_Download_Job_Error* downloadError, void* userData)
    {
        fprintf(stderr, "Download error: %s\n", ewk_error_description_get(downloadError->error));
        ecore_main_loop_quit();
        FAIL();
    }

    static void on_download_finished(EwkObject* eventInfo, void* userData)
    {
        DownloadTestData* testData = static_cast<DownloadTestData*>(userData);
        Ewk_Download_Job* download = static_cast<Ewk_Download_Job*>(eventInfo);

        ASSERT_EQ(1, ewk_download_job_estimated_progress_get(download));
        ASSERT_EQ(EWK_DOWNLOAD_JOB_STATE_FINISHED, ewk_download_job_state_get(download));
        ASSERT_GT(ewk_download_job_elapsed_time_get(download), 0);
        ASSERT_GT(ewk_download_job_received_data_size_get(download), 0);

        ASSERT_TRUE(fileExists(testData->destinationPath));

        ecore_main_loop_quit();
    }
};

TEST_F(EWK2DownloadJobTest, ewk_download)
{
    std::unique_ptr<EWK2UnitTestServer> httpServer = std::make_unique<EWK2UnitTestServer>();
    httpServer->run(serverCallback);

    CString fileUrl = httpServer->getURLForPath(testFilePath);

    char destinationDirectory[] = "/tmp/ewk2_download_job-XXXXXX";
    ASSERT_TRUE(mkdtemp(destinationDirectory));
    char destinationPath[64];
    snprintf(destinationPath, sizeof(destinationPath), "%s/pdf-file", destinationDirectory);

    DownloadTestData userData = { fileUrl.data(), destinationPath };
    ASSERT_FALSE(fileExists(destinationPath));

    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_download_callbacks_set(context, on_download_requested, on_download_failed, on_download_cancelled, on_download_finished, &userData);

    // Download test pdf
    ewk_view_url_set(webView(), fileUrl.data());
    ecore_main_loop_begin();

    // Clean up
    unlink(destinationPath);
    rmdir(destinationDirectory);
}
