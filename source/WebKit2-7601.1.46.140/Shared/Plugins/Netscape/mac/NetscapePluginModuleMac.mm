/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetscapePluginModule.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "PluginProcessProxy.h"
#import "PluginSandboxProfile.h"
#import <wtf/HashSet.h>
#import <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

static bool getPluginArchitecture(CFBundleRef bundle, PluginModuleInfo& plugin)
{
    RetainPtr<CFArrayRef> pluginArchitecturesArray = adoptCF(CFBundleCopyExecutableArchitectures(bundle));
    if (!pluginArchitecturesArray)
        return false;

    // Turn the array into a set.
    HashSet<unsigned> architectures;
    for (CFIndex i = 0, numPluginArchitectures = CFArrayGetCount(pluginArchitecturesArray.get()); i < numPluginArchitectures; ++i) {
        CFNumberRef number = static_cast<CFNumberRef>(CFArrayGetValueAtIndex(pluginArchitecturesArray.get(), i));
        
        SInt32 architecture;
        if (!CFNumberGetValue(number, kCFNumberSInt32Type, &architecture))
            continue;
        architectures.add(architecture);
    }
    
#ifdef __x86_64__
    // We only support 64-bit Intel plug-ins on 64-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureX86_64)) {
        plugin.pluginArchitecture = CPU_TYPE_X86_64;
        return true;
    }

    // We also support 32-bit Intel plug-ins on 64-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureI386)) {
        plugin.pluginArchitecture = CPU_TYPE_X86;
        return true;
    }
#elif defined(__i386__)
    // We only support 32-bit Intel plug-ins on 32-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureI386)) {
        plugin.pluginArchitecture = CPU_TYPE_X86;
        return true;
    }
#elif defined(__ppc64__)
    // We only support 64-bit PPC plug-ins on 64-bit PPC.
    if (architectures.contains(kCFBundleExecutableArchitecturePPC64)) {
        plugin.pluginArchitecture = CPU_TYPE_POWERPC64;
        return true;
    }
#elif defined(__ppc__)
    // We only support 32-bit PPC plug-ins on 32-bit PPC.
    if (architectures.contains(kCFBundleExecutableArchitecturePPC)) {
        plugin.pluginArchitecture = CPU_TYPE_POWERPC;
        return true;
    }
#else
#error "Unhandled architecture"
#endif

    return false;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
static RetainPtr<CFDictionaryRef> contentsOfPropertyListAtURL(CFURLRef propertyListURL)
{
    RetainPtr<NSData> propertyListData = adoptNS([[NSData alloc] initWithContentsOfURL:(NSURL *)propertyListURL]);
    if (!propertyListData)
        return 0;

    RetainPtr<CFPropertyListRef> propertyList = adoptCF(CFPropertyListCreateWithData(kCFAllocatorDefault, (CFDataRef)propertyListData.get(), kCFPropertyListImmutable, 0, 0));
    if (!propertyList)
        return 0;

    if (CFGetTypeID(propertyList.get()) != CFDictionaryGetTypeID())
        return 0;

    return static_cast<CFDictionaryRef>(propertyList.get());
}
#endif

static RetainPtr<CFDictionaryRef> getMIMETypesFromPluginBundle(CFBundleRef bundle, const PluginModuleInfo& plugin)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
    CFStringRef propertyListFilename = static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypesFilename")));
    if (propertyListFilename) {
        RetainPtr<CFStringRef> propertyListPath = adoptCF(CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("%@/Library/Preferences/%@"), NSHomeDirectory(), propertyListFilename));
        RetainPtr<CFURLRef> propertyListURL = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, propertyListPath.get(), kCFURLPOSIXPathStyle, FALSE));

        RetainPtr<CFDictionaryRef> propertyList = contentsOfPropertyListAtURL(propertyListURL.get());

        if (!propertyList && PluginProcessProxy::createPropertyListFile(plugin))
            propertyList = contentsOfPropertyListAtURL(propertyListURL.get());

        if (!propertyList)
            return 0;
        
        return static_cast<CFDictionaryRef>(CFDictionaryGetValue(propertyList.get(), CFSTR("WebPluginMIMETypes")));
    }
#endif
    
    return static_cast<CFDictionaryRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypes")));
}

static bool getPluginInfoFromPropertyLists(CFBundleRef bundle, PluginModuleInfo& plugin)
{
    RetainPtr<CFDictionaryRef> mimeTypes = getMIMETypesFromPluginBundle(bundle, plugin);
    if (!mimeTypes || CFGetTypeID(mimeTypes.get()) != CFDictionaryGetTypeID())
        return false;

    // Get the plug-in name.
    CFStringRef pluginName = static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginName")));
    if (pluginName && CFGetTypeID(pluginName) == CFStringGetTypeID())
        plugin.info.name = pluginName;
    
    // Get the plug-in description.
    CFStringRef pluginDescription = static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginDescription")));
    if (pluginDescription && CFGetTypeID(pluginDescription) == CFStringGetTypeID())
        plugin.info.desc = pluginDescription;
    
    // Get the MIME type mapping dictionary.
    CFIndex numMimeTypes = CFDictionaryGetCount(mimeTypes.get());          
    Vector<CFStringRef> mimeTypesVector(numMimeTypes);
    Vector<CFDictionaryRef> mimeTypeInfoVector(numMimeTypes);
    CFDictionaryGetKeysAndValues(mimeTypes.get(), reinterpret_cast<const void**>(mimeTypesVector.data()), reinterpret_cast<const void**>(mimeTypeInfoVector.data()));
    
    for (CFIndex i = 0; i < numMimeTypes; ++i) {
        MimeClassInfo mimeClassInfo;
        
        // If this MIME type is invalid, ignore it.
        CFStringRef mimeType = mimeTypesVector[i];
        if (!mimeType || CFGetTypeID(mimeType) != CFStringGetTypeID() || CFStringGetLength(mimeType) == 0)
            continue;

        // If this MIME type doesn't have a valid info dictionary, ignore it.
        CFDictionaryRef mimeTypeInfo = mimeTypeInfoVector[i];
        if (!mimeTypeInfo || CFGetTypeID(mimeTypeInfo) != CFDictionaryGetTypeID())
            continue;

        // FIXME: Consider storing disabled MIME types.
        CFTypeRef isEnabled = CFDictionaryGetValue(mimeTypeInfo, CFSTR("WebPluginTypeEnabled"));
        if (isEnabled) {
            if (CFGetTypeID(isEnabled) == CFNumberGetTypeID()) {
                int value;
                if (!CFNumberGetValue(static_cast<CFNumberRef>(isEnabled), kCFNumberIntType, &value) || !value)
                    continue;
            } else if (CFGetTypeID(isEnabled) == CFBooleanGetTypeID()) {
                if (!CFBooleanGetValue(static_cast<CFBooleanRef>(isEnabled)))
                    continue;
            } else
                continue;
        }

        // Get the MIME type description.
        CFStringRef mimeTypeDescription = static_cast<CFStringRef>(CFDictionaryGetValue(mimeTypeInfo, CFSTR("WebPluginTypeDescription")));
        if (mimeTypeDescription && CFGetTypeID(mimeTypeDescription) != CFStringGetTypeID())
            mimeTypeDescription = 0;

        mimeClassInfo.type = String(mimeType).lower();
        mimeClassInfo.desc = mimeTypeDescription;

        // Now get the extensions for this MIME type.
        CFIndex numExtensions = 0;
        CFArrayRef extensionsArray = static_cast<CFArrayRef>(CFDictionaryGetValue(mimeTypeInfo, CFSTR("WebPluginExtensions")));
        if (extensionsArray && CFGetTypeID(extensionsArray) == CFArrayGetTypeID())
            numExtensions = CFArrayGetCount(extensionsArray);

        for (CFIndex i = 0; i < numExtensions; ++i) {
            CFStringRef extension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(extensionsArray, i));
            if (!extension || CFGetTypeID(extension) != CFStringGetTypeID())
                continue;

            // The DivX plug-in lists multiple extensions in a comma separated string instead of using
            // multiple array elements in the property list. Work around this here by splitting the
            // extension string into components.
            Vector<String> extensionComponents;
            String(extension).lower().split(',', extensionComponents);

            for (size_t i = 0; i < extensionComponents.size(); ++i)
                mimeClassInfo.extensions.append(extensionComponents[i]);
        }

        // Add this MIME type.
        plugin.info.mimes.append(mimeClassInfo);
    }

    return true;    
}

bool NetscapePluginModule::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
    RetainPtr<CFURLRef> bundleURL = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pluginPath.createCFString().get(), kCFURLPOSIXPathStyle, false));
    
    // Try to initialize the bundle.
    RetainPtr<CFBundleRef> bundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, bundleURL.get()));
    if (!bundle)
        return false;
    
    // Check if this bundle is an NPAPI plug-in.
    UInt32 packageType = 0;
    CFBundleGetPackageInfo(bundle.get(), &packageType, 0);
    if (packageType != FOUR_CHAR_CODE('BRPL'))
        return false;
    
    // Check that the architecture is valid.
    if (!getPluginArchitecture(bundle.get(), plugin))
        return false;

    plugin.path = pluginPath;
    plugin.bundleIdentifier = CFBundleGetIdentifier(bundle.get());
    if (CFTypeRef versionTypeRef = CFBundleGetValueForInfoDictionaryKey(bundle.get(), kCFBundleVersionKey)) {
        if (CFGetTypeID(versionTypeRef) == CFStringGetTypeID())
            plugin.versionString = static_cast<CFStringRef>(versionTypeRef);
    }

    if (CFTypeRef shortVersionTypeRef = CFBundleGetValueForInfoDictionaryKey(bundle.get(), CFSTR("CFBundleShortVersionString"))) {
        if (CFGetTypeID(shortVersionTypeRef) == CFStringGetTypeID())
            plugin.shortVersionString = static_cast<CFStringRef>(shortVersionTypeRef);
    }

    if (CFTypeRef preferencePathTypeRef = CFBundleGetValueForInfoDictionaryKey(bundle.get(), CFSTR("WebPluginPreferencePanePath"))) {
        if (CFGetTypeID(preferencePathTypeRef) == CFStringGetTypeID())
            plugin.preferencePanePath = static_cast<CFStringRef>(preferencePathTypeRef);
    }

    if (!getPluginInfoFromPropertyLists(bundle.get(), plugin))
        return false;

    plugin.hasSandboxProfile = pluginHasSandboxProfile(plugin.bundleIdentifier);

    RetainPtr<CFStringRef> filename = adoptCF(CFURLCopyLastPathComponent(bundleURL.get()));
    plugin.info.file = filename.get();
    
    if (plugin.info.name.isNull())
        plugin.info.name = plugin.info.file;
    if (plugin.info.desc.isNull())
        plugin.info.desc = plugin.info.file;

    plugin.info.isApplicationPlugin = false;
    plugin.info.clientLoadPolicy = PluginLoadClientPolicyUndefined;
#if PLATFORM(MAC)
    plugin.info.bundleIdentifier = plugin.bundleIdentifier;
    plugin.info.versionString = plugin.versionString;
#endif

    return true;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
bool NetscapePluginModule::createPluginMIMETypesPreferences(const String& pluginPath)
{
    RetainPtr<CFURLRef> bundleURL = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pluginPath.createCFString().get(), kCFURLPOSIXPathStyle, false));
    
    RetainPtr<CFBundleRef> bundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, bundleURL.get()));
    if (!bundle)
        return false;

    if (!CFBundleLoadExecutable(bundle.get()))
        return false;

    void (*createPluginMIMETypesPreferences)(void) = reinterpret_cast<void (*)(void)>(CFBundleGetFunctionPointerForName(bundle.get(), CFSTR("BP_CreatePluginMIMETypesPreferences")));
    if (!createPluginMIMETypesPreferences)
        return false;

    createPluginMIMETypesPreferences();
    return true;
}
#endif

// FIXME: This doesn't need to be platform-specific.
class PluginVersion {
public:
    static PluginVersion parse(const String& versionString);

    bool isLessThan(unsigned componentA) const;
    bool isValid() const { return !m_versionComponents.isEmpty(); }

private:
    PluginVersion()
    {
    }

    Vector<unsigned, 4> m_versionComponents;
};

PluginVersion PluginVersion::parse(const String& versionString)
{
    PluginVersion version;

    Vector<String> versionStringComponents;
    versionString.split('.', versionStringComponents);
    for (size_t i = 0; i < versionStringComponents.size(); ++i) {
        bool successfullyParsed = false;
        unsigned versionComponent = versionStringComponents[i].toUInt(&successfullyParsed);
        if (!successfullyParsed)
            return PluginVersion();

        version.m_versionComponents.append(versionComponent);
    }

    return version;
}

bool PluginVersion::isLessThan(unsigned componentA) const
{
    ASSERT(isValid());

    return m_versionComponents[0] < componentA;
}

void NetscapePluginModule::determineQuirks()
{
    PluginModuleInfo plugin;
    if (!getPluginInfo(m_pluginPath, plugin))
        return;

    if (plugin.bundleIdentifier == "com.macromedia.Flash Player.plugin") {
        // Flash requires that the return value of getprogname() be "WebKitPluginHost".
        m_pluginQuirks.add(PluginQuirks::PrognameShouldBeWebKitPluginHost);

        // Flash supports snapshotting.
        m_pluginQuirks.add(PluginQuirks::SupportsSnapshotting);

        // Flash returns a retained Core Animation layer.
        m_pluginQuirks.add(PluginQuirks::ReturnsRetainedCoreAnimationLayer);

        // Flash has a bug where NSExceptions can be released too early.
        m_pluginQuirks.add(PluginQuirks::LeakAllThrownNSExceptions);
    }

    if (plugin.bundleIdentifier == "com.microsoft.SilverlightPlugin") {
        // Silverlight doesn't explicitly opt into transparency, so we'll do it whenever
        // there's a 'background' attribute that's set to a transparent color.
        m_pluginQuirks.add(PluginQuirks::MakeOpaqueUnlessTransparentSilverlightBackgroundAttributeExists);

        // Silverlight has a workaround for a leak in Safari 2. This workaround is
        // applied when the user agent does not contain "Version/3" so we append it
        // at the end of the user agent.
        m_pluginQuirks.add(PluginQuirks::AppendVersion3UserAgent);

        PluginVersion pluginVersion = PluginVersion::parse(plugin.versionString);
        if (pluginVersion.isValid()) {
            if (pluginVersion.isLessThan(4)) {
                // Versions of Silverlight prior to 4 don't retain the scriptable NPObject.
                m_pluginQuirks.add(PluginQuirks::ReturnsNonRetainedScriptableNPObject);
            }
        }
    }

    if (plugin.bundleIdentifier == "com.apple.ist.ds.appleconnect.webplugin") {
        // <rdar://problem/8440903>: AppleConnect has a bug where it does not
        // understand the parameter names specified in the <object> element that
        // embeds its plug-in. 
        m_pluginQuirks.add(PluginQuirks::WantsLowercaseParameterNames);

#ifndef NP_NO_QUICKDRAW
        // The AppleConnect plug-in uses QuickDraw but doesn't paint or receive events
        // so we'll allow it to be instantiated even though we don't support QuickDraw.
        m_pluginQuirks.add(PluginQuirks::AllowHalfBakedQuickDrawSupport);
#endif
    }

#ifndef NP_NO_QUICKDRAW
    if (plugin.bundleIdentifier == "com.microsoft.sharepoint.browserplugin") {
        // The Microsoft SharePoint plug-in uses QuickDraw but doesn't paint or receive events
        // so we'll allow it to be instantiated even though we don't support QuickDraw.
        m_pluginQuirks.add(PluginQuirks::AllowHalfBakedQuickDrawSupport);
    }

    if (plugin.bundleIdentifier == "com.jattesaker.macid2.NPPlugin") {
        // The BankID plug-in uses QuickDraw but doesn't paint or receive events
        // so we'll allow it to be instantiated even though we don't support QuickDraw.
        m_pluginQuirks.add(PluginQuirks::AllowHalfBakedQuickDrawSupport);
    }
#endif

    if (plugin.bundleIdentifier == "com.adobe.acrobat.pdfviewerNPAPI" || plugin.bundleIdentifier == "com.apple.testnetscapeplugin") {
        // The Adobe Reader plug-in wants wheel events.
        m_pluginQuirks.add(PluginQuirks::WantsWheelEvents);
    }
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
