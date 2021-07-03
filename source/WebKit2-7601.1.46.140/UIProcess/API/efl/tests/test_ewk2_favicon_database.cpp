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
#include "WKEinaSharedString.h"

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

class EWK2FaviconDatabaseTest : public EWK2UnitTestBase {
public:
    struct IconRequestData {
        Evas_Object* view;
        Evas_Object* icon;
    };

    static void serverCallback(SoupServer* httpServer, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
    {
        if (message->method != SOUP_METHOD_GET) {
            soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
        }

        if (!strcmp(path, "/favicon.ico")) {
            CString faviconPath = environment->pathForResource("blank.ico");
            Eina_File* f = eina_file_open(faviconPath.data(), false);
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

            soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, contents, fileSize);
            soup_message_set_status(message, SOUP_STATUS_OK);
            soup_message_body_complete(message->response_body);

            eina_file_map_free(f, contents);
            eina_file_close(f);
            return;
        }

        const char contents[] = "<html><body>favicon test</body></html>";
        soup_message_set_status(message, SOUP_STATUS_OK);
        soup_message_body_append(message->response_body, SOUP_MEMORY_COPY, contents, strlen(contents));
        soup_message_body_complete(message->response_body);
    }

    static void requestFaviconData(Ewk_Favicon_Database *faviconDatabase, const char *url, void *userdata)
    {
        IconRequestData* data = static_cast<IconRequestData*>(userdata);

        Evas* evas = evas_object_evas_get(data->view);
        data->icon = ewk_favicon_database_icon_get(faviconDatabase, url, evas);
    }
};

TEST_F(EWK2FaviconDatabaseTest, ewk_favicon_database_async_icon_get)
{
    std::unique_ptr<EWK2UnitTestServer> httpServer = std::make_unique<EWK2UnitTestServer>();
    httpServer->run(serverCallback);

    // Set favicon database path and enable functionality.
    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_favicon_database_directory_set(context, 0);
    Ewk_Favicon_Database* database = ewk_context_favicon_database_get(context);

    IconRequestData data = { webView(), 0 };
    ewk_favicon_database_icon_change_callback_add(database, requestFaviconData, &data);

    // We need to load the page first to ensure the icon data will be
    // in the database in case there's an associated favicon.
    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath("/").data()));

    while (!data.icon)
        ecore_main_loop_iterate();

    ASSERT_TRUE(data.icon);

    // It is a 16x16 favicon.
    int width, height;
    evas_object_image_size_get(data.icon, &width, &height);
    EXPECT_EQ(16, width);
    EXPECT_EQ(16, height);
    evas_object_unref(data.icon);

    ewk_favicon_database_icon_change_callback_del(database, requestFaviconData);
}

TEST_F(EWK2FaviconDatabaseTest, ewk_favicon_database_clear)
{
    std::unique_ptr<EWK2UnitTestServer> httpServer1 = std::make_unique<EWK2UnitTestServer>();
    httpServer1->run(serverCallback);

    // Set favicon database path and enable functionality.
    Ewk_Context* context = ewk_view_context_get(webView());
    ewk_context_favicon_database_directory_set(context, 0);
    Ewk_Favicon_Database* database = ewk_context_favicon_database_get(context);

    IconRequestData data1 = { webView(), 0 };
    ewk_favicon_database_icon_change_callback_add(database, requestFaviconData, &data1);

    ASSERT_TRUE(loadUrlSync(httpServer1->getURLForPath("/").data()));

    while (!data1.icon)
        ecore_main_loop_iterate();

    ASSERT_TRUE(data1.icon);
    evas_object_unref(data1.icon);

    ewk_favicon_database_icon_change_callback_del(database, requestFaviconData);

    // Runs another httpServer for loading another favicon.
    // EWK2UnitTestServer runs on random port that automatically makes different url of the favicon.
    std::unique_ptr<EWK2UnitTestServer> httpServer2 = std::make_unique<EWK2UnitTestServer>();
    httpServer2->run(serverCallback);

    IconRequestData data2 = { webView(), 0 };
    ewk_favicon_database_icon_change_callback_add(database, requestFaviconData, &data2);

    ASSERT_TRUE(loadUrlSync(httpServer2->getURLForPath("/").data()));

    while (!data2.icon)
        ecore_main_loop_iterate();
    
    ASSERT_TRUE(data2.icon);
    evas_object_unref(data2.icon);

    ewk_favicon_database_icon_change_callback_del(database, requestFaviconData);

    ewk_favicon_database_clear(database);

    data1.icon = ewk_favicon_database_icon_get(database, httpServer1->getURLForPath("/").data(), evas_object_evas_get(webView()));
    ASSERT_FALSE(data1.icon);

    data2.icon = ewk_favicon_database_icon_get(database, httpServer2->getURLForPath("/").data(), evas_object_evas_get(webView()));
    ASSERT_FALSE(data2.icon);
}
