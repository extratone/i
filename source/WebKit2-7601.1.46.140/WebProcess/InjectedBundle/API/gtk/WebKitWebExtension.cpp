/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebExtension.h"

#include "APIDictionary.h"
#include "APIString.h"
#include "WKBundleAPICast.h"
#include "WKBundlePage.h"
#include "WebKitPrivate.h"
#include "WebKitWebExtensionPrivate.h"
#include "WebKitWebPagePrivate.h"
#include <WebCore/DNS.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebExtension
 * @Short_description: Represents a WebExtension of the WebProcess
 * @Title: WebKitWebExtension
 *
 * WebKitWebExtension is a loadable module for the WebProcess. It allows you to execute code in the
 * WebProcess and being able to use the DOM API, to change any request or to inject custom
 * JavaScript code, for example.
 *
 * To create a WebKitWebExtension you should write a module with an initialization function that could
 * be either webkit_web_extension_initialize() with prototype #WebKitWebExtensionInitializeFunction or
 * webkit_web_extension_initialize_with_user_data() with prototype #WebKitWebExtensionInitializeWithUserDataFunction.
 * This function has to be public and it has to use the #G_MODULE_EXPORT macro. It is called when the
 * web process is initialized.
 *
 * <informalexample><programlisting>
 * static void
 * web_page_created_callback (WebKitWebExtension *extension,
 *                            WebKitWebPage      *web_page,
 *                            gpointer            user_data)
 * {
 *     g_print ("Page %d created for %s\n",
 *              webkit_web_page_get_id (web_page),
 *              webkit_web_page_get_uri (web_page));
 * }
 *
 * G_MODULE_EXPORT void
 * webkit_web_extension_initialize (WebKitWebExtension *extension)
 * {
 *     g_signal_connect (extension, "page-created",
 *                       G_CALLBACK (web_page_created_callback),
 *                       NULL);
 * }
 * </programlisting></informalexample>
 *
 * The previous piece of code shows a trivial example of an extension that notifies when
 * a #WebKitWebPage is created.
 *
 * WebKit has to know where it can find the created WebKitWebExtension. To do so you
 * should use the webkit_web_context_set_web_extensions_directory() function. The signal
 * #WebKitWebContext::initialize-web-extensions is the recommended place to call it.
 *
 * To provide the initialization data used by the webkit_web_extension_initialize_with_user_data()
 * function, you have to call webkit_web_context_set_web_extensions_initialization_user_data() with
 * the desired data as parameter. You can see an example of this in the following piece of code:
 *
 * <informalexample><programlisting>
 * #define WEB_EXTENSIONS_DIRECTORY /<!-- -->* ... *<!-- -->/
 *
 * static void
 * initialize_web_extensions (WebKitWebContext *context,
 *                            gpointer          user_data)
 * {
 *   /<!-- -->* Web Extensions get a different ID for each Web Process *<!-- -->/
 *   static guint32 unique_id = 0;
 *
 *   webkit_web_context_set_web_extensions_directory (
 *      context, WEB_EXTENSIONS_DIRECTORY);
 *   webkit_web_context_set_web_extensions_initialization_user_data (
 *      context, g_variant_new_uint32 (unique_id++));
 * }
 *
 * int main (int argc, char **argv)
 * {
 *   g_signal_connect (webkit_web_context_get_default (),
 *                    "initialize-web-extensions",
 *                     G_CALLBACK (initialize_web_extensions),
 *                     NULL);
 *
 *   GtkWidget *view = webkit_web_view_new ();
 *
 *   /<!-- -->* ... *<!-- -->/
 * }
 * </programlisting></informalexample>
 */

enum {
    PAGE_CREATED,

    LAST_SIGNAL
};

typedef HashMap<WebPage*, GRefPtr<WebKitWebPage> > WebPageMap;

struct _WebKitWebExtensionPrivate {
    WebPageMap pages;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitWebExtension, webkit_web_extension, G_TYPE_OBJECT)

static void webkit_web_extension_class_init(WebKitWebExtensionClass* klass)
{
    /**
     * WebKitWebExtension::page-created:
     * @extension: the #WebKitWebExtension on which the signal is emitted
     * @web_page: the #WebKitWebPage created
     *
     * This signal is emitted when a new #WebKitWebPage is created in
     * the Web Process.
     */
    signals[PAGE_CREATED] = g_signal_new(
        "page-created",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, 0,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_WEB_PAGE);
}

static void webkitWebExtensionPageCreated(WebKitWebExtension* extension, WebPage* page)
{
    GRefPtr<WebKitWebPage> webPage = adoptGRef(webkitWebPageCreate(page));
    extension->priv->pages.add(page, webPage);
    g_signal_emit(extension, signals[PAGE_CREATED], 0, webPage.get());
}

static void webkitWebExtensionPageDestroy(WebKitWebExtension* extension, WebPage* page)
{
    extension->priv->pages.remove(page);
}

static void webkitWebExtensionDidReceiveMessage(WebKitWebExtension*, const String& messageName, API::Dictionary& message)
{
    if (messageName == String::fromUTF8("PrefetchDNS")) {
        API::String* hostname = static_cast<API::String*>(message.get(String::fromUTF8("Hostname")));
        WebCore::prefetchDNS(hostname->string());
    } else
        ASSERT_NOT_REACHED();
}

static void didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    webkitWebExtensionPageCreated(WEBKIT_WEB_EXTENSION(clientInfo), toImpl(page));
}

static void willDestroyPage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    webkitWebExtensionPageDestroy(WEBKIT_WEB_EXTENSION(clientInfo), toImpl(page));
}

static void didReceiveMessage(WKBundleRef, WKStringRef name, WKTypeRef messageBody, const void* clientInfo)
{
    ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
    webkitWebExtensionDidReceiveMessage(WEBKIT_WEB_EXTENSION(clientInfo), toImpl(name)->string(), *toImpl(static_cast<WKDictionaryRef>(messageBody)));
}

static void didReceiveMessageToPage(WKBundleRef, WKBundlePageRef page, WKStringRef name, WKTypeRef messageBody, const void* clientInfo)
{
    ASSERT(WKGetTypeID(messageBody) == WKDictionaryGetTypeID());
    if (WebKitWebPage* webPage = WEBKIT_WEB_EXTENSION(clientInfo)->priv->pages.get(toImpl(page)).get())
        webkitWebPageDidReceiveMessage(webPage, toImpl(name)->string(), *toImpl(static_cast<WKDictionaryRef>(messageBody)));
}

WebKitWebExtension* webkitWebExtensionCreate(InjectedBundle* bundle)
{
    WebKitWebExtension* extension = WEBKIT_WEB_EXTENSION(g_object_new(WEBKIT_TYPE_WEB_EXTENSION, NULL));

    WKBundleClientV1 wkBundleClient = {
        {
            1, // version
            extension, // clientInfo
        },
        didCreatePage,
        willDestroyPage,
        0, // didInitializePageGroup
        didReceiveMessage,
        didReceiveMessageToPage
    };
    WKBundleSetClient(toAPI(bundle), &wkBundleClient.base);

    return extension;
}

/**
 * webkit_web_extension_get_page:
 * @extension: a #WebKitWebExtension
 * @page_id: the identifier of the #WebKitWebPage to get
 *
 * Get the web page of the given @page_id.
 *
 * Returns: (transfer none): the #WebKitWebPage for the given @page_id, or %NULL if the
 *    identifier doesn't correspond to an exsiting web page.
 */
WebKitWebPage* webkit_web_extension_get_page(WebKitWebExtension* extension, guint64 pageID)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), 0);

    WebKitWebExtensionPrivate* priv = extension->priv;
    WebPageMap::const_iterator end = priv->pages.end();
    for (WebPageMap::const_iterator it = priv->pages.begin(); it != end; ++it)
        if (it->key->pageID() == pageID)
            return it->value.get();

    return 0;
}
