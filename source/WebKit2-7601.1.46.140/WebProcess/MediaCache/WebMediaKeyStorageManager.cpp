/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebMediaKeyStorageManager.h"

#include "SecurityOriginData.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/FileSystem.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/URL.h>

using namespace WebCore;

namespace WebKit {

void WebMediaKeyStorageManager::initialize(const WebProcessCreationParameters &parameters)
{
    ASSERT(!parameters.mediaKeyStorageDirectory.isEmpty());
    m_mediaKeyStorageDirectory = parameters.mediaKeyStorageDirectory;
}

const char* WebMediaKeyStorageManager::supplementName()
{
    return "WebMediaKeyStorageManager";
}

String WebMediaKeyStorageManager::mediaKeyStorageDirectoryForOrigin(const SecurityOriginData& originData)
{
    if (!m_mediaKeyStorageDirectory)
        return emptyString();

    return pathByAppendingComponent(m_mediaKeyStorageDirectory, originData.securityOrigin()->databaseIdentifier());
}

Vector<SecurityOriginData> WebMediaKeyStorageManager::getMediaKeyOrigins()
{
    Vector<SecurityOriginData> results;

    if (m_mediaKeyStorageDirectory.isEmpty())
        return results;

    Vector<String> originPaths = listDirectory(m_mediaKeyStorageDirectory, "*");
    for (const auto& originPath : originPaths) {
        URL url;
        url.setProtocol(ASCIILiteral("file"));
        url.setPath(originPath);

        String mediaKeyIdentifier = url.lastPathComponent();

        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::maybeCreateFromDatabaseIdentifier(mediaKeyIdentifier);
        if (!securityOrigin)
            continue;

        results.append(SecurityOriginData::fromSecurityOrigin(*securityOrigin));
    }

    return results;
}

static void removeAllMediaKeyStorageForOriginPath(const String& originPath, double startDate, double endDate)
{
    Vector<String> mediaKeyPaths = listDirectory(originPath, "*");

    for (const auto& mediaKeyPath : mediaKeyPaths) {
        String mediaKeyFile = pathByAppendingComponent(mediaKeyPath, "SecureStop.plist");

        if (!fileExists(mediaKeyFile))
            continue;

        time_t modTime;
        getFileModificationTime(mediaKeyFile, modTime);

        if (modTime < startDate || modTime > endDate)
            continue;

        deleteFile(mediaKeyFile);
        deleteEmptyDirectory(mediaKeyPath);
    }
    
    deleteEmptyDirectory(originPath);
}

void WebMediaKeyStorageManager::deleteMediaKeyEntriesForOrigin(const SecurityOriginData& originData)
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    String originPath = mediaKeyStorageDirectoryForOrigin(originData);
    removeAllMediaKeyStorageForOriginPath(originPath, std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());
}

void WebMediaKeyStorageManager::deleteMediaKeyEntriesModifiedBetweenDates(double startDate, double endDate)
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    Vector<String> originPaths = listDirectory(m_mediaKeyStorageDirectory, "*");
    for (auto& originPath : originPaths)
        removeAllMediaKeyStorageForOriginPath(originPath, startDate, endDate);
}

void WebMediaKeyStorageManager::deleteAllMediaKeyEntries()
{
    if (m_mediaKeyStorageDirectory.isEmpty())
        return;

    Vector<String> originPaths = listDirectory(m_mediaKeyStorageDirectory, "*");
    for (auto& originPath : originPaths)
        removeAllMediaKeyStorageForOriginPath(originPath, std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());
}

}
