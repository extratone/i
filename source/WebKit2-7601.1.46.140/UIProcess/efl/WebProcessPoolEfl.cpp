/*
 * Copyright (C) 2011 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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
#include "WebProcessPool.h"

#include "APIProcessPoolConfiguration.h"
#include "Logging.h"
#include "WebCookieManagerProxy.h"
#include "WebInspectorServer.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebSoupCustomProtocolRequestManager.h"
#include <Efreet.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {

static void initializeInspectorServer()
{
#if ENABLE(INSPECTOR_SERVER)
    static bool initialized = false;
    if (initialized)
        return;

    // It should be set to true always.
    // Because it is to ensure initializeInspectorServer() is executed only once,
    // even if the server fails to run.
    initialized = true;

    String serverAddress(getenv("WEBKIT_INSPECTOR_SERVER"));
    if (!serverAddress.isNull()) {
        String bindAddress = ASCIILiteral("127.0.0.1");
        unsigned short port = 2999;

        Vector<String> result;
        serverAddress.split(':', result);

        if (result.size() == 2) {
            bindAddress = result[0];
            bool ok = false;
            port = result[1].toUInt(&ok);
            if (!ok) {
                port = 2999;
                LOG_ERROR("Couldn't parse the port. Using 2999 instead.");
            }
        } else
            LOG_ERROR("Couldn't parse %s, wrong format? Using 127.0.0.1:2999 instead.", serverAddress.utf8().data());

        if (!WebInspectorServer::singleton().listen(bindAddress, port))
            LOG_ERROR("Couldn't start listening on: IP address=%s, port=%d.", bindAddress.utf8().data(), port);

        return;
    }

    LOG(InspectorServer, "To start inspector server set WEBKIT_INSPECTOR_SERVER to 127.0.0.1:2999 for example.");
#endif
}

String WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory()
{
    return String::fromUTF8(efreet_cache_home_get()) + "/WebKitEfl/Applications";
}

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    initializeInspectorServer();

    parameters.urlSchemesRegisteredForCustomProtocols = supplement<WebSoupCustomProtocolRequestManager>()->registeredSchemesForCustomProtocols();
    supplement<WebCookieManagerProxy>()->getCookiePersistentStorage(parameters.cookiePersistentStoragePath, parameters.cookiePersistentStorageType);
    parameters.cookieAcceptPolicy = m_initialHTTPCookieAcceptPolicy;
    parameters.ignoreTLSErrors = m_ignoreTLSErrors;
    parameters.diskCacheDirectory = m_configuration->diskCacheDirectory();
}

void WebProcessPool::platformInvalidateContext()
{
    notImplemented();
}

String WebProcessPool::legacyPlatformDefaultWebSQLDatabaseDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/Databases";
}

String WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory()
{
    notImplemented();
    return String();
}

String WebProcessPool::platformDefaultIconDatabasePath() const
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/IconDatabase/" + WebCore::IconDatabase::defaultDatabaseFilename();
}

String WebProcessPool::legacyPlatformDefaultLocalStorageDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/LocalStorage";
}

String WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory()
{
    return String::fromUTF8(efreet_data_home_get()) + "/WebKitEfl/MediaKeys";
}

String WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory()
{
#if ENABLE(NETWORK_CACHE)
    static const char networkCacheSubdirectory[] = "WebKitCache";
#else
    static const char networkCacheSubdirectory[] = "webkit";
#endif

    StringBuilder diskCacheDirectory;
    diskCacheDirectory.append(efreet_cache_home_get());
    diskCacheDirectory.appendLiteral("/");
    diskCacheDirectory.append(networkCacheSubdirectory);

    return diskCacheDirectory.toString();
}

void WebProcessPool::setIgnoreTLSErrors(bool ignoreTLSErrors)
{
    m_ignoreTLSErrors = ignoreTLSErrors;
    sendToAllProcesses(Messages::WebProcess::SetIgnoreTLSErrors(m_ignoreTLSErrors));
}

} // namespace WebKit
