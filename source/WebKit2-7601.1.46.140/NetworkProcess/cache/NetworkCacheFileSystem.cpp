/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheFileSystem.h"

#if ENABLE(NETWORK_CACHE)

#include <WebCore/FileSystem.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <wtf/text/CString.h>

#if USE(SOUP)
#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebKit {
namespace NetworkCache {

static DirectoryEntryType directoryEntryType(uint8_t dtype)
{
    switch (dtype) {
    case DT_DIR:
        return DirectoryEntryType::Directory;
    case DT_REG:
        return DirectoryEntryType::File;
    default:
        ASSERT_NOT_REACHED();
        return DirectoryEntryType::File;
    }
}

void traverseDirectory(const String& path, const std::function<void (const String&, DirectoryEntryType)>& function)
{
    DIR* dir = opendir(WebCore::fileSystemRepresentation(path).data());
    if (!dir)
        return;
    dirent* dp;
    while ((dp = readdir(dir))) {
        if (dp->d_type != DT_DIR && dp->d_type != DT_REG)
            continue;
        const char* name = dp->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        auto nameString = String::fromUTF8(name);
        if (nameString.isNull())
            continue;
        function(nameString, directoryEntryType(dp->d_type));
    }
    closedir(dir);
}

void deleteDirectoryRecursively(const String& path)
{
    traverseDirectory(path, [&path](const String& name, DirectoryEntryType type) {
        String entryPath = WebCore::pathByAppendingComponent(path, name);
        switch (type) {
        case DirectoryEntryType::File:
            WebCore::deleteFile(entryPath);
            break;
        case DirectoryEntryType::Directory:
            deleteDirectoryRecursively(entryPath);
            break;
        // This doesn't follow symlinks.
        }
    });
    WebCore::deleteEmptyDirectory(path);
}

FileTimes fileTimes(const String& path)
{
#if HAVE(STAT_BIRTHTIME)
    struct stat fileInfo;
    if (stat(WebCore::fileSystemRepresentation(path).data(), &fileInfo))
        return { };
    return { std::chrono::system_clock::from_time_t(fileInfo.st_birthtime), std::chrono::system_clock::from_time_t(fileInfo.st_mtime) };
#elif USE(SOUP)
    // There's no st_birthtime in some operating systems like Linux, so we use xattrs to set/get the creation time.
    GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(WebCore::fileSystemRepresentation(path).data()));
    GRefPtr<GFileInfo> fileInfo = adoptGRef(g_file_query_info(file.get(), "xattr::birthtime,time::modified", G_FILE_QUERY_INFO_NONE, nullptr, nullptr));
    if (!fileInfo)
        return { };
    const char* birthtimeString = g_file_info_get_attribute_string(fileInfo.get(), "xattr::birthtime");
    if (!birthtimeString)
        return { };
    return { std::chrono::system_clock::from_time_t(g_ascii_strtoull(birthtimeString, nullptr, 10)),
        std::chrono::system_clock::from_time_t(g_file_info_get_attribute_uint64(fileInfo.get(), "time::modified")) };
#endif
}

void updateFileModificationTimeIfNeeded(const String& path)
{
    auto times = fileTimes(path);
    if (times.creation != times.modification) {
        // Don't update more than once per hour.
        if (std::chrono::system_clock::now() - times.modification < std::chrono::hours(1))
            return;
    }
    // This really updates both the access time and the modification time.
    utimes(WebCore::fileSystemRepresentation(path).data(), nullptr);
}

}
}

#endif // ENABLE(NETWORK_CACHE)
