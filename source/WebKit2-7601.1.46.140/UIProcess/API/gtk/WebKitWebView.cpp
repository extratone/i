/*
 * Copyright (C) 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2014 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "WebKitWebView.h"

#include "APIData.h"
#include "APISerializedScriptValue.h"
#include "ImageOptions.h"
#include "WebCertificateInfo.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebKitAuthenticationDialog.h"
#include "WebKitAuthenticationRequestPrivate.h"
#include "WebKitBackForwardListPrivate.h"
#include "WebKitContextMenuClient.h"
#include "WebKitContextMenuItemPrivate.h"
#include "WebKitContextMenuPrivate.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitEnumTypes.h"
#include "WebKitError.h"
#include "WebKitFaviconDatabasePrivate.h"
#include "WebKitFormClient.h"
#include "WebKitFullscreenClient.h"
#include "WebKitHitTestResultPrivate.h"
#include "WebKitJavascriptResultPrivate.h"
#include "WebKitLoaderClient.h"
#include "WebKitMarshal.h"
#include "WebKitNotificationPrivate.h"
#include "WebKitPolicyClient.h"
#include "WebKitPrintOperationPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitResponsePolicyDecision.h"
#include "WebKitScriptDialogPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitUIClient.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebInspectorPrivate.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GUniquePtrSoup.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/RefPtrCairo.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

#if USE(LIBNOTIFY)
#include <libnotify/notify.h>
#endif

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitWebView
 * @Short_description: The central class of the WebKit2GTK+ API
 * @Title: WebKitWebView
 *
 * #WebKitWebView is the central class of the WebKit2GTK+ API. It is
 * responsible for managing the drawing of the content and forwarding
 * of events. You can load any URI into the #WebKitWebView or a data
 * string. With #WebKitSettings you can control various aspects of the
 * rendering and loading of the content.
 *
 * Note that #WebKitWebView is scrollable by itself, so you don't need
 * to embed it in a #GtkScrolledWindow.
 */

enum {
    LOAD_CHANGED,
    LOAD_FAILED,
    LOAD_FAILED_WITH_TLS_ERRORS,

    CREATE,
    READY_TO_SHOW,
    RUN_AS_MODAL,
    CLOSE,

    SCRIPT_DIALOG,

    DECIDE_POLICY,
    PERMISSION_REQUEST,

    MOUSE_TARGET_CHANGED,

    PRINT,

    RESOURCE_LOAD_STARTED,

    ENTER_FULLSCREEN,
    LEAVE_FULLSCREEN,

    RUN_FILE_CHOOSER,

    CONTEXT_MENU,
    CONTEXT_MENU_DISMISSED,

    SUBMIT_FORM,

    INSECURE_CONTENT_DETECTED,

    WEB_PROCESS_CRASHED,

    AUTHENTICATE,

    SHOW_NOTIFICATION,

    RUN_COLOR_CHOOSER,

    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_WEB_CONTEXT,
    PROP_RELATED_VIEW,
    PROP_SETTINGS,
    PROP_USER_CONTENT_MANAGER,
    PROP_TITLE,
    PROP_ESTIMATED_LOAD_PROGRESS,
    PROP_FAVICON,
    PROP_URI,
    PROP_ZOOM_LEVEL,
    PROP_IS_LOADING,
    PROP_IS_PLAYING_AUDIO,
    PROP_EDITABLE
};

typedef HashMap<uint64_t, GRefPtr<WebKitWebResource> > LoadingResourcesMap;
typedef HashMap<uint64_t, GRefPtr<GTask> > SnapshotResultsMap;

class PageLoadStateObserver;

struct _WebKitWebViewPrivate {
    ~_WebKitWebViewPrivate()
    {
        if (javascriptGlobalContext)
            JSGlobalContextRelease(javascriptGlobalContext);

        // For modal dialogs, make sure the main loop is stopped when finalizing the webView.
        if (modalLoop && g_main_loop_is_running(modalLoop.get()))
            g_main_loop_quit(modalLoop.get());
    }

    WebKitWebView* relatedView;
    CString title;
    CString customTextEncoding;
    CString activeURI;
    bool isLoading;

    std::unique_ptr<PageLoadStateObserver> loadObserver;
    bool waitingForMainResource;
    unsigned long mainResourceResponseHandlerID;
    WebKitLoadEvent lastDelayedEvent;

    GRefPtr<WebKitBackForwardList> backForwardList;
    GRefPtr<WebKitSettings> settings;
    GRefPtr<WebKitUserContentManager> userContentManager;
    GRefPtr<WebKitWebContext> context;
    GRefPtr<WebKitWindowProperties> windowProperties;

    GRefPtr<GMainLoop> modalLoop;

    GRefPtr<WebKitHitTestResult> mouseTargetHitTestResult;
    unsigned mouseTargetModifiers;

    GRefPtr<WebKitFindController> findController;
    JSGlobalContextRef javascriptGlobalContext;

    GRefPtr<WebKitWebResource> mainResource;
    LoadingResourcesMap loadingResourcesMap;

    GRefPtr<WebKitWebInspector> inspector;

    RefPtr<cairo_surface_t> favicon;
    GRefPtr<GCancellable> faviconCancellable;
    CString faviconURI;
    unsigned long faviconChangedHandlerID;

    SnapshotResultsMap snapshotResultsMap;
    GRefPtr<WebKitAuthenticationRequest> authenticationRequest;

};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitWebView, webkit_web_view, WEBKIT_TYPE_WEB_VIEW_BASE)

static inline WebPageProxy* getPage(WebKitWebView* webView)
{
    return webkitWebViewBaseGetPage(reinterpret_cast<WebKitWebViewBase*>(webView));
}

static void webkitWebViewSetIsLoading(WebKitWebView* webView, bool isLoading)
{
    if (webView->priv->isLoading == isLoading)
        return;

    webView->priv->isLoading = isLoading;
    g_object_notify(G_OBJECT(webView), "is-loading");
}

void webkitWebViewIsPlayingAudioChanged(WebKitWebView* webView)
{
    g_object_notify(G_OBJECT(webView), "is-playing-audio");
}

class PageLoadStateObserver final : public PageLoadState::Observer {
public:
    PageLoadStateObserver(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    virtual void willChangeIsLoading() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    virtual void didChangeIsLoading() override
    {
        if (m_webView->priv->waitingForMainResource) {
            // The actual load has finished but we haven't emitted the delayed load events yet, so we are still loading.
            g_object_thaw_notify(G_OBJECT(m_webView));
            return;
        }
        webkitWebViewSetIsLoading(m_webView, getPage(m_webView)->pageLoadState().isLoading());
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    virtual void willChangeTitle() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    virtual void didChangeTitle() override
    {
        m_webView->priv->title = getPage(m_webView)->pageLoadState().title().utf8();
        g_object_notify(G_OBJECT(m_webView), "title");
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    virtual void willChangeActiveURL() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    virtual void didChangeActiveURL() override
    {
        m_webView->priv->activeURI = getPage(m_webView)->pageLoadState().activeURL().utf8();
        g_object_notify(G_OBJECT(m_webView), "uri");
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    virtual void willChangeHasOnlySecureContent() override { }
    virtual void didChangeHasOnlySecureContent() override { }

    virtual void willChangeEstimatedProgress() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    virtual void didChangeEstimatedProgress() override
    {
        g_object_notify(G_OBJECT(m_webView), "estimated-load-progress");
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    virtual void willChangeCanGoBack() override { }
    virtual void didChangeCanGoBack() override { }
    virtual void willChangeCanGoForward() override { }
    virtual void didChangeCanGoForward() override { }
    virtual void willChangeNetworkRequestsInProgress() override { }
    virtual void didChangeNetworkRequestsInProgress() override { }
    virtual void willChangeCertificateInfo() override { }
    virtual void didChangeCertificateInfo() override { }

    WebKitWebView* m_webView;
};

static gboolean webkitWebViewLoadFail(WebKitWebView* webView, WebKitLoadEvent, const char* failingURI, GError* error)
{
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)
        || g_error_matches(error, WEBKIT_PLUGIN_ERROR, WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD))
        return FALSE;

    GUniquePtr<char> htmlString(g_strdup_printf("<html><body>%s</body></html>", error->message));
    webkit_web_view_load_alternate_html(webView, htmlString.get(), failingURI, 0);

    return TRUE;
}

static GtkWidget* webkitWebViewCreate(WebKitWebView*, WebKitNavigationAction*)
{
    return nullptr;
}

static GtkWidget* webkitWebViewCreateJavaScriptDialog(WebKitWebView* webView, GtkMessageType type, GtkButtonsType buttons, int defaultResponse, const char* message)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    GtkWidget* dialog = gtk_message_dialog_new(widgetIsOnscreenToplevelWindow(parent) ? GTK_WINDOW(parent) : 0,
                                               GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, "%s", message);
    GUniquePtr<char> title(g_strdup_printf("JavaScript - %s", webkit_web_view_get_uri(webView)));
    gtk_window_set_title(GTK_WINDOW(dialog), title.get());
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), defaultResponse);

    return dialog;
}

static gboolean webkitWebViewScriptDialog(WebKitWebView* webView, WebKitScriptDialog* scriptDialog)
{
    GtkWidget* dialog = 0;

    switch (scriptDialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE, scriptDialog->message.data());
        gtk_dialog_run(GTK_DIALOG(dialog));
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, scriptDialog->message.data());
        scriptDialog->confirmed = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, scriptDialog->message.data());
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), scriptDialog->defaultText.data());
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_widget_show(entry);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
            scriptDialog->text = gtk_entry_get_text(GTK_ENTRY(entry));
        break;
    }

    gtk_widget_destroy(dialog);

    return TRUE;
}

static gboolean webkitWebViewDecidePolicy(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType decisionType)
{
    if (decisionType != WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
        webkit_policy_decision_use(decision);
        return TRUE;
    }

    WebKitURIResponse* response = webkit_response_policy_decision_get_response(WEBKIT_RESPONSE_POLICY_DECISION(decision));
    const ResourceResponse& resourceResponse = webkitURIResponseGetResourceResponse(response);
    if (resourceResponse.isAttachment()) {
        webkit_policy_decision_download(decision);
        return TRUE;
    }

    if (webkit_response_policy_decision_is_mime_type_supported(WEBKIT_RESPONSE_POLICY_DECISION(decision)))
        webkit_policy_decision_use(decision);
    else
        webkit_policy_decision_ignore(decision);

    return TRUE;
}

static gboolean webkitWebViewPermissionRequest(WebKitWebView*, WebKitPermissionRequest* request)
{
    webkit_permission_request_deny(request);
    return TRUE;
}

static void allowModalDialogsChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    if (!page)
        return;
    getPage(webView)->setCanRunModal(webkit_settings_get_allow_modal_dialogs(settings));
}

static void zoomTextOnlyChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    WebPageProxy* page = getPage(webView);
    gboolean zoomTextOnly = webkit_settings_get_zoom_text_only(settings);
    gdouble pageZoomLevel = zoomTextOnly ? 1 : page->textZoomFactor();
    gdouble textZoomLevel = zoomTextOnly ? page->pageZoomFactor() : 1;
    page->setPageAndTextZoomFactors(pageZoomLevel, textZoomLevel);
}

static void userAgentChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    getPage(webView)->setCustomUserAgent(String::fromUTF8(webkit_settings_get_user_agent(settings)));
}

static void webkitWebViewUpdateFavicon(WebKitWebView* webView, cairo_surface_t* favicon)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->favicon.get() == favicon)
        return;

    priv->favicon = favicon;
    g_object_notify(G_OBJECT(webView), "favicon");
}

static void webkitWebViewCancelFaviconRequest(WebKitWebView* webView)
{
    if (!webView->priv->faviconCancellable)
        return;

    g_cancellable_cancel(webView->priv->faviconCancellable.get());
    webView->priv->faviconCancellable = 0;
}

static void gotFaviconCallback(GObject* object, GAsyncResult* result, gpointer userData)
{
    GUniqueOutPtr<GError> error;
    RefPtr<cairo_surface_t> favicon = adoptRef(webkit_favicon_database_get_favicon_finish(WEBKIT_FAVICON_DATABASE(object), result, &error.outPtr()));
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    WebKitWebView* webView = WEBKIT_WEB_VIEW(userData);
    webkitWebViewUpdateFavicon(webView, favicon.get());
    webView->priv->faviconCancellable = 0;
}

static void webkitWebViewRequestFavicon(WebKitWebView* webView)
{
    webkitWebViewCancelFaviconRequest(webView);

    WebKitWebViewPrivate* priv = webView->priv;
    priv->faviconCancellable = adoptGRef(g_cancellable_new());
    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
    webkit_favicon_database_get_favicon(database, priv->activeURI.data(), priv->faviconCancellable.get(), gotFaviconCallback, webView);
}

static void webkitWebViewUpdateFaviconURI(WebKitWebView* webView, const char* faviconURI)
{
    if (webView->priv->faviconURI == faviconURI)
        return;

    webView->priv->faviconURI = faviconURI;
    webkitWebViewRequestFavicon(webView);
}

static void faviconChangedCallback(WebKitFaviconDatabase*, const char* pageURI, const char* faviconURI, WebKitWebView* webView)
{
    if (webView->priv->activeURI != pageURI)
        return;

    webkitWebViewUpdateFaviconURI(webView, faviconURI);
}

static void webkitWebViewUpdateSettings(WebKitWebView* webView)
{
    // The "settings" property is set on construction, and in that
    // case webkit_web_view_set_settings() will be called *before* the
    // WebPageProxy has been created so we should do an early return.
    WebPageProxy* page = getPage(webView);
    if (!page)
        return;

    WebKitSettings* settings = webView->priv->settings.get();
    page->setPreferences(*webkitSettingsGetPreferences(settings));
    page->setCanRunModal(webkit_settings_get_allow_modal_dialogs(settings));
    page->setCustomUserAgent(String::fromUTF8(webkit_settings_get_user_agent(settings)));

    g_signal_connect(settings, "notify::allow-modal-dialogs", G_CALLBACK(allowModalDialogsChanged), webView);
    g_signal_connect(settings, "notify::zoom-text-only", G_CALLBACK(zoomTextOnlyChanged), webView);
    g_signal_connect(settings, "notify::user-agent", G_CALLBACK(userAgentChanged), webView);
}

static void webkitWebViewDisconnectSettingsSignalHandlers(WebKitWebView* webView)
{
    WebKitSettings* settings = webView->priv->settings.get();
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(allowModalDialogsChanged), webView);
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(zoomTextOnlyChanged), webView);
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(userAgentChanged), webView);
}

static void webkitWebViewDisconnectMainResourceResponseChangedSignalHandler(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->mainResourceResponseHandlerID)
        g_signal_handler_disconnect(priv->mainResource.get(), priv->mainResourceResponseHandlerID);
    priv->mainResourceResponseHandlerID = 0;
}

static void webkitWebViewWatchForChangesInFavicon(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->faviconChangedHandlerID)
        return;

    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
    priv->faviconChangedHandlerID = g_signal_connect(database, "favicon-changed", G_CALLBACK(faviconChangedCallback), webView);
}

static void webkitWebViewDisconnectFaviconDatabaseSignalHandlers(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->faviconChangedHandlerID)
        g_signal_handler_disconnect(webkit_web_context_get_favicon_database(priv->context.get()), priv->faviconChangedHandlerID);
    priv->faviconChangedHandlerID = 0;
}

static gboolean webkitWebViewAuthenticate(WebKitWebView* webView, WebKitAuthenticationRequest* request)
{
    CredentialStorageMode credentialStorageMode = webkit_authentication_request_can_save_credentials(request) ? AllowPersistentStorage : DisallowPersistentStorage;
    webkitWebViewBaseAddAuthenticationDialog(WEBKIT_WEB_VIEW_BASE(webView), webkitAuthenticationDialogNew(request, credentialStorageMode));

    return TRUE;
}

static void fileChooserDialogResponseCallback(GtkDialog* dialog, gint responseID, WebKitFileChooserRequest* request)
{
    GRefPtr<WebKitFileChooserRequest> adoptedRequest = adoptGRef(request);
    if (responseID == GTK_RESPONSE_ACCEPT) {
        GUniquePtr<GSList> filesList(gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog)));
        GRefPtr<GPtrArray> filesArray = adoptGRef(g_ptr_array_new());
        for (GSList* file = filesList.get(); file; file = g_slist_next(file))
            g_ptr_array_add(filesArray.get(), file->data);
        g_ptr_array_add(filesArray.get(), 0);
        webkit_file_chooser_request_select_files(adoptedRequest.get(), reinterpret_cast<const gchar* const*>(filesArray->pdata));
    } else
        webkit_file_chooser_request_cancel(adoptedRequest.get());

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean webkitWebViewRunFileChooser(WebKitWebView* webView, WebKitFileChooserRequest* request)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    if (!widgetIsOnscreenToplevelWindow(toplevel))
        toplevel = 0;

    gboolean allowsMultipleSelection = webkit_file_chooser_request_get_select_multiple(request);
    GtkWidget* dialog = gtk_file_chooser_dialog_new(allowsMultipleSelection ? _("Select Files") : _("Select File"),
                                                    toplevel ? GTK_WINDOW(toplevel) : 0,
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                    NULL);

    if (GtkFileFilter* filter = webkit_file_chooser_request_get_mime_types_filter(request))
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), allowsMultipleSelection);

    if (const gchar* const* selectedFiles = webkit_file_chooser_request_get_selected_files(request))
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), selectedFiles[0]);

    g_signal_connect(dialog, "response", G_CALLBACK(fileChooserDialogResponseCallback), g_object_ref(request));
    gtk_widget_show(dialog);

    return TRUE;
}

static void webkitWebViewHandleDownloadRequest(WebKitWebViewBase* webViewBase, DownloadProxy* downloadProxy)
{
    GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
    webkitDownloadSetWebView(download.get(), WEBKIT_WEB_VIEW(webViewBase));
}

#if USE(LIBNOTIFY)
static const char* gNotifyNotificationID = "wk-notify-notification";

static void notifyNotificationClosed(NotifyNotification*, WebKitNotification* webNotification)
{
    g_object_set_data(G_OBJECT(webNotification), gNotifyNotificationID, nullptr);
    webkit_notification_close(webNotification);
}

static void webNotificationClosed(WebKitNotification* webNotification)
{
    NotifyNotification* notification = NOTIFY_NOTIFICATION(g_object_get_data(G_OBJECT(webNotification), gNotifyNotificationID));
    if (!notification)
        return;

    notify_notification_close(notification, nullptr);
    g_object_set_data(G_OBJECT(webNotification), gNotifyNotificationID, nullptr);
}
#endif // USE(LIBNOTIFY)

static gboolean webkitWebViewShowNotification(WebKitWebView*, WebKitNotification* webNotification)
{
#if USE(LIBNOTIFY)
    if (!notify_is_initted())
        notify_init(g_get_prgname());

    NotifyNotification* notification = NOTIFY_NOTIFICATION(g_object_get_data(G_OBJECT(webNotification), gNotifyNotificationID));
    if (!notification) {
        notification = notify_notification_new(webkit_notification_get_title(webNotification),
            webkit_notification_get_body(webNotification), nullptr);

        g_signal_connect_object(notification, "closed", G_CALLBACK(notifyNotificationClosed), webNotification, static_cast<GConnectFlags>(0));
        g_signal_connect(webNotification, "closed", G_CALLBACK(webNotificationClosed), nullptr);
        g_object_set_data_full(G_OBJECT(webNotification), gNotifyNotificationID, notification, static_cast<GDestroyNotify>(g_object_unref));
    } else {
        notify_notification_update(notification, webkit_notification_get_title(webNotification),
            webkit_notification_get_body(webNotification), nullptr);
    }

    notify_notification_show(notification, nullptr);
    return TRUE;
#else
    UNUSED_PARAM(webNotification);
    return FALSE;
#endif
}

static void webkitWebViewConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_parent_class)->constructed(object);

    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->relatedView)
        priv->context = webkit_web_view_get_context(priv->relatedView);
    else if (!priv->context)
        priv->context = webkit_web_context_get_default();
    if (!priv->settings)
        priv->settings = adoptGRef(webkit_settings_new());

    webkitWebContextCreatePageForWebView(priv->context.get(), webView, priv->userContentManager.get(), priv->relatedView);

    priv->loadObserver = std::make_unique<PageLoadStateObserver>(webView);
    getPage(webView)->pageLoadState().addObserver(*priv->loadObserver);

    // The related view is only valid during the construction.
    priv->relatedView = nullptr;

    webkitWebViewBaseSetDownloadRequestHandler(WEBKIT_WEB_VIEW_BASE(webView), webkitWebViewHandleDownloadRequest);

    attachLoaderClientToView(webView);
    attachUIClientToView(webView);
    attachPolicyClientToView(webView);
    attachFullScreenClientToView(webView);
    attachContextMenuClientToView(webView);
    attachFormClientToView(webView);

    // This needs to be after attachUIClientToView() because WebPageProxy::setUIClient() calls setCanRunModal() with true.
    // See https://bugs.webkit.org/show_bug.cgi?id=135412.
    webkitWebViewUpdateSettings(webView);

    priv->backForwardList = adoptGRef(webkitBackForwardListCreate(&getPage(webView)->backForwardList()));
    priv->windowProperties = adoptGRef(webkitWindowPropertiesCreate());
}

static void webkitWebViewSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT: {
        gpointer webContext = g_value_get_object(value);
        webView->priv->context = webContext ? WEBKIT_WEB_CONTEXT(webContext) : nullptr;
        break;
    }
    case PROP_RELATED_VIEW: {
        gpointer relatedView = g_value_get_object(value);
        webView->priv->relatedView = relatedView ? WEBKIT_WEB_VIEW(relatedView) : nullptr;
        break;
    }
    case PROP_SETTINGS: {
        if (gpointer settings = g_value_get_object(value))
            webkit_web_view_set_settings(webView, WEBKIT_SETTINGS(settings));
        break;
    }
    case PROP_USER_CONTENT_MANAGER: {
        gpointer userContentManager = g_value_get_object(value);
        webView->priv->userContentManager = userContentManager ? WEBKIT_USER_CONTENT_MANAGER(userContentManager) : nullptr;
        break;
    }
    case PROP_ZOOM_LEVEL:
        webkit_web_view_set_zoom_level(webView, g_value_get_double(value));
        break;
    case PROP_EDITABLE:
        webkit_web_view_set_editable(webView, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
    case PROP_WEB_CONTEXT:
        g_value_set_object(value, webView->priv->context.get());
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_view_get_settings(webView));
        break;
    case PROP_USER_CONTENT_MANAGER:
        g_value_set_object(value, webkit_web_view_get_user_content_manager(webView));
        break;
    case PROP_TITLE:
        g_value_set_string(value, webView->priv->title.data());
        break;
    case PROP_ESTIMATED_LOAD_PROGRESS:
        g_value_set_double(value, webkit_web_view_get_estimated_load_progress(webView));
        break;
    case PROP_FAVICON:
        g_value_set_pointer(value, webkit_web_view_get_favicon(webView));
        break;
    case PROP_URI:
        g_value_set_string(value, webkit_web_view_get_uri(webView));
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_double(value, webkit_web_view_get_zoom_level(webView));
        break;
    case PROP_IS_LOADING:
        g_value_set_boolean(value, webkit_web_view_is_loading(webView));
        break;
    case PROP_IS_PLAYING_AUDIO:
        g_value_set_boolean(value, webkit_web_view_is_playing_audio(webView));
        break;
    case PROP_EDITABLE:
        g_value_set_boolean(value, webkit_web_view_is_editable(webView));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewDispose(GObject* object)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    webkitWebViewCancelFaviconRequest(webView);
    webkitWebViewDisconnectMainResourceResponseChangedSignalHandler(webView);
    webkitWebViewDisconnectSettingsSignalHandlers(webView);
    webkitWebViewDisconnectFaviconDatabaseSignalHandlers(webView);

    if (webView->priv->loadObserver) {
        getPage(webView)->pageLoadState().removeObserver(*webView->priv->loadObserver);
        webView->priv->loadObserver.reset();
    }

    webkitWebContextWebViewDestroyed(webView->priv->context.get(), webView);

    G_OBJECT_CLASS(webkit_web_view_parent_class)->dispose(object);
}

static gboolean webkitWebViewAccumulatorObjectHandled(GSignalInvocationHint*, GValue* returnValue, const GValue* handlerReturn, gpointer)
{
    void* object = g_value_get_object(handlerReturn);
    if (object)
        g_value_set_object(returnValue, object);

    return !object;
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webViewClass);

    gObjectClass->constructed = webkitWebViewConstructed;
    gObjectClass->set_property = webkitWebViewSetProperty;
    gObjectClass->get_property = webkitWebViewGetProperty;
    gObjectClass->dispose = webkitWebViewDispose;

    webViewClass->load_failed = webkitWebViewLoadFail;
    webViewClass->create = webkitWebViewCreate;
    webViewClass->script_dialog = webkitWebViewScriptDialog;
    webViewClass->decide_policy = webkitWebViewDecidePolicy;
    webViewClass->permission_request = webkitWebViewPermissionRequest;
    webViewClass->run_file_chooser = webkitWebViewRunFileChooser;
    webViewClass->authenticate = webkitWebViewAuthenticate;
    webViewClass->show_notification = webkitWebViewShowNotification;

    /**
     * WebKitWebView:web-context:
     *
     * The #WebKitWebContext of the view.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_WEB_CONTEXT,
                                    g_param_spec_object("web-context",
                                                        _("Web Context"),
                                                        _("The web context for the view"),
                                                        WEBKIT_TYPE_WEB_CONTEXT,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    /**
     * WebKitWebView:related-view:
     *
     * The related #WebKitWebView used when creating the view to share the
     * same web process. This property is not readable because the related
     * web view is only valid during the object construction.
     *
     * Since: 2.4
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_RELATED_VIEW,
        g_param_spec_object(
            "related-view",
            _("Related WebView"),
            _("The related WebKitWebView used when creating the view to share the same web process"),
            WEBKIT_TYPE_WEB_VIEW,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebView:settings:
     *
     * The #WebKitSettings of the view.
     *
     * Since: 2.6
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_SETTINGS,
        g_param_spec_object(
            "settings",
            _("WebView settings"),
            _("The WebKitSettings of the view"),
            WEBKIT_TYPE_SETTINGS,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT)));

    /**
     * WebKitWebView:user-content-manager:
     *
     * The #WebKitUserContentManager of the view.
     *
     * Since: 2.6
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_USER_CONTENT_MANAGER,
        g_param_spec_object(
            "user-content-manager",
            _("WebView user content manager"),
            _("The WebKitUserContentManager of the view"),
            WEBKIT_TYPE_USER_CONTENT_MANAGER,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebView:title:
     *
     * The main frame document title of this #WebKitWebView. If
     * the title has not been received yet, it will be %NULL.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                                        _("Title"),
                                                        _("Main frame document title"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:estimated-load-progress:
     *
     * An estimate of the percent completion for the current loading operation.
     * This value will range from 0.0 to 1.0 and, once a load completes,
     * will remain at 1.0 until a new load starts, at which point it
     * will be reset to 0.0.
     * The value is an estimate based on the total number of bytes expected
     * to be received for a document, including all its possible subresources
     * and child documents.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_ESTIMATED_LOAD_PROGRESS,
                                    g_param_spec_double("estimated-load-progress",
                                                        _("Estimated Load Progress"),
                                                        _("An estimate of the percent completion for a document load"),
                                                        0.0, 1.0, 0.0,
                                                        WEBKIT_PARAM_READABLE));
    /**
     * WebKitWebView:favicon:
     *
     * The favicon currently associated to the #WebKitWebView.
     * See webkit_web_view_get_favicon() for more details.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_FAVICON,
                                    g_param_spec_pointer("favicon",
                                                         _("Favicon"),
                                                         _("The favicon associated to the view, if any"),
                                                         WEBKIT_PARAM_READABLE));
    /**
     * WebKitWebView:uri:
     *
     * The current active URI of the #WebKitWebView.
     * See webkit_web_view_get_uri() for more details.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("The current active URI of the view"),
                                                        0,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:zoom-level:
     *
     * The zoom level of the #WebKitWebView content.
     * See webkit_web_view_set_zoom_level() for more details.
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_ZOOM_LEVEL,
        g_param_spec_double(
            "zoom-level",
            _("Zoom level"),
            _("The zoom level of the view content"),
            0, G_MAXDOUBLE, 1,
            WEBKIT_PARAM_READWRITE));

    /**
     * WebKitWebView:is-loading:
     *
     * Whether the #WebKitWebView is currently loading a page. This property becomes
     * %TRUE as soon as a new load operation is requested and before the
     * #WebKitWebView::load-changed signal is emitted with %WEBKIT_LOAD_STARTED and
     * at that point the active URI is the requested one.
     * When the load operation finishes the property is set to %FALSE before
     * #WebKitWebView::load-changed is emitted with %WEBKIT_LOAD_FINISHED.
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_IS_LOADING,
        g_param_spec_boolean(
            "is-loading",
            _("Is Loading"),
            _("Whether the view is loading a page"),
            FALSE,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:is-playing-audio:
     *
     * Whether the #WebKitWebView is currently playing audio from a page.
     * This property becomes %TRUE as soon as web content starts playing any
     * kind of audio. When a page is no longer playing any kind of sound,
     * the property is set back to %FALSE.
     *
     * Since: 2.8
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_IS_PLAYING_AUDIO,
        g_param_spec_boolean(
            "is-playing-audio",
            "Is Playing Audio",
            _("Whether the view is playing audio"),
            FALSE,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:editable:
     *
     * Whether the pages loaded inside #WebKitWebView are editable. For more
     * information see webkit_web_view_set_editable().
     *
     * Since: 2.8
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_EDITABLE,
        g_param_spec_boolean(
            "editable",
            _("Editable"),
            _("Whether the content can be modified by the user."),
            FALSE,
            WEBKIT_PARAM_READWRITE));

    /**
     * WebKitWebView::load-changed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent
     *
     * Emitted when the a load operation in @web_view changes.
     * The signal is always emitted with %WEBKIT_LOAD_STARTED when a
     * new load request is made and %WEBKIT_LOAD_FINISHED when the load
     * finishes successfully or due to an error. When the ongoing load
     * operation fails #WebKitWebView::load-failed signal is emitted
     * before #WebKitWebView::load-changed is emitted with
     * %WEBKIT_LOAD_FINISHED.
     * If a redirection is received from the server, this signal is emitted
     * with %WEBKIT_LOAD_REDIRECTED after the initial emission with
     * %WEBKIT_LOAD_STARTED and before %WEBKIT_LOAD_COMMITTED.
     * When the page content starts arriving the signal is emitted with
     * %WEBKIT_LOAD_COMMITTED event.
     *
     * You can handle this signal and use a switch to track any ongoing
     * load operation.
     *
     * <informalexample><programlisting>
     * static void web_view_load_changed (WebKitWebView  *web_view,
     *                                    WebKitLoadEvent load_event,
     *                                    gpointer        user_data)
     * {
     *     switch (load_event) {
     *     case WEBKIT_LOAD_STARTED:
     *         /<!-- -->* New load, we have now a provisional URI *<!-- -->/
     *         provisional_uri = webkit_web_view_get_uri (web_view);
     *         /<!-- -->* Here we could start a spinner or update the
     *          <!-- -->* location bar with the provisional URI *<!-- -->/
     *         break;
     *     case WEBKIT_LOAD_REDIRECTED:
     *         redirected_uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_COMMITTED:
     *         /<!-- -->* The load is being performed. Current URI is
     *          <!-- -->* the final one and it won't change unless a new
     *          <!-- -->* load is requested or a navigation within the
     *          <!-- -->* same page is performed *<!-- -->/
     *         uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_FINISHED:
     *         /<!-- -->* Load finished, we can now stop the spinner *<!-- -->/
     *         break;
     *     }
     * }
     * </programlisting></informalexample>
     */
    signals[LOAD_CHANGED] =
        g_signal_new("load-changed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, load_changed),
                     0, 0,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_LOAD_EVENT);

    /**
     * WebKitWebView::load-failed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent of the load operation
     * @failing_uri: the URI that failed to load
     * @error: the #GError that was triggered
     *
     * Emitted when an error occurs during a load operation.
     * If the error happened when starting to load data for a page
     * @load_event will be %WEBKIT_LOAD_STARTED. If it happened while
     * loading a committed data source @load_event will be %WEBKIT_LOAD_COMMITTED.
     * Since a load error causes the load operation to finish, the signal
     * WebKitWebView::load-changed will always be emitted with
     * %WEBKIT_LOAD_FINISHED event right after this one.
     *
     * By default, if the signal is not handled, a stock error page will be displayed.
     * You need to handle the signal if you want to provide your own error page.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[LOAD_FAILED] =
        g_signal_new("load-failed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, load_failed),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__ENUM_STRING_POINTER,
                     G_TYPE_BOOLEAN, 3,
                     WEBKIT_TYPE_LOAD_EVENT,
                     G_TYPE_STRING,
                     G_TYPE_POINTER);

    /**
     * WebKitWebView::load-failed-with-tls-errors:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @failing_uri: the URI that failed to load
     * @certificate: a #GTlsCertificate
     * @errors: a #GTlsCertificateFlags with the verification status of @certificate
     *
     * Emitted when a TLS error occurs during a load operation.
     * To allow an exception for this @certificate
     * and the host of @failing_uri use webkit_web_context_allow_tls_certificate_for_host().
     *
     * To handle this signal asynchronously you should call g_object_ref() on @certificate
     * and return %TRUE.
     *
     * If %FALSE is returned, #WebKitWebView::load-failed will be emitted. The load
     * will finish regardless of the returned value.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.6
     */
    signals[LOAD_FAILED_WITH_TLS_ERRORS] =
        g_signal_new("load-failed-with-tls-errors",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, load_failed_with_tls_errors),
            g_signal_accumulator_true_handled, 0 /* accumulator data */,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN, 3,
            G_TYPE_STRING,
            G_TYPE_TLS_CERTIFICATE,
            G_TYPE_TLS_CERTIFICATE_FLAGS);

    /**
     * WebKitWebView::create:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @navigation_action: a #WebKitNavigationAction
     *
     * Emitted when the creation of a new #WebKitWebView is requested.
     * If this signal is handled the signal handler should return the
     * newly created #WebKitWebView.
     *
     * The #WebKitNavigationAction parameter contains information about the
     * navigation action that triggered this signal.
     *
     * When using %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES
     * process model, the new #WebKitWebView should be related to
     * @web_view to share the same web process, see webkit_web_view_new_with_related_view()
     * for more details.
     *
     * The new #WebKitWebView should not be displayed to the user
     * until the #WebKitWebView::ready-to-show signal is emitted.
     *
     * Returns: (transfer full): a newly allocated #WebKitWebView widget
     *    or %NULL to propagate the event further.
     */
    signals[CREATE] = g_signal_new(
        "create",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, create),
        webkitWebViewAccumulatorObjectHandled, 0,
        g_cclosure_marshal_generic,
        GTK_TYPE_WIDGET, 1,
        WEBKIT_TYPE_NAVIGATION_ACTION | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitWebView::ready-to-show:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::create on the newly created #WebKitWebView
     * when it should be displayed to the user. When this signal is emitted
     * all the information about how the window should look, including
     * size, position, whether the location, status and scrollbars
     * should be displayed, is already set on the #WebKitWindowProperties
     * of @web_view. See also webkit_web_view_get_window_properties().
     */
    signals[READY_TO_SHOW] =
        g_signal_new("ready-to-show",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, ready_to_show),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

     /**
     * WebKitWebView::run-as-modal:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::ready-to-show on the newly
     * created #WebKitWebView when JavaScript code calls
     * <function>window.showModalDialog</function>. The purpose of
     * this signal is to allow the client application to prepare the
     * new view to behave as modal. Once the signal is emitted a new
     * mainloop will be run to block user interaction in the parent
     * #WebKitWebView until the new dialog is closed.
     */
    signals[RUN_AS_MODAL] =
            g_signal_new("run-as-modal",
                         G_TYPE_FROM_CLASS(webViewClass),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET(WebKitWebViewClass, run_as_modal),
                         0, 0,
                         g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0);

    /**
     * WebKitWebView::close:
     * @webView: the #WebKitWebView on which the signal is emitted
     *
     * Emitted when closing a #WebKitWebView is requested. This occurs when a
     * call is made from JavaScript's <function>window.close</function> function.
     * It is the owner's responsibility to handle this signal to hide or
     * destroy the #WebKitWebView, if necessary.
     */
    signals[CLOSE] =
        g_signal_new("close",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, close),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::script-dialog:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @dialog: the #WebKitScriptDialog to show
     *
     * Emitted when JavaScript code calls <function>window.alert</function>,
     * <function>window.confirm</function> or <function>window.prompt</function>.
     * The @dialog parameter should be used to build the dialog.
     * If the signal is not handled a different dialog will be built and shown depending
     * on the dialog type:
     * <itemizedlist>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_ALERT: message dialog with a single Close button.
     * </para></listitem>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_CONFIRM: message dialog with OK and Cancel buttons.
     * </para></listitem>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_PROMPT: message dialog with OK and Cancel buttons and
     *  a text entry with the default text.
     * </para></listitem>
     * </itemizedlist>
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[SCRIPT_DIALOG] =
        g_signal_new("script-dialog",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, script_dialog),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__BOXED,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_SCRIPT_DIALOG | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitWebView::decide-policy:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @decision: the #WebKitPolicyDecision
     * @decision_type: a #WebKitPolicyDecisionType denoting the type of @decision
     *
     * This signal is emitted when WebKit is requesting the client to decide a policy
     * decision, such as whether to navigate to a page, open a new window or whether or
     * not to download a resource. The #WebKitNavigationPolicyDecision passed in the
     * @decision argument is a generic type, but should be casted to a more
     * specific type when making the decision. For example:
     *
     * <informalexample><programlisting>
     * static gboolean
     * decide_policy_cb (WebKitWebView *web_view,
     *                   WebKitPolicyDecision *decision,
     *                   WebKitPolicyDecisionType type)
     * {
     *     switch (type) {
     *     case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
     *         WebKitResponsePolicyDecision *response = WEBKIT_RESPONSE_POLICY_DECISION (decision);
     *         /<!-- -->* Make a policy decision here. *<!-- -->/
     *         break;
     *     default:
     *         /<!-- -->* Making no decision results in webkit_policy_decision_use(). *<!-- -->/
     *         return FALSE;
     *     }
     *     return TRUE;
     * }
     * </programlisting></informalexample>
     *
     * It is possible to make policy decision asynchronously, by simply calling g_object_ref()
     * on the @decision argument and returning %TRUE to block the default signal handler.
     * If the last reference is removed on a #WebKitPolicyDecision and no decision has been
     * made explicitly, webkit_policy_decision_use() will be the default policy decision. The
     * default signal handler will simply call webkit_policy_decision_use(). Only the first
     * policy decision chosen for a given #WebKitPolicyDecision will have any affect.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[DECIDE_POLICY] =
        g_signal_new("decide-policy",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, decide_policy),
                     g_signal_accumulator_true_handled, 0 /* accumulator data */,
                     webkit_marshal_BOOLEAN__OBJECT_ENUM,
                     G_TYPE_BOOLEAN, 2, /* number of parameters */
                     WEBKIT_TYPE_POLICY_DECISION,
                     WEBKIT_TYPE_POLICY_DECISION_TYPE);

    /**
     * WebKitWebView::permission-request:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: the #WebKitPermissionRequest
     *
     * This signal is emitted when WebKit is requesting the client to
     * decide about a permission request, such as allowing the browser
     * to switch to fullscreen mode, sharing its location or similar
     * operations.
     *
     * A possible way to use this signal could be through a dialog
     * allowing the user decide what to do with the request:
     *
     * <informalexample><programlisting>
     * static gboolean permission_request_cb (WebKitWebView *web_view,
     *                                        WebKitPermissionRequest *request,
     *                                        GtkWindow *parent_window)
     * {
     *     GtkWidget *dialog = gtk_message_dialog_new (parent_window,
     *                                                 GTK_DIALOG_MODAL,
     *                                                 GTK_MESSAGE_QUESTION,
     *                                                 GTK_BUTTONS_YES_NO,
     *                                                 "Allow Permission Request?");
     *     gtk_widget_show (dialog);
     *     gint result = gtk_dialog_run (GTK_DIALOG (dialog));
     *
     *     switch (result) {
     *     case GTK_RESPONSE_YES:
     *         webkit_permission_request_allow (request);
     *         break;
     *     default:
     *         webkit_permission_request_deny (request);
     *         break;
     *     }
     *     gtk_widget_destroy (dialog);
     *
     *     return TRUE;
     * }
     * </programlisting></informalexample>
     *
     * It is possible to handle permission requests asynchronously, by
     * simply calling g_object_ref() on the @request argument and
     * returning %TRUE to block the default signal handler.  If the
     * last reference is removed on a #WebKitPermissionRequest and the
     * request has not been handled, webkit_permission_request_deny()
     * will be the default action.
     *
     * By default, if the signal is not handled,
     * webkit_permission_request_deny() will be called over the
     * #WebKitPermissionRequest.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[PERMISSION_REQUEST] =
        g_signal_new("permission-request",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, permission_request),
                     g_signal_accumulator_true_handled, 0 /* accumulator data */,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1, /* number of parameters */
                     WEBKIT_TYPE_PERMISSION_REQUEST);
    /**
     * WebKitWebView::mouse-target-changed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @hit_test_result: a #WebKitHitTestResult
     * @modifiers: a bitmask of #GdkModifierType
     *
     * This signal is emitted when the mouse cursor moves over an
     * element such as a link, image or a media element. To determine
     * what type of element the mouse cursor is over, a Hit Test is performed
     * on the current mouse coordinates and the result is passed in the
     * @hit_test_result argument. The @modifiers argument is a bitmask of
     * #GdkModifierType flags indicating the state of modifier keys.
     * The signal is emitted again when the mouse is moved out of the
     * current element with a new @hit_test_result.
     */
     signals[MOUSE_TARGET_CHANGED] =
         g_signal_new("mouse-target-changed",
                      G_TYPE_FROM_CLASS(webViewClass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(WebKitWebViewClass, mouse_target_changed),
                      0, 0,
                      webkit_marshal_VOID__OBJECT_UINT,
                      G_TYPE_NONE, 2,
                      WEBKIT_TYPE_HIT_TEST_RESULT,
                      G_TYPE_UINT);
    /**
     * WebKitWebView::print:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @print_operation: the #WebKitPrintOperation that will handle the print request
     *
     * Emitted when printing is requested on @web_view, usually by a javascript call,
     * before the print dialog is shown. This signal can be used to set the initial
     * print settings and page setup of @print_operation to be used as default values in
     * the print dialog. You can call webkit_print_operation_set_print_settings() and
     * webkit_print_operation_set_page_setup() and then return %FALSE to propagate the
     * event so that the print dialog is shown.
     *
     * You can connect to this signal and return %TRUE to cancel the print operation
     * or implement your own print dialog.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[PRINT] =
        g_signal_new("print",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, print),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1,
                     WEBKIT_TYPE_PRINT_OPERATION);

    /**
     * WebKitWebView::resource-load-started:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @resource: a #WebKitWebResource
     * @request: a #WebKitURIRequest
     *
     * Emitted when a new resource is going to be loaded. The @request parameter
     * contains the #WebKitURIRequest that will be sent to the server.
     * You can monitor the load operation by connecting to the different signals
     * of @resource.
     */
    signals[RESOURCE_LOAD_STARTED] =
        g_signal_new("resource-load-started",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, resource_load_started),
                     0, 0,
                     webkit_marshal_VOID__OBJECT_OBJECT,
                     G_TYPE_NONE, 2,
                     WEBKIT_TYPE_WEB_RESOURCE,
                     WEBKIT_TYPE_URI_REQUEST);

    /**
     * WebKitWebView::enter-fullscreen:
     * @web_view: the #WebKitWebView on which the signal is emitted.
     *
     * Emitted when JavaScript code calls
     * <function>element.webkitRequestFullScreen</function>. If the
     * signal is not handled the #WebKitWebView will proceed to full screen
     * its top level window. This signal can be used by client code to
     * request permission to the user prior doing the full screen
     * transition and eventually prepare the top-level window
     * (e.g. hide some widgets that would otherwise be part of the
     * full screen window).
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[ENTER_FULLSCREEN] =
        g_signal_new("enter-fullscreen",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, enter_fullscreen),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebView::leave-fullscreen:
     * @web_view: the #WebKitWebView on which the signal is emitted.
     *
     * Emitted when the #WebKitWebView is about to restore its top level
     * window out of its full screen state. This signal can be used by
     * client code to restore widgets hidden during the
     * #WebKitWebView::enter-fullscreen stage for instance.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[LEAVE_FULLSCREEN] =
        g_signal_new("leave-fullscreen",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, leave_fullscreen),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0);
     /**
     * WebKitWebView::run-file-chooser:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitFileChooserRequest
     *
     * This signal is emitted when the user interacts with a &lt;input
     * type='file' /&gt; HTML element, requesting from WebKit to show
     * a dialog to select one or more files to be uploaded. To let the
     * application know the details of the file chooser, as well as to
     * allow the client application to either cancel the request or
     * perform an actual selection of files, the signal will pass an
     * instance of the #WebKitFileChooserRequest in the @request
     * argument.
     *
     * The default signal handler will asynchronously run a regular
     * #GtkFileChooserDialog for the user to interact with.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[RUN_FILE_CHOOSER] =
        g_signal_new("run-file-chooser",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, run_file_chooser),
                     g_signal_accumulator_true_handled, 0 /* accumulator data */,
                     webkit_marshal_BOOLEAN__OBJECT,
                     G_TYPE_BOOLEAN, 1, /* number of parameters */
                     WEBKIT_TYPE_FILE_CHOOSER_REQUEST);

    /**
     * WebKitWebView::context-menu:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @context_menu: the proposed #WebKitContextMenu
     * @event: the #GdkEvent that triggered the context menu
     * @hit_test_result: a #WebKitHitTestResult
     *
     * Emmited when a context menu is about to be displayed to give the application
     * a chance to customize the proposed menu, prevent the menu from being displayed
     * or build its own context menu.
     * <itemizedlist>
     * <listitem><para>
     *  To customize the proposed menu you can use webkit_context_menu_prepend(),
     *  webkit_context_menu_append() or webkit_context_menu_insert() to add new
     *  #WebKitContextMenuItem<!-- -->s to @context_menu, webkit_context_menu_move_item()
     *  to reorder existing items, or webkit_context_menu_remove() to remove an
     *  existing item. The signal handler should return %FALSE, and the menu represented
     *  by @context_menu will be shown.
     * </para></listitem>
     * <listitem><para>
     *  To prevent the menu from being displayed you can just connect to this signal
     *  and return %TRUE so that the proposed menu will not be shown.
     * </para></listitem>
     * <listitem><para>
     *  To build your own menu, you can remove all items from the proposed menu with
     *  webkit_context_menu_remove_all(), add your own items and return %FALSE so
     *  that the menu will be shown. You can also ignore the proposed #WebKitContextMenu,
     *  build your own #GtkMenu and return %TRUE to prevent the proposed menu from being shown.
     * </para></listitem>
     * <listitem><para>
     *  If you just want the default menu to be shown always, simply don't connect to this
     *  signal because showing the proposed context menu is the default behaviour.
     * </para></listitem>
     * </itemizedlist>
     *
     * If the signal handler returns %FALSE the context menu represented by @context_menu
     * will be shown, if it return %TRUE the context menu will not be shown.
     *
     * The proposed #WebKitContextMenu passed in @context_menu argument is only valid
     * during the signal emission.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[CONTEXT_MENU] =
        g_signal_new("context-menu",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, context_menu),
                     g_signal_accumulator_true_handled, 0,
                     webkit_marshal_BOOLEAN__OBJECT_BOXED_OBJECT,
                     G_TYPE_BOOLEAN, 3,
                     WEBKIT_TYPE_CONTEXT_MENU,
                     GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
                     WEBKIT_TYPE_HIT_TEST_RESULT);

    /**
     * WebKitWebView::context-menu-dismissed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::context-menu signal, if the context menu is shown,
     * to notify that the context menu is dismissed.
     */
    signals[CONTEXT_MENU_DISMISSED] =
        g_signal_new("context-menu-dismissed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, context_menu_dismissed),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::submit-form:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitFormSubmissionRequest
     *
     * This signal is emitted when a form is about to be submitted. The @request
     * argument passed contains information about the text fields of the form. This
     * is typically used to store login information that can be used later to
     * pre-fill the form.
     * The form will not be submitted until webkit_form_submission_request_submit() is called.
     *
     * It is possible to handle the form submission request asynchronously, by
     * simply calling g_object_ref() on the @request argument and calling
     * webkit_form_submission_request_submit() when done to continue with the form submission.
     * If the last reference is removed on a #WebKitFormSubmissionRequest and the
     * form has not been submitted, webkit_form_submission_request_submit() will be called.
     */
    signals[SUBMIT_FORM] =
        g_signal_new("submit-form",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, submit_form),
                     0, 0,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_FORM_SUBMISSION_REQUEST);

    /**
     * WebKitWebView::insecure-content-detected:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @event: the #WebKitInsecureContentEvent
     *
     * This signal is emitted when insecure content has been detected
     * in a page loaded through a secure connection. This typically
     * means that a external resource from an unstrusted source has
     * been run or displayed, resulting in a mix of HTTPS and
     * non-HTTPS content.
     *
     * You can check the @event parameter to know exactly which kind
     * of event has been detected (see #WebKitInsecureContentEvent).
     */
    signals[INSECURE_CONTENT_DETECTED] =
        g_signal_new("insecure-content-detected",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, insecure_content_detected),
            0, 0,
            g_cclosure_marshal_VOID__ENUM,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_INSECURE_CONTENT_EVENT);

    /**
     * WebKitWebView::web-process-crashed:
     * @web_view: the #WebKitWebView
     *
     * This signal is emitted when the web process crashes.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[WEB_PROCESS_CRASHED] = g_signal_new(
        "web-process-crashed",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, web_process_crashed),
        g_signal_accumulator_true_handled,
        0,
        webkit_marshal_BOOLEAN__VOID,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebView::authenticate:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitAuthenticationRequest
     *
     * This signal is emitted when the user is challenged with HTTP
     * authentication. To let the  application access or supply
     * the credentials as well as to allow the client application
     * to either cancel the request or perform the authentication,
     * the signal will pass an instance of the
     * #WebKitAuthenticationRequest in the @request argument.
     * To handle this signal asynchronously you should keep a ref
     * of the request and return %TRUE. To disable HTTP authentication
     * entirely, connect to this signal and simply return %TRUE.
     *
     * The default signal handler will run a default authentication
     * dialog asynchronously for the user to interact with.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.2
     */
    signals[AUTHENTICATE] =
        g_signal_new("authenticate",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, authenticate),
            g_signal_accumulator_true_handled, 0 /* accumulator data */,
            webkit_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN, 1, /* number of parameters */
            WEBKIT_TYPE_AUTHENTICATION_REQUEST);

    /**
     * WebKitWebView::show-notification:
     * @web_view: the #WebKitWebView
     * @notification: a #WebKitNotification
     *
     * This signal is emitted when a notification should be presented to the
     * user. The @notification is kept alive until either: 1) the web page cancels it
     * or 2) a navigation happens.
     *
     * The default handler will emit a notification using libnotify, if built with
     * support for it.
     *
     * Returns: %TRUE to stop other handlers from being invoked. %FALSE otherwise.
     *
     * Since: 2.8
     */
    signals[SHOW_NOTIFICATION] =
        g_signal_new("show-notification",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, show_notification),
            g_signal_accumulator_true_handled, nullptr /* accumulator data */,
            webkit_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN, 1,
            WEBKIT_TYPE_NOTIFICATION);

     /**
      * WebKitWebView::run-color-chooser:
      * @web_view: the #WebKitWebView on which the signal is emitted
      * @request: a #WebKitColorChooserRequest
      *
      * This signal is emitted when the user interacts with a &lt;input
      * type='color' /&gt; HTML element, requesting from WebKit to show
      * a dialog to select a color. To let the application know the details of
      * the color chooser, as well as to allow the client application to either
      * cancel the request or perform an actual color selection, the signal will
      * pass an instance of the #WebKitColorChooserRequest in the @request
      * argument.
      *
      * It is possible to handle this request asynchronously by increasing the
      * reference count of the request.
      *
      * The default signal handler will asynchronously run a regular
      * #GtkColorChooser for the user to interact with.
      *
      * Returns: %TRUE to stop other handlers from being invoked for the event.
      *   %FALSE to propagate the event further.
      *
      * Since: 2.8
      */
    signals[RUN_COLOR_CHOOSER] =
        g_signal_new("run-color-chooser",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, run_color_chooser),
            g_signal_accumulator_true_handled, nullptr,
            webkit_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN, 1,
            WEBKIT_TYPE_COLOR_CHOOSER_REQUEST);
}

static void webkitWebViewCancelAuthenticationRequest(WebKitWebView* webView)
{
    if (!webView->priv->authenticationRequest)
        return;

    webkit_authentication_request_cancel(webView->priv->authenticationRequest.get());
    webView->priv->authenticationRequest.clear();
}

static void webkitWebViewEmitLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent, bool isDelayedEvent)
{
    if (loadEvent == WEBKIT_LOAD_STARTED) {
        webkitWebViewWatchForChangesInFavicon(webView);
        webkitWebViewCancelAuthenticationRequest(webView);
    } else if (loadEvent == WEBKIT_LOAD_FINISHED) {
        if (isDelayedEvent) {
            // In case of the delayed event, we need to manually set is-loading to false.
            webkitWebViewSetIsLoading(webView, false);
        }
        webkitWebViewCancelAuthenticationRequest(webView);
        webkitWebViewDisconnectMainResourceResponseChangedSignalHandler(webView);
    }

    g_signal_emit(webView, signals[LOAD_CHANGED], 0, loadEvent);

    if (isDelayedEvent) {
        if (loadEvent == WEBKIT_LOAD_COMMITTED)
            webView->priv->waitingForMainResource = false;
        else if (loadEvent == WEBKIT_LOAD_FINISHED) {
            // Manually set is-loading again in case a new load was started.
            webkitWebViewSetIsLoading(webView, getPage(webView)->pageLoadState().isLoading());
        }
    }
}

static void webkitWebViewEmitDelayedLoadEvents(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (!priv->waitingForMainResource)
        return;
    ASSERT(priv->lastDelayedEvent == WEBKIT_LOAD_COMMITTED || priv->lastDelayedEvent == WEBKIT_LOAD_FINISHED);

    if (priv->lastDelayedEvent == WEBKIT_LOAD_FINISHED)
        webkitWebViewEmitLoadChanged(webView, WEBKIT_LOAD_COMMITTED, true);
    webkitWebViewEmitLoadChanged(webView, priv->lastDelayedEvent, true);
}

void webkitWebViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (loadEvent == WEBKIT_LOAD_STARTED) {
        // Finish a possible previous load waiting for main resource.
        webkitWebViewEmitDelayedLoadEvents(webView);

        webkitWebViewCancelFaviconRequest(webView);
        priv->loadingResourcesMap.clear();
        priv->mainResource = 0;
        priv->waitingForMainResource = false;
    } else if (loadEvent == WEBKIT_LOAD_COMMITTED) {
        WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
        GUniquePtr<char> faviconURI(webkit_favicon_database_get_favicon_uri(database, priv->activeURI.data()));
        webkitWebViewUpdateFaviconURI(webView, faviconURI.get());

        if (!priv->mainResource) {
            // When a page is loaded from the history cache, the main resource load callbacks
            // are called when the main frame load is finished. We want to make sure there's a
            // main resource available when load has been committed, so we delay the emission of
            // load-changed signal until main resource object has been created.
            priv->waitingForMainResource = true;
        }
    }

    if (priv->waitingForMainResource)
        priv->lastDelayedEvent = loadEvent;
    else
        webkitWebViewEmitLoadChanged(webView, loadEvent, false);
}

void webkitWebViewLoadFailed(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError *error)
{
    webkitWebViewCancelAuthenticationRequest(webView);

    gboolean returnValue;
    g_signal_emit(webView, signals[LOAD_FAILED], 0, loadEvent, failingURI, error, &returnValue);
    g_signal_emit(webView, signals[LOAD_CHANGED], 0, WEBKIT_LOAD_FINISHED);
}

void webkitWebViewLoadFailedWithTLSErrors(WebKitWebView* webView, const char* failingURI, GError* error, GTlsCertificateFlags tlsErrors, GTlsCertificate* certificate)
{
    webkitWebViewCancelAuthenticationRequest(webView);

    WebKitTLSErrorsPolicy tlsErrorsPolicy = webkit_web_context_get_tls_errors_policy(webView->priv->context.get());
    if (tlsErrorsPolicy == WEBKIT_TLS_ERRORS_POLICY_FAIL) {
        gboolean returnValue;
        g_signal_emit(webView, signals[LOAD_FAILED_WITH_TLS_ERRORS], 0, failingURI, certificate, tlsErrors, &returnValue);
        if (!returnValue)
            g_signal_emit(webView, signals[LOAD_FAILED], 0, WEBKIT_LOAD_STARTED, failingURI, error, &returnValue);
    }

    g_signal_emit(webView, signals[LOAD_CHANGED], 0, WEBKIT_LOAD_FINISHED);
}

WebPageProxy* webkitWebViewCreateNewPage(WebKitWebView* webView, const WindowFeatures& windowFeatures, WebKitNavigationAction* navigationAction)
{
    WebKitWebView* newWebView;
    g_signal_emit(webView, signals[CREATE], 0, navigationAction, &newWebView);
    if (!newWebView)
        return 0;

    webkitWindowPropertiesUpdateFromWebWindowFeatures(newWebView->priv->windowProperties.get(), windowFeatures);

    RefPtr<WebPageProxy> newPage = getPage(newWebView);
    return newPage.release().leakRef();
}

void webkitWebViewReadyToShowPage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[READY_TO_SHOW], 0, NULL);
}

void webkitWebViewRunAsModal(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[RUN_AS_MODAL], 0, NULL);

    webView->priv->modalLoop = adoptGRef(g_main_loop_new(0, FALSE));

// This is to suppress warnings about gdk_threads_leave and gdk_threads_enter.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gdk_threads_leave();
    g_main_loop_run(webView->priv->modalLoop.get());
    gdk_threads_enter();
#pragma GCC diagnostic pop
}

void webkitWebViewClosePage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[CLOSE], 0, NULL);
}

void webkitWebViewRunJavaScriptAlert(WebKitWebView* webView, const CString& message)
{
    WebKitScriptDialog dialog(WEBKIT_SCRIPT_DIALOG_ALERT, message);
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, &dialog, &returnValue);
}

bool webkitWebViewRunJavaScriptConfirm(WebKitWebView* webView, const CString& message)
{
    WebKitScriptDialog dialog(WEBKIT_SCRIPT_DIALOG_CONFIRM, message);
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, &dialog, &returnValue);
    return dialog.confirmed;
}

CString webkitWebViewRunJavaScriptPrompt(WebKitWebView* webView, const CString& message, const CString& defaultText)
{
    WebKitScriptDialog dialog(WEBKIT_SCRIPT_DIALOG_PROMPT, message, defaultText);
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, &dialog, &returnValue);
    return dialog.text;
}

void webkitWebViewMakePolicyDecision(WebKitWebView* webView, WebKitPolicyDecisionType type, WebKitPolicyDecision* decision)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[DECIDE_POLICY], 0, decision, type, &returnValue);
}

void webkitWebViewMakePermissionRequest(WebKitWebView* webView, WebKitPermissionRequest* request)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[PERMISSION_REQUEST], 0, request, &returnValue);
}

void webkitWebViewMouseTargetChanged(WebKitWebView* webView, const WebHitTestResult::Data& hitTestResult, unsigned modifiers)
{
    webkitWebViewBaseSetTooltipArea(WEBKIT_WEB_VIEW_BASE(webView), hitTestResult.elementBoundingBox);

    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->mouseTargetHitTestResult
        && priv->mouseTargetModifiers == modifiers
        && webkitHitTestResultCompare(priv->mouseTargetHitTestResult.get(), hitTestResult))
        return;

    priv->mouseTargetModifiers = modifiers;
    priv->mouseTargetHitTestResult = adoptGRef(webkitHitTestResultCreate(hitTestResult));
    g_signal_emit(webView, signals[MOUSE_TARGET_CHANGED], 0, priv->mouseTargetHitTestResult.get(), modifiers);
}

void webkitWebViewPrintFrame(WebKitWebView* webView, WebFrameProxy* frame)
{
    GRefPtr<WebKitPrintOperation> printOperation = adoptGRef(webkit_print_operation_new(webView));
    webkitPrintOperationSetPrintMode(printOperation.get(), PrintInfo::PrintModeSync);
    gboolean returnValue;
    g_signal_emit(webView, signals[PRINT], 0, printOperation.get(), &returnValue);
    if (returnValue)
        return;

    WebKitPrintOperationResponse response = webkitPrintOperationRunDialogForFrame(printOperation.get(), 0, frame);
    if (response == WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL)
        return;
    g_signal_connect(printOperation.leakRef(), "finished", G_CALLBACK(g_object_unref), 0);
}

static void mainResourceResponseChangedCallback(WebKitWebResource*, GParamSpec*, WebKitWebView* webView)
{
    webkitWebViewDisconnectMainResourceResponseChangedSignalHandler(webView);
    webkitWebViewEmitDelayedLoadEvents(webView);
}

static void waitForMainResourceResponseIfWaitingForResource(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (!priv->waitingForMainResource)
        return;

    webkitWebViewDisconnectMainResourceResponseChangedSignalHandler(webView);
    priv->mainResourceResponseHandlerID =
        g_signal_connect(priv->mainResource.get(), "notify::response", G_CALLBACK(mainResourceResponseChangedCallback), webView);
}

void webkitWebViewResourceLoadStarted(WebKitWebView* webView, WebFrameProxy* frame, uint64_t resourceIdentifier, WebKitURIRequest* request)
{
    WebKitWebViewPrivate* priv = webView->priv;
    bool isMainResource = frame->isMainFrame() && !priv->mainResource;
    WebKitWebResource* resource = webkitWebResourceCreate(frame, request, isMainResource);
    if (isMainResource) {
        priv->mainResource = resource;
        waitForMainResourceResponseIfWaitingForResource(webView);
    }
    priv->loadingResourcesMap.set(resourceIdentifier, adoptGRef(resource));
    g_signal_emit(webView, signals[RESOURCE_LOAD_STARTED], 0, resource, request);
}

WebKitWebResource* webkitWebViewGetLoadingWebResource(WebKitWebView* webView, uint64_t resourceIdentifier)
{
    GRefPtr<WebKitWebResource> resource = webView->priv->loadingResourcesMap.get(resourceIdentifier);
    return resource.get();
}

void webkitWebViewRemoveLoadingWebResource(WebKitWebView* webView, uint64_t resourceIdentifier)
{
    WebKitWebViewPrivate* priv = webView->priv;
    ASSERT(priv->loadingResourcesMap.contains(resourceIdentifier));
    priv->loadingResourcesMap.remove(resourceIdentifier);
}

bool webkitWebViewEnterFullScreen(WebKitWebView* webView)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[ENTER_FULLSCREEN], 0, &returnValue);
    return !returnValue;
}

bool webkitWebViewLeaveFullScreen(WebKitWebView* webView)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[LEAVE_FULLSCREEN], 0, &returnValue);
    return !returnValue;
}

void webkitWebViewRunFileChooserRequest(WebKitWebView* webView, WebKitFileChooserRequest* request)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[RUN_FILE_CHOOSER], 0, request, &returnValue);
}

static bool webkitWebViewShouldShowInputMethodsMenu(WebKitWebView* webView)
{
    GtkSettings* settings = gtk_widget_get_settings(GTK_WIDGET(webView));
    if (!settings)
        return true;

    gboolean showInputMethodMenu;
    g_object_get(settings, "gtk-show-input-method-menu", &showInputMethodMenu, NULL);
    return showInputMethodMenu;
}

static int getUnicodeMenuItemPosition(WebKitContextMenu* contextMenu)
{
    GList* items = webkit_context_menu_get_items(contextMenu);
    GList* iter;
    int i = 0;
    for (iter = items, i = 0; iter; iter = g_list_next(iter), ++i) {
        WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(iter->data);

        if (webkit_context_menu_item_is_separator(item))
            continue;
        if (webkit_context_menu_item_get_stock_action(item) == WEBKIT_CONTEXT_MENU_ACTION_UNICODE)
            return i;
    }
    return -1;
}

static void webkitWebViewCreateAndAppendInputMethodsMenuItem(WebKitWebView* webView, WebKitContextMenu* contextMenu)
{
    if (!webkitWebViewShouldShowInputMethodsMenu(webView))
        return;

    // Place the im context menu item right before the unicode menu item
    // if it's present.
    int unicodeMenuItemPosition = getUnicodeMenuItemPosition(contextMenu);
    if (unicodeMenuItemPosition == -1)
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_separator());

    GtkIMContext* imContext = webkitWebViewBaseGetIMContext(WEBKIT_WEB_VIEW_BASE(webView));
    GtkMenu* imContextMenu = GTK_MENU(gtk_menu_new());
    gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(imContext), GTK_MENU_SHELL(imContextMenu));
    WebKitContextMenuItem* menuItem = webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS);
    webkitContextMenuItemSetSubMenuFromGtkMenu(menuItem, imContextMenu);
    webkit_context_menu_insert(contextMenu, menuItem, unicodeMenuItemPosition);
}

static void contextMenuDismissed(GtkMenuShell*, WebKitWebView* webView)
{
    g_signal_emit(webView, signals[CONTEXT_MENU_DISMISSED], 0, NULL);
}

void webkitWebViewPopulateContextMenu(WebKitWebView* webView, const Vector<WebContextMenuItemData>& proposedMenu, const WebHitTestResult::Data& hitTestResultData, GVariant* userData)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(webView);
    WebContextMenuProxyGtk* contextMenuProxy = webkitWebViewBaseGetActiveContextMenuProxy(webViewBase);
    ASSERT(contextMenuProxy);

    GRefPtr<WebKitContextMenu> contextMenu = adoptGRef(webkitContextMenuCreate(proposedMenu));
    if (hitTestResultData.isContentEditable)
        webkitWebViewCreateAndAppendInputMethodsMenuItem(webView, contextMenu.get());

    GRefPtr<WebKitHitTestResult> hitTestResult = adoptGRef(webkitHitTestResultCreate(hitTestResultData));
    GUniquePtr<GdkEvent> contextMenuEvent(webkitWebViewBaseTakeContextMenuEvent(webViewBase));

    if (userData)
        webkit_context_menu_set_user_data(WEBKIT_CONTEXT_MENU(contextMenu.get()), userData);

    gboolean returnValue;
    g_signal_emit(webView, signals[CONTEXT_MENU], 0, contextMenu.get(), contextMenuEvent.get(), hitTestResult.get(), &returnValue);
    if (returnValue)
        return;

    Vector<ContextMenuItem> contextMenuItems;
    webkitContextMenuPopulate(contextMenu.get(), contextMenuItems);
    contextMenuProxy->populate(contextMenuItems);

    g_signal_connect(contextMenuProxy->gtkMenu(), "deactivate", G_CALLBACK(contextMenuDismissed), webView);

    // Clear the menu to make sure it's useless after signal emission.
    webkit_context_menu_remove_all(contextMenu.get());
}

void webkitWebViewSubmitFormRequest(WebKitWebView* webView, WebKitFormSubmissionRequest* request)
{
    g_signal_emit(webView, signals[SUBMIT_FORM], 0, request);
}

void webkitWebViewHandleAuthenticationChallenge(WebKitWebView* webView, AuthenticationChallengeProxy* authenticationChallenge)
{
    gboolean privateBrowsingEnabled = webkit_settings_get_enable_private_browsing(webView->priv->settings.get());
    webView->priv->authenticationRequest = adoptGRef(webkitAuthenticationRequestCreate(authenticationChallenge, privateBrowsingEnabled));
    gboolean returnValue;
    g_signal_emit(webView, signals[AUTHENTICATE], 0, webView->priv->authenticationRequest.get(), &returnValue);
}

void webkitWebViewInsecureContentDetected(WebKitWebView* webView, WebKitInsecureContentEvent type)
{
    g_signal_emit(webView, signals[INSECURE_CONTENT_DETECTED], 0, type);
}

bool webkitWebViewEmitShowNotification(WebKitWebView* webView, WebKitNotification* webNotification)
{
    gboolean handled;
    g_signal_emit(webView, signals[SHOW_NOTIFICATION], 0, webNotification, &handled);
    return handled;
}

bool webkitWebViewEmitRunColorChooser(WebKitWebView* webView, WebKitColorChooserRequest* request)
{
    gboolean handled;
    g_signal_emit(webView, signals[RUN_COLOR_CHOOSER], 0, request, &handled);
    return handled;
}

/**
 * webkit_web_view_new:
 *
 * Creates a new #WebKitWebView with the default #WebKitWebContext and
 * no #WebKitUserContentManager associated with it.
 * See also webkit_web_view_new_with_context(),
 * webkit_web_view_new_with_user_content_manager(), and
 * webkit_web_view_new_with_settings().
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new()
{
    return webkit_web_view_new_with_context(webkit_web_context_get_default());
}

/**
 * webkit_web_view_new_with_context:
 * @context: the #WebKitWebContext to be used by the #WebKitWebView
 *
 * Creates a new #WebKitWebView with the given #WebKitWebContext and
 * no #WebKitUserContentManager associated with it.
 * See also webkit_web_view_new_with_user_content_manager() and
 * webkit_web_view_new_with_settings().
 *
 * Returns: The newly created #WebKitWebView widget
 */
GtkWidget* webkit_web_view_new_with_context(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, NULL));
}

/**
 * webkit_web_view_new_with_related_view:
 * @web_view: the related #WebKitWebView
 *
 * Creates a new #WebKitWebView sharing the same web process with @web_view.
 * This method doesn't have any effect when %WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS
 * process model is used, because a single web process is shared for all the web views in the
 * same #WebKitWebContext. When using %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES process model,
 * this method should always be used when creating the #WebKitWebView in the #WebKitWebView::create signal.
 * You can also use this method to implement other process models based on %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES,
 * like for example, sharing the same web process for all the views in the same security domain.
 *
 * The newly created #WebKitWebView will also have the same #WebKitUserContentManager
 * and #WebKitSettings as @web_view.
 *
 * Returns: (transfer full): The newly created #WebKitWebView widget
 *
 * Since: 2.4
 */
GtkWidget* webkit_web_view_new_with_related_view(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "user-content-manager", webView->priv->userContentManager.get(),
        "settings", webView->priv->settings.get(),
        "related-view", webView,
        nullptr));
}

/**
 * webkit_web_view_new_with_settings:
 * @settings: a #WebKitSettings
 *
 * Creates a new #WebKitWebView with the given #WebKitSettings.
 * See also webkit_web_view_new_with_context(), and
 * webkit_web_view_new_with_user_content_manager().
 *
 * Returns: The newly created #WebKitWebView widget
 *
 * Since: 2.6
 */
GtkWidget* webkit_web_view_new_with_settings(WebKitSettings* settings)
{
    g_return_val_if_fail(WEBKIT_IS_SETTINGS(settings), nullptr);
    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "settings", settings, nullptr));
}

/**
 * webkit_web_view_new_with_user_content_manager:
 * @user_content_manager: a #WebKitUserContentManager.
 *
 * Creates a new #WebKitWebView with the given #WebKitUserContentManager.
 * The content loaded in the view may be affected by the content injected
 * in the view by the user content manager.
 *
 * Returns: The newly created #WebKitWebView widget
 *
 * Since: 2.6
 */
GtkWidget* webkit_web_view_new_with_user_content_manager(WebKitUserContentManager* userContentManager)
{
    g_return_val_if_fail(WEBKIT_IS_USER_CONTENT_MANAGER(userContentManager), nullptr);

    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW, "user-content-manager", userContentManager, nullptr));
}

/**
 * webkit_web_view_get_context:
 * @web_view: a #WebKitWebView
 *
 * Gets the web context of @web_view.
 *
 * Returns: (transfer none): the #WebKitWebContext of the view
 */
WebKitWebContext* webkit_web_view_get_context(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->context.get();
}

/**
 * webkit_web_view_get_user_content_manager:
 * @web_view: a #WebKitWebView
 *
 * Gets the user content manager associated to @web_view, or %NULL if the
 * view does not have an user content manager.
 *
 * Returns: (transfer none): the #WebKitUserContentManager associated with the view
 *
 * Since: 2.6
 */
WebKitUserContentManager* webkit_web_view_get_user_content_manager(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return webView->priv->userContentManager.get();
}

/**
 * webkit_web_view_load_uri:
 * @web_view: a #WebKitWebView
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    GUniquePtr<SoupURI> soupURI(soup_uri_new(uri));
    getPage(webView)->loadRequest(URL(soupURI.get()));
}

/**
 * webkit_web_view_load_html:
 * @web_view: a #WebKitWebView
 * @content: The HTML string to load
 * @base_uri: (allow-none): The base URI for relative locations or %NULL
 *
 * Load the given @content string with the specified @base_uri.
 * If @base_uri is not %NULL, relative URLs in the @content will be
 * resolved against @base_uri and absolute local paths must be children of the @base_uri.
 * For security reasons absolute local paths that are not children of @base_uri
 * will cause the web process to terminate.
 * If you need to include URLs in @content that are local paths in a different
 * directory than @base_uri you can build a data URI for them. When @base_uri is %NULL,
 * it defaults to "about:blank". The mime type of the document will be "text/html".
 * You can monitor the load operation by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_html(WebKitWebView* webView, const gchar* content, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    getPage(webView)->loadHTMLString(String::fromUTF8(content), String::fromUTF8(baseURI));
}

/**
 * webkit_web_view_load_alternate_html:
 * @web_view: a #WebKitWebView
 * @content: the new content to display as the main page of the @web_view
 * @content_uri: the URI for the alternate page content
 * @base_uri: (allow-none): the base URI for relative locations or %NULL
 *
 * Load the given @content string for the URI @content_uri.
 * This allows clients to display page-loading errors in the #WebKitWebView itself.
 * When this method is called from #WebKitWebView::load-failed signal to show an
 * error page, the the back-forward list is maintained appropriately.
 * For everything else this method works the same way as webkit_web_view_load_html().
 */
void webkit_web_view_load_alternate_html(WebKitWebView* webView, const gchar* content, const gchar* contentURI, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);
    g_return_if_fail(contentURI);

    getPage(webView)->loadAlternateHTMLString(String::fromUTF8(content), String::fromUTF8(baseURI), String::fromUTF8(contentURI));
}

/**
 * webkit_web_view_load_plain_text:
 * @web_view: a #WebKitWebView
 * @plain_text: The plain text to load
 *
 * Load the specified @plain_text string into @web_view. The mime type of
 * document will be "text/plain". You can monitor the load
 * operation by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_plain_text(WebKitWebView* webView, const gchar* plainText)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(plainText);

    getPage(webView)->loadPlainTextString(String::fromUTF8(plainText));
}

static void releaseGBytes(unsigned char*, const void* bytes)
{
    // Balanced by g_bytes_ref in webkit_web_view_load_bytes().
    g_bytes_unref(static_cast<GBytes*>(const_cast<void*>(bytes)));
}

/**
 * webkit_web_view_load_bytes:
 * @web_view: a #WebKitWebView
 * @bytes: input data to load
 * @mime_type: (allow-none): the MIME type of @bytes, or %NULL
 * @encoding: (allow-none): the character encoding of @bytes, or %NULL
 * @base_uri: (allow-none): the base URI for relative locations or %NULL
 *
 * Load the specified @bytes into @web_view using the given @mime_type and @encoding.
 * When @mime_type is %NULL, it defaults to "text/html".
 * When @encoding is %NULL, it defaults to "UTF-8".
 * When @base_uri is %NULL, it defaults to "about:blank".
 * You can monitor the load operation by connecting to #WebKitWebView::load-changed signal.
 *
 * Since: 2.6
 */
void webkit_web_view_load_bytes(WebKitWebView* webView, GBytes* bytes, const char* mimeType, const char* encoding, const char* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(bytes);

    gsize bytesDataSize;
    gconstpointer bytesData = g_bytes_get_data(bytes, &bytesDataSize);
    g_return_if_fail(bytesDataSize);

    // Balanced by g_bytes_unref in releaseGBytes.
    g_bytes_ref(bytes);

    Ref<API::Data> data = API::Data::createWithoutCopying(static_cast<const unsigned char*>(bytesData), bytesDataSize, releaseGBytes, bytes);
    getPage(webView)->loadData(data.ptr(), mimeType ? String::fromUTF8(mimeType) : String::fromUTF8("text/html"),
        encoding ? String::fromUTF8(encoding) : String::fromUTF8("UTF-8"), String::fromUTF8(baseURI));
}

/**
 * webkit_web_view_load_request:
 * @web_view: a #WebKitWebView
 * @request: a #WebKitURIRequest to load
 *
 * Requests loading of the specified #WebKitURIRequest.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_request(WebKitWebView* webView, WebKitURIRequest* request)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_URI_REQUEST(request));

    ResourceRequest resourceRequest;
    webkitURIRequestGetResourceRequest(request, resourceRequest);
    getPage(webView)->loadRequest(resourceRequest);
}

/**
 * webkit_web_view_get_page_id:
 * @web_view: a #WebKitWebView
 *
 * Get the identifier of the #WebKitWebPage corresponding to
 * the #WebKitWebView
 *
 * Returns: the page ID of @web_view.
 */
guint64 webkit_web_view_get_page_id(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return getPage(webView)->pageID();
}

/**
 * webkit_web_view_get_title:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:title property.
 * You can connect to notify::title signal of @web_view to
 * be notified when the title has been received.
 *
 * Returns: The main frame document title of @web_view.
 */
const gchar* webkit_web_view_get_title(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->title.data();
}

/**
 * webkit_web_view_reload:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view.
 * See also webkit_web_view_reload_bypass_cache().
 */
void webkit_web_view_reload(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    const bool reloadFromOrigin = false;
    const bool contentBlockersEnabled = true;
    getPage(webView)->reload(reloadFromOrigin, contentBlockersEnabled);
}

/**
 * webkit_web_view_reload_bypass_cache:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view without
 * using any cached data.
 */
void webkit_web_view_reload_bypass_cache(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    const bool reloadFromOrigin = true;
    const bool contentBlockersEnabled = true;
    getPage(webView)->reload(reloadFromOrigin, contentBlockersEnabled);
}

/**
 * webkit_web_view_stop_loading:
 * @web_view: a #WebKitWebView
 *
 * Stops any ongoing loading operation in @web_view.
 * This method does nothing if no content is being loaded.
 * If there is a loading operation in progress, it will be cancelled and
 * #WebKitWebView::load-failed signal will be emitted with
 * %WEBKIT_NETWORK_ERROR_CANCELLED error.
 */
void webkit_web_view_stop_loading(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView)->stopLoading();
}

/**
 * webkit_web_view_is_loading:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:is-loading property.
 * You can monitor when a #WebKitWebView is loading a page by connecting to
 * notify::is-loading signal of @web_view. This is useful when you are
 * interesting in knowing when the view is loding something but not in the
 * details about the status of the load operation, for example to start a spinner
 * when the view is loading a page and stop it when it finishes.
 *
 * Returns: %TRUE if @web_view is loading a page or %FALSE otherwise.
 */
gboolean webkit_web_view_is_loading(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return webView->priv->isLoading;
}

/**
 * webkit_web_view_is_playing_audio:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:is-playing-audio property.
 * You can monitor when a page in a #WebKitWebView is playing audio by
 * connecting to the notify::is-playing-audio signal of @web_view. This
 * is useful when the application wants to provide visual feedback when a
 * page is producing sound.
 *
 * Returns: %TRUE if a page in @web_view is playing audio or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_web_view_is_playing_audio(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return getPage(webView)->isPlayingAudio();
}

/**
 * webkit_web_view_go_back:
 * @web_view: a #WebKitWebView
 *
 * Loads the previous history item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView)->goBack();
}

/**
 * webkit_web_view_can_go_back:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a previous history item.
 *
 * Returns: %TRUE if able to move back or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_back(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return !!getPage(webView)->backForwardList().backItem();
}

/**
 * webkit_web_view_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Loads the next history item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView)->goForward();
}

/**
 * webkit_web_view_can_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a next history item.
 *
 * Returns: %TRUE if able to move forward or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_forward(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return !!getPage(webView)->backForwardList().forwardItem();
}

/**
 * webkit_web_view_get_uri:
 * @web_view: a #WebKitWebView
 *
 * Returns the current active URI of @web_view. The active URI might change during
 * a load operation:
 *
 * <orderedlist>
 * <listitem><para>
 *   When nothing has been loaded yet on @web_view the active URI is %NULL.
 * </para></listitem>
 * <listitem><para>
 *   When a new load operation starts the active URI is the requested URI:
 *   <itemizedlist>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_uri(),
 *     the requested URI is the given one.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_html(),
 *     the requested URI is "about:blank".
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_alternate_html(),
 *     the requested URI is content URI provided.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_go_back() or
 *     webkit_web_view_go_forward(), the requested URI is the original URI
 *     of the previous/next item in the #WebKitBackForwardList of @web_view.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by
 *     webkit_web_view_go_to_back_forward_list_item(), the requested URI
 *     is the opriginal URI of the given #WebKitBackForwardListItem.
 *   </para></listitem>
 *   </itemizedlist>
 * </para></listitem>
 * <listitem><para>
 *   If there is a server redirection during the load operation,
 *   the active URI is the redirected URI. When the signal
 *   #WebKitWebView::load-changed is emitted with %WEBKIT_LOAD_REDIRECTED
 *   event, the active URI is already updated to the redirected URI.
 * </para></listitem>
 * <listitem><para>
 *   When the signal #WebKitWebView::load-changed is emitted
 *   with %WEBKIT_LOAD_COMMITTED event, the active URI is the final
 *   one and it will not change unless a new load operation is started
 *   or a navigation action within the same page is performed.
 * </para></listitem>
 * </orderedlist>
 *
 * You can monitor the active URI by connecting to the notify::uri
 * signal of @web_view.
 *
 * Returns: the current active URI of @web_view or %NULL
 *    if nothing has been loaded yet.
 */
const gchar* webkit_web_view_get_uri(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->activeURI.data();
}

/**
 * webkit_web_view_get_favicon:
 * @web_view: a #WebKitWebView
 *
 * Returns favicon currently associated to @web_view, if any. You can
 * connect to notify::favicon signal of @web_view to be notified when
 * the favicon is available.
 *
 * Returns: (transfer none): a pointer to a #cairo_surface_t with the
 *    favicon or %NULL if there's no icon associated with @web_view.
 */
cairo_surface_t* webkit_web_view_get_favicon(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    if (webView->priv->activeURI.isNull())
        return 0;

    return webView->priv->favicon.get();
}

/**
 * webkit_web_view_get_custom_charset:
 * @web_view: a #WebKitWebView
 *
 * Returns the current custom character encoding name of @web_view.
 *
 * Returns: the current custom character encoding name or %NULL if no
 *    custom character encoding has been set.
 */
const gchar* webkit_web_view_get_custom_charset(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    String customTextEncoding = getPage(webView)->customTextEncodingName();
    if (customTextEncoding.isEmpty())
        return 0;

    webView->priv->customTextEncoding = customTextEncoding.utf8();
    return webView->priv->customTextEncoding.data();
}

/**
 * webkit_web_view_set_custom_charset:
 * @web_view: a #WebKitWebView
 * @charset: (allow-none): a character encoding name or %NULL
 *
 * Sets the current custom character encoding override of @web_view. The custom
 * character encoding will override any text encoding detected via HTTP headers or
 * META tags. Calling this method will stop any current load operation and reload the
 * current page. Setting the custom character encoding to %NULL removes the character
 * encoding override.
 */
void webkit_web_view_set_custom_charset(WebKitWebView* webView, const gchar* charset)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView)->setCustomTextEncodingName(String::fromUTF8(charset));
}

/**
 * webkit_web_view_get_estimated_load_progress:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:estimated-load-progress property.
 * You can monitor the estimated progress of a load operation by
 * connecting to the notify::estimated-load-progress signal of @web_view.
 *
 * Returns: an estimate of the of the percent complete for a document
 *     load as a range from 0.0 to 1.0.
 */
gdouble webkit_web_view_get_estimated_load_progress(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    return getPage(webView)->pageLoadState().estimatedProgress();
}

/**
 * webkit_web_view_get_back_forward_list:
 * @web_view: a #WebKitWebView
 *
 * Obtains the #WebKitBackForwardList associated with the given #WebKitWebView. The
 * #WebKitBackForwardList is owned by the #WebKitWebView.
 *
 * Returns: (transfer none): the #WebKitBackForwardList
 */
WebKitBackForwardList* webkit_web_view_get_back_forward_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->backForwardList.get();
}

/**
 * webkit_web_view_go_to_back_forward_list_item:
 * @web_view: a #WebKitWebView
 * @list_item: a #WebKitBackForwardListItem
 *
 * Loads the specific history item @list_item.
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_to_back_forward_list_item(WebKitWebView* webView, WebKitBackForwardListItem* listItem)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem));

    getPage(webView)->goToBackForwardItem(webkitBackForwardListItemGetItem(listItem));
}

/**
 * webkit_web_view_set_settings:
 * @web_view: a #WebKitWebView
 * @settings: a #WebKitSettings
 *
 * Sets the #WebKitSettings to be applied to @web_view. The
 * existing #WebKitSettings of @web_view will be replaced by
 * @settings. New settings are applied immediately on @web_view.
 * The same #WebKitSettings object can be shared
 * by multiple #WebKitWebView<!-- -->s.
 */
void webkit_web_view_set_settings(WebKitWebView* webView, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    if (webView->priv->settings == settings)
        return;

    // The "settings" property is set on construction, and in that
    // case webkit_web_view_set_settings() will be called *before*
    // any settings have been assigned. In that case there are no
    // signal handlers to disconnect.
    if (webView->priv->settings)
        webkitWebViewDisconnectSettingsSignalHandlers(webView);

    webView->priv->settings = settings;
    webkitWebViewUpdateSettings(webView);
    g_object_notify(G_OBJECT(webView), "settings");
}

/**
 * webkit_web_view_get_settings:
 * @web_view: a #WebKitWebView
 *
 * Gets the #WebKitSettings currently applied to @web_view.
 * If no other #WebKitSettings have been explicitly applied to
 * @web_view with webkit_web_view_set_settings(), the default
 * #WebKitSettings will be returned. This method always returns
 * a valid #WebKitSettings object.
 * To modify any of the @web_view settings, you can either create
 * a new #WebKitSettings object with webkit_settings_new(), setting
 * the desired preferences, and then replace the existing @web_view
 * settings with webkit_web_view_set_settings() or get the existing
 * @web_view settings and update it directly. #WebKitSettings objects
 * can be shared by multiple #WebKitWebView<!-- -->s, so modifying
 * the settings of a #WebKitWebView would affect other
 * #WebKitWebView<!-- -->s using the same #WebKitSettings.
 *
 * Returns: (transfer none): the #WebKitSettings attached to @web_view
 */
WebKitSettings* webkit_web_view_get_settings(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    return webView->priv->settings.get();
}

/**
 * webkit_web_view_get_window_properties:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWindowProperties object containing the properties
 * that the window containing @web_view should have.
 *
 * Returns: (transfer none): the #WebKitWindowProperties of @web_view
 */
WebKitWindowProperties* webkit_web_view_get_window_properties(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->windowProperties.get();
}

/**
 * webkit_web_view_set_zoom_level:
 * @web_view: a #WebKitWebView
 * @zoom_level: the zoom level
 *
 * Set the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 */
void webkit_web_view_set_zoom_level(WebKitWebView* webView, gdouble zoomLevel)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_get_zoom_level(webView) == zoomLevel)
        return;

    WebPageProxy* page = getPage(webView);
    if (webkit_settings_get_zoom_text_only(webView->priv->settings.get()))
        page->setTextZoomFactor(zoomLevel);
    else
        page->setPageZoomFactor(zoomLevel);
    g_object_notify(G_OBJECT(webView), "zoom-level");
}

/**
 * webkit_web_view_get_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Get the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 *
 * Returns: the current zoom level of @web_view
 */
gdouble webkit_web_view_get_zoom_level(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 1);

    WebPageProxy* page = getPage(webView);
    gboolean zoomTextOnly = webkit_settings_get_zoom_text_only(webView->priv->settings.get());
    return zoomTextOnly ? page->textZoomFactor() : page->pageZoomFactor();
}

/**
 * webkit_web_view_can_execute_editing_command:
 * @web_view: a #WebKitWebView
 * @command: the command to check
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously execute the given editing command.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_can_execute_editing_command_finish() to get the result of the operation.
 */
void webkit_web_view_can_execute_editing_command(WebKitWebView* webView, const char* command, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(command);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    getPage(webView)->validateCommand(String::fromUTF8(command), [task](const String&, bool isEnabled, int32_t, WebKit::CallbackBase::Error) {
        g_task_return_boolean(adoptGRef(task).get(), isEnabled);        
    });
}

/**
 * webkit_web_view_can_execute_editing_command_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_can_execute_editing_command().
 *
 * Returns: %TRUE if the editing command can be executed or %FALSE otherwise
 */
gboolean webkit_web_view_can_execute_editing_command_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, webView), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_web_view_execute_editing_command:
 * @web_view: a #WebKitWebView
 * @command: the command to execute
 *
 * Request to execute the given @command for @web_view. You can use
 * webkit_web_view_can_execute_editing_command() to check whether
 * it's possible to execute the command.
 */
void webkit_web_view_execute_editing_command(WebKitWebView* webView, const char* command)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(command);

    getPage(webView)->executeEditCommand(String::fromUTF8(command));
}

/**
 * webkit_web_view_get_find_controller:
 * @web_view: the #WebKitWebView
 *
 * Gets the #WebKitFindController that will allow the caller to query
 * the #WebKitWebView for the text to look for.
 *
 * Returns: (transfer none): the #WebKitFindController associated to
 * this particular #WebKitWebView.
 */
WebKitFindController* webkit_web_view_get_find_controller(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    if (!webView->priv->findController)
        webView->priv->findController = adoptGRef(WEBKIT_FIND_CONTROLLER(g_object_new(WEBKIT_TYPE_FIND_CONTROLLER, "web-view", webView, NULL)));

    return webView->priv->findController.get();
}

/**
 * webkit_web_view_get_javascript_global_context:
 * @web_view: a #WebKitWebView
 *
 * Get the global JavaScript context used by @web_view to deserialize the
 * result values of scripts executed with webkit_web_view_run_javascript().
 *
 * Returns: the <function>JSGlobalContextRef</function> used by @web_view to deserialize
 *    the result values of scripts.
 */
JSGlobalContextRef webkit_web_view_get_javascript_global_context(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    if (!webView->priv->javascriptGlobalContext)
        webView->priv->javascriptGlobalContext = JSGlobalContextCreate(0);
    return webView->priv->javascriptGlobalContext;
}

static void webkitWebViewRunJavaScriptCallback(API::SerializedScriptValue* wkSerializedScriptValue, GTask* task)
{
    if (g_task_return_error_if_cancelled(task))
        return;

    if (!wkSerializedScriptValue) {
        g_task_return_new_error(task, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED,
            _("An exception was raised in JavaScript"));
        return;
    }

    WebKitWebView* webView = WEBKIT_WEB_VIEW(g_task_get_source_object(task));
    g_task_return_pointer(task, webkitJavascriptResultCreate(webView,
        *wkSerializedScriptValue->internalRepresentation()),
        reinterpret_cast<GDestroyNotify>(webkit_javascript_result_unref));
}

/**
 * webkit_web_view_run_javascript:
 * @web_view: a #WebKitWebView
 * @script: the script to run
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run @script in the context of the current page in @web_view. If
 * WebKitWebSettings:enable-javascript is FALSE, this method will do nothing.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_run_javascript_finish() to get the result of the operation.
 */
void webkit_web_view_run_javascript(WebKitWebView* webView, const gchar* script, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(script);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    getPage(webView)->runJavaScriptInMainFrame(String::fromUTF8(script), [task](API::SerializedScriptValue* serializedScriptValue, bool, WebKit::CallbackBase::Error) {
        webkitWebViewRunJavaScriptCallback(serializedScriptValue, adoptGRef(task).get());
    });
}

/**
 * webkit_web_view_run_javascript_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_run_javascript().
 *
 * This is an example of using webkit_web_view_run_javascript() with a script returning
 * a string:
 *
 * <informalexample><programlisting>
 * static void
 * web_view_javascript_finished (GObject      *object,
 *                               GAsyncResult *result,
 *                               gpointer      user_data)
 * {
 *     WebKitJavascriptResult *js_result;
 *     JSValueRef              value;
 *     JSGlobalContextRef      context;
 *     GError                 *error = NULL;
 *
 *     js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
 *     if (!js_result) {
 *         g_warning ("Error running javascript: %s", error->message);
 *         g_error_free (error);
 *         return;
 *     }
 *
 *     context = webkit_javascript_result_get_global_context (js_result);
 *     value = webkit_javascript_result_get_value (js_result);
 *     if (JSValueIsString (context, value)) {
 *         JSStringRef js_str_value;
 *         gchar      *str_value;
 *         gsize       str_length;
 *
 *         js_str_value = JSValueToStringCopy (context, value, NULL);
 *         str_length = JSStringGetMaximumUTF8CStringSize (js_str_value);
 *         str_value = (gchar *)g_malloc (str_length);
 *         JSStringGetUTF8CString (js_str_value, str_value, str_length);
 *         JSStringRelease (js_str_value);
 *         g_print ("Script result: %s\n", str_value);
 *         g_free (str_value);
 *     } else {
 *         g_warning ("Error running javascript: unexpected return value");
 *     }
 *     webkit_javascript_result_unref (js_result);
 * }
 *
 * static void
 * web_view_get_link_url (WebKitWebView *web_view,
 *                        const gchar   *link_id)
 * {
 *     gchar *script;
 *
 *     script = g_strdup_printf ("window.document.getElementById('%s').href;", link_id);
 *     webkit_web_view_run_javascript (web_view, script, NULL, web_view_javascript_finished, NULL);
 *     g_free (script);
 * }
 * </programlisting></informalexample>
 *
 * Returns: (transfer full): a #WebKitJavascriptResult with the result of the last executed statement in @script
 *    or %NULL in case of error
 */
WebKitJavascriptResult* webkit_web_view_run_javascript_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    return static_cast<WebKitJavascriptResult*>(g_task_propagate_pointer(G_TASK(result), error));
}

static void resourcesStreamReadCallback(GObject* object, GAsyncResult* result, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(G_TASK(userData));

    GError* error = 0;
    g_output_stream_splice_finish(G_OUTPUT_STREAM(object), result, &error);
    if (error) {
        g_task_return_error(task.get(), error);
        return;
    }

    WebKitWebView* webView = WEBKIT_WEB_VIEW(g_task_get_source_object(task.get()));
    gpointer outputStreamData = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(object));
    getPage(webView)->runJavaScriptInMainFrame(String::fromUTF8(reinterpret_cast<const gchar*>(outputStreamData)),
        [task](API::SerializedScriptValue* serializedScriptValue, bool, WebKit::CallbackBase::Error) {
            webkitWebViewRunJavaScriptCallback(serializedScriptValue, task.get());
        });
}

/**
 * webkit_web_view_run_javascript_from_gresource:
 * @web_view: a #WebKitWebView
 * @resource: the location of the resource to load
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run the script from @resource in the context of the
 * current page in @web_view.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_run_javascript_from_gresource_finish() to get the result
 * of the operation.
 */
void webkit_web_view_run_javascript_from_gresource(WebKitWebView* webView, const gchar* resource, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(resource);

    GError* error = 0;
    GRefPtr<GInputStream> inputStream = adoptGRef(g_resources_open_stream(resource, G_RESOURCE_LOOKUP_FLAGS_NONE, &error));
    if (error) {
        g_task_report_error(webView, callback, userData, 0, error);
        return;
    }

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new(0, 0, fastRealloc, fastFree));
    g_output_stream_splice_async(outputStream.get(), inputStream.get(),
        static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
        G_PRIORITY_DEFAULT, cancellable, resourcesStreamReadCallback, task);
}

/**
 * webkit_web_view_run_javascript_from_gresource_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_run_javascript_from_gresource().
 *
 * Check webkit_web_view_run_javascript_finish() for a usage example.
 *
 * Returns: (transfer full): a #WebKitJavascriptResult with the result of the last executed statement in @script
 *    or %NULL in case of error
 */
WebKitJavascriptResult* webkit_web_view_run_javascript_from_gresource_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    return static_cast<WebKitJavascriptResult*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_view_get_main_resource:
 * @web_view: a #WebKitWebView
 *
 * Return the main resource of @web_view.
 *
 * Returns: (transfer none): the main #WebKitWebResource of the view
 *    or %NULL if nothing has been loaded.
 */
WebKitWebResource* webkit_web_view_get_main_resource(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->mainResource.get();
}

/**
 * webkit_web_view_get_inspector:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWebInspector associated to @web_view
 *
 * Returns: (transfer none): the #WebKitWebInspector of @web_view
 */
WebKitWebInspector* webkit_web_view_get_inspector(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    if (!webView->priv->inspector)
        webView->priv->inspector = adoptGRef(webkitWebInspectorCreate(getPage(webView)->inspector()));

    return webView->priv->inspector.get();
}

/**
 * webkit_web_view_can_show_mime_type:
 * @web_view: a #WebKitWebView
 * @mime_type: a MIME type
 *
 * Whether or not a MIME type can be displayed in @web_view.
 *
 * Returns: %TRUE if the MIME type @mime_type can be displayed or %FALSE otherwise
 */
gboolean webkit_web_view_can_show_mime_type(WebKitWebView* webView, const char* mimeType)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(mimeType, FALSE);

    return getPage(webView)->canShowMIMEType(String::fromUTF8(mimeType));
}

struct ViewSaveAsyncData {
    RefPtr<API::Data> webData;
    GRefPtr<GFile> file;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ViewSaveAsyncData)

static void fileReplaceContentsCallback(GObject* object, GAsyncResult* result, gpointer data)
{
    GRefPtr<GTask> task = adoptGRef(G_TASK(data));
    GError* error = 0;
    if (!g_file_replace_contents_finish(G_FILE(object), result, 0, &error)) {
        g_task_return_error(task.get(), error);
        return;
    }

    g_task_return_boolean(task.get(), TRUE);
}

static void getContentsAsMHTMLDataCallback(API::Data* wkData, GTask* taskPtr)
{
    GRefPtr<GTask> task = adoptGRef(taskPtr);
    if (g_task_return_error_if_cancelled(task.get()))
        return;

    ViewSaveAsyncData* data = static_cast<ViewSaveAsyncData*>(g_task_get_task_data(task.get()));
    // We need to retain the data until the asyncronous process
    // initiated by the user has finished completely.
    data->webData = wkData;

    // If we are saving to a file we need to write the data on disk before finishing.
    if (g_task_get_source_tag(task.get()) == webkit_web_view_save_to_file) {
        ASSERT(G_IS_FILE(data->file.get()));
        GCancellable* cancellable = g_task_get_cancellable(task.get());
        g_file_replace_contents_async(data->file.get(), reinterpret_cast<const gchar*>(data->webData->bytes()), data->webData->size(),
            0, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, fileReplaceContentsCallback, task.leakRef());
        return;
    }

    g_task_return_boolean(task.get(), TRUE);
}

/**
 * webkit_web_view_save:
 * @web_view: a #WebKitWebView
 * @save_mode: the #WebKitSaveMode specifying how the web page should be saved.
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously save the current web page associated to the
 * #WebKitWebView into a self-contained format using the mode
 * specified in @save_mode.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_save_finish() to get the result of the
 * operation.
 */
void webkit_web_view_save(WebKitWebView* webView, WebKitSaveMode saveMode, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    // We only support MHTML at the moment.
    g_return_if_fail(saveMode == WEBKIT_SAVE_MODE_MHTML);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    g_task_set_source_tag(task, reinterpret_cast<gpointer>(webkit_web_view_save));
    g_task_set_task_data(task, createViewSaveAsyncData(), reinterpret_cast<GDestroyNotify>(destroyViewSaveAsyncData));
    getPage(webView)->getContentsAsMHTMLData([task](API::Data* data, WebKit::CallbackBase::Error) {
        getContentsAsMHTMLDataCallback(data, task);
    }, false);
}

/**
 * webkit_web_view_save_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_save().
 *
 * Returns: (transfer full): a #GInputStream with the result of saving
 *    the current web page or %NULL in case of error.
 */
GInputStream* webkit_web_view_save_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    GTask* task = G_TASK(result);
    if (!g_task_propagate_boolean(task, error))
        return 0;

    GInputStream* dataStream = g_memory_input_stream_new();
    ViewSaveAsyncData* data = static_cast<ViewSaveAsyncData*>(g_task_get_task_data(task));
    gsize length = data->webData->size();
    if (length)
        g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(dataStream), g_memdup(data->webData->bytes(), length), length, g_free);

    return dataStream;
}

/**
 * webkit_web_view_save_to_file:
 * @web_view: a #WebKitWebView
 * @file: the #GFile where the current web page should be saved to.
 * @save_mode: the #WebKitSaveMode specifying how the web page should be saved.
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously save the current web page associated to the
 * #WebKitWebView into a self-contained format using the mode
 * specified in @save_mode and writing it to @file.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_save_to_file_finish() to get the result of the
 * operation.
 */
void webkit_web_view_save_to_file(WebKitWebView* webView, GFile* file, WebKitSaveMode saveMode, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(G_IS_FILE(file));

    // We only support MHTML at the moment.
    g_return_if_fail(saveMode == WEBKIT_SAVE_MODE_MHTML);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    g_task_set_source_tag(task, reinterpret_cast<gpointer>(webkit_web_view_save_to_file));
    ViewSaveAsyncData* data = createViewSaveAsyncData();
    data->file = file;
    g_task_set_task_data(task, data, reinterpret_cast<GDestroyNotify>(destroyViewSaveAsyncData));

    getPage(webView)->getContentsAsMHTMLData([task](API::Data* data, WebKit::CallbackBase::Error) {
        getContentsAsMHTMLDataCallback(data, task);
    }, false);
}

/**
 * webkit_web_view_save_to_file_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_save_to_file().
 *
 * Returns: %TRUE if the web page was successfully saved to a file or %FALSE otherwise.
 */
gboolean webkit_web_view_save_to_file_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, webView), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_web_view_download_uri:
 * @web_view: a #WebKitWebView
 * @uri: the URI to download
 *
 * Requests downloading of the specified URI string for @web_view.
 *
 * Returns: (transfer full): a new #WebKitDownload representing the
 *    the download operation.
 */
WebKitDownload* webkit_web_view_download_uri(WebKitWebView* webView, const char* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(uri, 0);

    WebKitDownload* download = webkitWebContextStartDownload(webView->priv->context.get(), uri, getPage(webView));
    webkitDownloadSetWebView(download, webView);

    return download;
}

/**
 * webkit_web_view_get_tls_info:
 * @web_view: a #WebKitWebView
 * @certificate: (out) (transfer none): return location for a #GTlsCertificate
 * @errors: (out): return location for a #GTlsCertificateFlags the verification status of @certificate
 *
 * Retrieves the #GTlsCertificate associated with the main resource of @web_view,
 * and the #GTlsCertificateFlags showing what problems, if any, have been found
 * with that certificate.
 * If the connection is not HTTPS, this function returns %FALSE.
 * This function should be called after a response has been received from the
 * server, so you can connect to #WebKitWebView::load-changed and call this function
 * when it's emitted with %WEBKIT_LOAD_COMMITTED event.
 *
 * Note that this function provides no information about the security of the web
 * page if the current #WebKitTLSErrorsPolicy is @WEBKIT_TLS_ERRORS_POLICY_IGNORE,
 * as subresources of the page may be controlled by an attacker. This function
 * may safely be used to determine the security status of the current page only
 * if the current #WebKitTLSErrorsPolicy is @WEBKIT_TLS_ERRORS_POLICY_FAIL, in
 * which case subresources that fail certificate verification will be blocked.
 *
 * Returns: %TRUE if the @web_view connection uses HTTPS and a response has been received
 *    from the server, or %FALSE otherwise.
 */
gboolean webkit_web_view_get_tls_info(WebKitWebView* webView, GTlsCertificate** certificate, GTlsCertificateFlags* errors)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    WebFrameProxy* mainFrame = getPage(webView)->mainFrame();
    if (!mainFrame)
        return FALSE;

    const WebCore::CertificateInfo& certificateInfo = mainFrame->certificateInfo()->certificateInfo();
    if (certificate)
        *certificate = certificateInfo.certificate();
    if (errors)
        *errors = certificateInfo.tlsErrors();

    return !!certificateInfo.certificate();
}

void webKitWebViewDidReceiveSnapshot(WebKitWebView* webView, uint64_t callbackID, WebImage* webImage)
{
    GRefPtr<GTask> task = webView->priv->snapshotResultsMap.take(callbackID);
    if (g_task_return_error_if_cancelled(task.get()))
        return;

    if (!webImage) {
        g_task_return_new_error(task.get(), WEBKIT_SNAPSHOT_ERROR, WEBKIT_SNAPSHOT_ERROR_FAILED_TO_CREATE,
            _("There was an error creating the snapshot"));
        return;
    }

    if (RefPtr<ShareableBitmap> image = webImage->bitmap())
        g_task_return_pointer(task.get(), image->createCairoSurface().leakRef(), reinterpret_cast<GDestroyNotify>(cairo_surface_destroy));
    else
        g_task_return_pointer(task.get(), 0, 0);
}

static inline unsigned webKitSnapshotOptionsToSnapshotOptions(WebKitSnapshotOptions options)
{
    SnapshotOptions snapshotOptions = 0;

    if (!(options & WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING))
        snapshotOptions |= SnapshotOptionsExcludeSelectionHighlighting;

    return snapshotOptions;
}

static inline SnapshotRegion toSnapshotRegion(WebKitSnapshotRegion region)
{
    switch (region) {
    case WEBKIT_SNAPSHOT_REGION_VISIBLE:
        return SnapshotRegionVisible;
    case WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT:
        return SnapshotRegionFullDocument;
    default:
        ASSERT_NOT_REACHED();
        return SnapshotRegionVisible;
    }
}

static inline uint64_t generateSnapshotCallbackID()
{
    static uint64_t uniqueCallbackID = 1;
    return uniqueCallbackID++;
}

/**
 * webkit_web_view_get_snapshot:
 * @web_view: a #WebKitWebView
 * @options: #WebKitSnapshotOptions for the snapshot
 * @region: the #WebKitSnapshotRegion for this snapshot
 * @cancellable: (allow-none): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data
 *
 * Asynchronously retrieves a snapshot of @web_view for @region.
 * @options specifies how the snapshot should be rendered.
 *
 * When the operation is finished, @callback will be called. You must
 * call webkit_web_view_get_snapshot_finish() to get the result of the
 * operation.
 */
void webkit_web_view_get_snapshot(WebKitWebView* webView, WebKitSnapshotRegion region, WebKitSnapshotOptions options, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    API::Dictionary::MapType message;
    uint64_t callbackID = generateSnapshotCallbackID();
    message.set(String::fromUTF8("SnapshotOptions"), API::UInt64::create(static_cast<uint64_t>(webKitSnapshotOptionsToSnapshotOptions(options))));
    message.set(String::fromUTF8("SnapshotRegion"), API::UInt64::create(static_cast<uint64_t>(toSnapshotRegion(region))));
    message.set(String::fromUTF8("CallbackID"), API::UInt64::create(callbackID));
    message.set(String::fromUTF8("TransparentBackground"), API::Boolean::create(options & WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND));

    webView->priv->snapshotResultsMap.set(callbackID, adoptGRef(g_task_new(webView, cancellable, callback, userData)));
    getPage(webView)->postMessageToInjectedBundle(String::fromUTF8("GetSnapshot"), API::Dictionary::create(WTF::move(message)).ptr());
}

/**
 * webkit_web_view_get_snapshot_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes an asynchronous operation started with webkit_web_view_get_snapshot().
 *
 * Returns: (transfer full): a #cairo_surface_t with the retrieved snapshot or %NULL in error.
 */
cairo_surface_t* webkit_web_view_get_snapshot_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    return static_cast<cairo_surface_t*>(g_task_propagate_pointer(G_TASK(result), error));
}

void webkitWebViewWebProcessCrashed(WebKitWebView* webView)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[WEB_PROCESS_CRASHED], 0, &returnValue);
}

/**
 * webkit_web_view_set_background_color:
 * @web_view: a #WebKitWebView
 * @rgba: a #GdkRGBA
 *
 * Sets the color that will be used to draw the @web_view background before
 * the actual contents are rendered. Note that if the web page loaded in @web_view
 * specifies a background color, it will take precedence over the @rgba color.
 * By default the @web_view background color is opaque white.
 * If the @rgba color is not fully opaque, the parent window must have a RGBA visual and
 * #GtkWidget:app-paintable property set to %TRUE, for the transparencies to work.
 *
 * <informalexample><programlisting>
 * static void browser_window_set_background_color (BrowserWindow *window,
 *                                                  const GdkRGBA *rgba)
 * {
 *     WebKitWebView *web_view;
 *
 *     if (rgba->alpha < 1) {
 *         GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
 *         GdkVisual *rgba_visual = gdk_screen_get_rgba_visual (screen);
 *
 *         if (!rgba_visual)
 *              return;
 *
 *         gtk_widget_set_visual (GTK_WIDGET (window), rgba_visual);
 *         gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
 *     }
 *
 *     web_view = browser_window_get_web_view (window);
 *     webkit_web_view_set_background_color (web_view, rgba);
 * }
 * </programlisting></informalexample>
 *
 * Since: 2.8
 */
void webkit_web_view_set_background_color(WebKitWebView* webView, const GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(rgba);

    Color color(*rgba);
    WebPageProxy* page = getPage(webView);
    if (page->backgroundColor() == color)
        return;

    page->setBackgroundColor(color);
    page->setDrawsBackground(color == Color::white);
}

/**
 * webkit_web_view_get_background_color:
 * @web_view: a #WebKitWebView
 * @rgba: (out): a #GdkRGBA to fill in with the background color
 *
 * Gets the color that is used to draw the @web_view background before
 * the actual contents are rendered.
 * For more information see also webkit_web_view_set_background_color()
 *
 * Since: 2.8
 */
void webkit_web_view_get_background_color(WebKitWebView* webView, GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(rgba);

    *rgba = getPage(webView)->backgroundColor();
}

/*
 * webkit_web_view_is_editable:
 * @web_view: a #WebKitWebView
 *
 * Gets whether the user is allowed to edit the HTML document. When @web_view
 * is not editable an element in the HTML document can only be edited if the
 * CONTENTEDITABLE attribute has been set on the element or one of its parent
 * elements. By default a #WebKitWebView is not editable.
 *
 * Returns: %TRUE if the user is allowed to edit the HTML document, or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_web_view_is_editable(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return getPage(webView)->isEditable();
}

/**
 * webkit_web_view_set_editable:
 * @web_view: a #WebKitWebView
 * @editable: a #gboolean indicating the editable state
 *
 * Sets whether the user is allowed to edit the HTML document.
 *
 * If @editable is %TRUE, @web_view allows the user to edit the HTML document. If
 * @editable is %FALSE, an element in @web_view's document can only be edited if the
 * CONTENTEDITABLE attribute has been set on the element or one of its parent
 * elements. By default a #WebKitWebView is not editable.
 *
 * Normally, a HTML document is not editable unless the elements within the
 * document are editable. This function provides a way to make the contents
 * of a #WebKitWebView editable without altering the document or DOM structure.
 *
 * Since: 2.8
 */
void webkit_web_view_set_editable(WebKitWebView* webView, gboolean editable)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (editable == getPage(webView)->isEditable())
        return;

    getPage(webView)->setEditable(editable);

    g_object_notify(G_OBJECT(webView), "editable");
}
