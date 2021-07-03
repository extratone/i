/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitPrivate.h"

#include "ErrorsGtk.h"
#include "WebEvent.h"
#include "WebKitError.h"
#include <gdk/gdk.h>

unsigned wkEventModifiersToGdkModifiers(WKEventModifiers wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= GDK_META_MASK;
    return modifiers;
}

unsigned toGdkModifiers(WebKit::WebEvent::Modifiers wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers & WebKit::WebEvent::Modifiers::ShiftKey)
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::ControlKey)
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::AltKey)
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::MetaKey)
        modifiers |= GDK_META_MASK;
    return modifiers;
}

WebKitNavigationType toWebKitNavigationType(WebCore::NavigationType type)
{
    switch (type) {
    case WebCore::NavigationType::LinkClicked:
        return WEBKIT_NAVIGATION_TYPE_LINK_CLICKED;
    case WebCore::NavigationType::FormSubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED;
    case WebCore::NavigationType::BackForward:
        return WEBKIT_NAVIGATION_TYPE_BACK_FORWARD;
    case WebCore::NavigationType::Reload:
        return WEBKIT_NAVIGATION_TYPE_RELOAD;
    case WebCore::NavigationType::FormResubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED;
    case WebCore::NavigationType::Other:
        return WEBKIT_NAVIGATION_TYPE_OTHER;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_NAVIGATION_TYPE_OTHER;
    }
}

unsigned toWebKitMouseButton(WebKit::WebMouseEvent::Button button)
{
    switch (button) {
    case WebKit::WebMouseEvent::Button::NoButton:
        return 0;
    case WebKit::WebMouseEvent::Button::LeftButton:
        return 1;
    case WebKit::WebMouseEvent::Button::MiddleButton:
        return 2;
    case WebKit::WebMouseEvent::Button::RightButton:
        return 3;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned wkEventMouseButtonToWebKitMouseButton(WKEventMouseButton wkButton)
{
    switch (wkButton) {
    case kWKEventMouseButtonNoButton:
        return 0;
    case kWKEventMouseButtonLeftButton:
        return 1;
    case kWKEventMouseButtonMiddleButton:
        return 2;
    case kWKEventMouseButtonRightButton:
        return 3;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned toWebKitError(unsigned webCoreError)
{
    switch (webCoreError) {
    case WebCore::NetworkErrorFailed:
        return WEBKIT_NETWORK_ERROR_FAILED;
    case WebCore::NetworkErrorTransport:
        return WEBKIT_NETWORK_ERROR_TRANSPORT;
    case WebCore::NetworkErrorUnknownProtocol:
        return WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL;
    case WebCore::NetworkErrorCancelled:
        return WEBKIT_NETWORK_ERROR_CANCELLED;
    case WebCore::NetworkErrorFileDoesNotExist:
        return WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST;
    case WebCore::PolicyErrorFailed:
        return WEBKIT_POLICY_ERROR_FAILED;
    case WebCore::PolicyErrorCannotShowMimeType:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE;
    case WebCore::PolicyErrorCannotShowURL:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI;
    case WebCore::PolicyErrorFrameLoadInterruptedByPolicyChange:
        return WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE;
    case WebCore::PolicyErrorCannotUseRestrictedPort:
        return WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT;
    case WebCore::PluginErrorFailed:
        return WEBKIT_PLUGIN_ERROR_FAILED;
    case WebCore::PluginErrorCannotFindPlugin:
        return WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN;
    case WebCore::PluginErrorCannotLoadPlugin:
        return WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN;
    case WebCore::PluginErrorJavaUnavailable:
        return WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE;
    case WebCore::PluginErrorConnectionCancelled:
        return WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED;
    case WebCore::PluginErrorWillHandleLoad:
        return WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD;
    case WebCore::DownloadErrorNetwork:
        return WEBKIT_DOWNLOAD_ERROR_NETWORK;
    case WebCore::DownloadErrorCancelledByUser:
        return WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER;
    case WebCore::DownloadErrorDestination:
        return WEBKIT_DOWNLOAD_ERROR_DESTINATION;
    case WebCore::PrintErrorGeneral:
        return WEBKIT_PRINT_ERROR_GENERAL;
    case WebCore::PrintErrorPrinterNotFound:
        return WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND;
    case WebCore::PrintErrorInvalidPageRange:
        return WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE;
    default:
        // This may be a user app defined error, which needs to be passed as-is.
        return webCoreError;
    }
}

unsigned toWebCoreError(unsigned webKitError)
{
    switch (webKitError) {
    case WEBKIT_NETWORK_ERROR_FAILED:
        return WebCore::NetworkErrorFailed;
    case WEBKIT_NETWORK_ERROR_TRANSPORT:
        return WebCore::NetworkErrorTransport;
    case WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL:
        return WebCore::NetworkErrorUnknownProtocol;
    case WEBKIT_NETWORK_ERROR_CANCELLED:
        return WebCore::NetworkErrorCancelled;
    case WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST:
        return WebCore::NetworkErrorFileDoesNotExist;
    case WEBKIT_POLICY_ERROR_FAILED:
        return WebCore::PolicyErrorFailed;
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE:
        return WebCore::PolicyErrorCannotShowMimeType;
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
        return WebCore::PolicyErrorCannotShowURL;
    case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
        return WebCore::PolicyErrorFrameLoadInterruptedByPolicyChange;
    case WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT:
        return WebCore::PolicyErrorCannotUseRestrictedPort;
    case WEBKIT_PLUGIN_ERROR_FAILED:
        return WebCore::PluginErrorFailed;
    case WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN:
        return WebCore::PluginErrorCannotFindPlugin;
    case WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN:
        return WebCore::PluginErrorCannotLoadPlugin;
    case WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE:
        return WebCore::PluginErrorJavaUnavailable;
    case WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED:
        return WebCore::PluginErrorConnectionCancelled;
    case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
        return WebCore::PluginErrorWillHandleLoad;
    case WEBKIT_DOWNLOAD_ERROR_NETWORK:
        return WebCore::DownloadErrorNetwork;
    case WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER:
        return WebCore::DownloadErrorCancelledByUser;
    case WEBKIT_DOWNLOAD_ERROR_DESTINATION:
        return WebCore::DownloadErrorDestination;
    case WEBKIT_PRINT_ERROR_GENERAL:
        return WebCore::PrintErrorGeneral;
    case WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND:
        return WebCore::PrintErrorPrinterNotFound;
    case WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE:
        return WebCore::PrintErrorInvalidPageRange;
    default:
        // This may be a user app defined error, which needs to be passed as-is.
        return webKitError;
    }
}
