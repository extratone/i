/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
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
#include "PluginDatabase.h"

#include "Frame.h"
#include "KURL.h"
#include "PluginPackage.h"
#include <stdlib.h>

namespace WebCore {

PluginDatabase* PluginDatabase::installedPlugins()
{
    static PluginDatabase* plugins = 0;
    
    if (!plugins) {
        plugins = new PluginDatabase;
        plugins->setPluginDirectories(PluginDatabase::defaultPluginDirectories());
        plugins->refresh();
    }

    return plugins;
}

bool PluginDatabase::isMIMETypeRegistered(const String& mimeType)
{
    if (mimeType.isNull())
        return false;
    if (m_registeredMIMETypes.contains(mimeType))
        return true;
    // No plugin was found, try refreshing the database and searching again
    return (refresh() && m_registeredMIMETypes.contains(mimeType));
}

void PluginDatabase::addExtraPluginDirectory(const String& directory)
{
    m_pluginDirectories.append(directory);
    refresh();
}

bool PluginDatabase::refresh()
{   
    bool pluginSetChanged = false;

    if (!m_plugins.isEmpty()) {
        PluginSet pluginsToUnload;
        getDeletedPlugins(pluginsToUnload);

        // Unload plugins
        PluginSet::const_iterator end = pluginsToUnload.end();
        for (PluginSet::const_iterator it = pluginsToUnload.begin(); it != end; ++it)
            remove(it->get());

        pluginSetChanged = !pluginsToUnload.isEmpty();
    }

    HashSet<String> paths;
    getPluginPathsInDirectories(paths);

    HashMap<String, time_t> pathsWithTimes;

    // We should only skip unchanged files if we didn't remove any plugins above. If we did remove
    // any plugins, we need to look at every plugin file so that, e.g., if the user has two versions
    // of RealPlayer installed and just removed the newer one, we'll pick up the older one.
    bool shouldSkipUnchangedFiles = !pluginSetChanged;

    HashSet<String>::const_iterator pathsEnd = paths.end();
    for (HashSet<String>::const_iterator it = paths.begin(); it != pathsEnd; ++it) {
        time_t lastModified;
        if (!getFileModificationTime(*it, lastModified))
            continue;

        pathsWithTimes.add(*it, lastModified);

        // If the path's timestamp hasn't changed since the last time we ran refresh(), we don't have to do anything.
        if (shouldSkipUnchangedFiles && m_pluginPathsWithTimes.get(*it) == lastModified)
            continue;

        if (RefPtr<PluginPackage> oldPackage = m_pluginsByPath.get(*it)) {
            ASSERT(!shouldSkipUnchangedFiles || oldPackage->lastModified() != lastModified);
            remove(oldPackage.get());
        }

        RefPtr<PluginPackage> package = PluginPackage::createPackage(*it, lastModified);
        if (package && add(package.release()))
            pluginSetChanged = true;
    }

    // Cache all the paths we found with their timestamps for next time.
    pathsWithTimes.swap(m_pluginPathsWithTimes);

    if (!pluginSetChanged)
        return false;

    m_registeredMIMETypes.clear();

    // Register plug-in MIME types
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        // Get MIME types
        MIMEToDescriptionsMap::const_iterator map_end = (*it)->mimeToDescriptions().end();
        for (MIMEToDescriptionsMap::const_iterator map_it = (*it)->mimeToDescriptions().begin(); map_it != map_end; ++map_it) {
            m_registeredMIMETypes.add(map_it->first);
        }
    }

    return true;
}

Vector<PluginPackage*> PluginDatabase::plugins() const
{
    Vector<PluginPackage*> result;

    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it)
        result.append((*it).get());

    return result;
}

int PluginDatabase::preferredPluginCompare(const void* a, const void* b)
{
    PluginPackage* pluginA = *static_cast<PluginPackage* const*>(a);
    PluginPackage* pluginB = *static_cast<PluginPackage* const*>(b);

    return pluginA->compare(*pluginB);
}

PluginPackage* PluginDatabase::pluginForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return 0;

    String key = mimeType.lower();
    PluginSet::const_iterator end = m_plugins.end();

    Vector<PluginPackage*, 2> pluginChoices;

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if ((*it)->mimeToDescriptions().contains(key))
            pluginChoices.append((*it).get());
    }

    if (pluginChoices.isEmpty())
        return 0;

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return pluginChoices[0];
}

String PluginDatabase::MIMETypeForExtension(const String& extension) const
{
    if (extension.isEmpty())
        return String();

    PluginSet::const_iterator end = m_plugins.end();
    String mimeType;
    Vector<PluginPackage*, 2> pluginChoices;
    HashMap<PluginPackage*, String> mimeTypeForPlugin;

    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        MIMEToExtensionsMap::const_iterator mime_end = (*it)->mimeToExtensions().end();

        for (MIMEToExtensionsMap::const_iterator mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            const Vector<String>& extensions = mime_it->second;
            bool foundMapping = false;
            for (unsigned i = 0; i < extensions.size(); i++) {
                if (equalIgnoringCase(extensions[i], extension)) {
                    PluginPackage* plugin = (*it).get();
                    pluginChoices.append(plugin);
                    mimeTypeForPlugin.add(plugin, mime_it->first);
                    foundMapping = true;
                    break;
                }
            }
            if (foundMapping)
                break;
        }
    }

    if (pluginChoices.isEmpty())
        return String();

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return mimeTypeForPlugin.get(pluginChoices[0]);
}

PluginPackage* PluginDatabase::findPlugin(const KURL& url, String& mimeType)
{
    PluginPackage* plugin = pluginForMIMEType(mimeType);
    String filename = url.string();
    
    if (!plugin) {
        String filename = url.lastPathComponent();
        if (!filename.endsWith("/")) {
            int extensionPos = filename.reverseFind('.');
            if (extensionPos != -1) {
                String extension = filename.substring(extensionPos + 1);

                mimeType = MIMETypeForExtension(extension);
                plugin = pluginForMIMEType(mimeType);
            }
        }
    }

    // FIXME: if no plugin could be found, query Windows for the mime type 
    // corresponding to the extension.

    return plugin;
}

void PluginDatabase::getDeletedPlugins(PluginSet& plugins) const
{
    PluginSet::const_iterator end = m_plugins.end();
    for (PluginSet::const_iterator it = m_plugins.begin(); it != end; ++it) {
        if (!fileExists((*it)->path()))
            plugins.add(*it);
    }
}

bool PluginDatabase::add(PassRefPtr<PluginPackage> prpPackage)
{
    ASSERT_ARG(prpPackage, prpPackage);

    RefPtr<PluginPackage> package = prpPackage;

    if (!m_plugins.add(package).second)
        return false;

    m_pluginsByPath.add(package->path(), package);
    return true;
}

void PluginDatabase::remove(PluginPackage* package)
{
    m_plugins.remove(package);
    m_pluginsByPath.remove(package->path());
}

#if !PLATFORM(WIN_OS) || PLATFORM(WX)
// For Safari/Win the following three methods are implemented
// in PluginDatabaseWin.cpp, but if we can use WebCore constructs
// for the logic we should perhaps move it here under XP_WIN?

Vector<String> PluginDatabase::defaultPluginDirectories()
{
    Vector<String> paths;

    // Add paths specific to each platform
#if defined(XP_UNIX)
    String userPluginPath = homeDirectoryPath();
    userPluginPath.append(String("/.mozilla/plugins"));
    paths.append(userPluginPath);

    userPluginPath = homeDirectoryPath();
    userPluginPath.append(String("/.netscape/plugins"));
    paths.append(userPluginPath);

    paths.append("/usr/lib/browser/plugins");
    paths.append("/usr/local/lib/mozilla/plugins");
    paths.append("/usr/lib/firefox/plugins");
    paths.append("/usr/lib64/browser-plugins");
    paths.append("/usr/lib/browser-plugins");
    paths.append("/usr/lib/mozilla/plugins");
    paths.append("/usr/local/netscape/plugins");
    paths.append("/opt/mozilla/plugins");
    paths.append("/opt/mozilla/lib/plugins");
    paths.append("/opt/netscape/plugins");
    paths.append("/opt/netscape/communicator/plugins");
    paths.append("/usr/lib/netscape/plugins");
    paths.append("/usr/lib/netscape/plugins-libc5");
    paths.append("/usr/lib/netscape/plugins-libc6");
    paths.append("/usr/lib64/netscape/plugins");
    paths.append("/usr/lib64/mozilla/plugins");

    String mozHome(getenv("MOZILLA_HOME"));
    mozHome.append("/plugins");
    paths.append(mozHome);

    Vector<String> mozPaths;
    String mozPath(getenv("MOZ_PLUGIN_PATH"));
    mozPath.split(UChar(':'), /* allowEmptyEntries */ false, mozPaths);
    paths.append(mozPaths);
#elif defined(XP_MACOSX)
    String userPluginPath = homeDirectoryPath();
    userPluginPath.append(String("/Library/Internet Plug-Ins"));
    paths.append(userPluginPath);
    paths.append("/Library/Internet Plug-Ins");
#elif defined(XP_WIN)
    String userPluginPath = homeDirectoryPath();
    userPluginPath.append(String("\\Application Data\\Mozilla\\plugins"));
    paths.append(userPluginPath);
#endif

    // Add paths specific to each port
#if PLATFORM(QT)
    Vector<String> qtPaths;
    String qtPath(getenv("QTWEBKIT_PLUGIN_PATH"));
    qtPath.split(UChar(':'), /* allowEmptyEntries */ false, qtPaths);
    paths.append(qtPaths);
#endif

    return paths;
}

bool PluginDatabase::isPreferredPluginDirectory(const String& path)
{
    String preferredPath = homeDirectoryPath();

#if defined(XP_UNIX)
    preferredPath.append(String("/.mozilla/plugins"));
#elif defined(XP_MACOSX)
    preferredPath.append(String("/Library/Internet Plug-Ins"));
#elif defined(XP_WIN)
    preferredPath.append(String("\\Application Data\\Mozilla\\plugins"));
#endif

    // TODO: We should normalize the path before doing a comparison.
    return path == preferredPath;
}

void PluginDatabase::getPluginPathsInDirectories(HashSet<String>& paths) const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;

#if defined(XP_UNIX)
    String fileNameFilter("*.so");
#else
    String fileNameFilter("");
#endif

    Vector<String>::const_iterator dirsEnd = m_pluginDirectories.end();
    for (Vector<String>::const_iterator dIt = m_pluginDirectories.begin(); dIt != dirsEnd; ++dIt) {
        Vector<String> pluginPaths = listDirectory(*dIt, fileNameFilter);
        Vector<String>::const_iterator pluginsEnd = pluginPaths.end();
        for (Vector<String>::const_iterator pIt = pluginPaths.begin(); pIt != pluginsEnd; ++pIt) {
            if (!fileExists(*pIt))
                continue;

            paths.add(*pIt);
        }
    }
}

#endif // !PLATFORM(WIN_OS)

}
