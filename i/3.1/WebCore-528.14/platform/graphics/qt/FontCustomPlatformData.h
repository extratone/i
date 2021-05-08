/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#ifndef FontCustomPlatformData_h_
#define FontCustomPlatformData_h_

#include "FontRenderingMode.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class SharedBuffer;
class FontPlatformData;

struct FontCustomPlatformData : Noncopyable {
    ~FontCustomPlatformData();

    // for use with QFontDatabase::addApplicationFont/removeApplicationFont
    int m_handle;

    FontPlatformData fontPlatformData(int size, bool bold, bool italic, FontRenderingMode = NormalRenderingMode);
};

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer);

} // namespace WebCore

#endif // FontCustomPlatformData_h_
