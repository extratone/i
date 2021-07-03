/*
 * Copyright (C) 2007, 2008, Apple Inc. All rights reserved.
 */

#include "config.h"
#include "MIMETypeRegistry.h"

#include "WebCoreSystemInterface.h"

namespace WebCore 
{
String MIMETypeRegistry::getMIMETypeForExtension(const String &ext)
{
    return wkGetMIMETypeForExtension(ext);
}

Vector<String> MIMETypeRegistry::getExtensionsForMIMEType(const String& /*type*/)
{
    return Vector<String>();
}

String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& /*type*/)
{
    return String();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

}

