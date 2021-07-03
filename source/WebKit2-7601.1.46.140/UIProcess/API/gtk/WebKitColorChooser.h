/*
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef WebKitColorChooser_h
#define WebKitColorChooser_h

#include "WebColorPickerGtk.h"
#include "WebKitPrivate.h"
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitColorChooserRequest WebKitColorChooserRequest;

namespace WebCore {
class Color;
class IntRect;
}

namespace WebKit {

class WebKitColorChooser final : public WebColorPickerGtk {
public:
    static Ref<WebKitColorChooser> create(WebPageProxy&, const WebCore::Color&, const WebCore::IntRect&);
    virtual ~WebKitColorChooser();

    const WebCore::IntRect& elementRect() const { return m_elementRect; }

private:
    WebKitColorChooser(WebPageProxy&, const WebCore::Color&, const WebCore::IntRect&);

    virtual void endPicker() override;
    virtual void showColorPicker(const WebCore::Color&) override;

    static void colorChooserRequestFinished(WebKitColorChooserRequest*, WebKitColorChooser*);
    static void colorChooserRequestRGBAChanged(WebKitColorChooserRequest*, GParamSpec*, WebKitColorChooser*);

    GRefPtr<WebKitColorChooserRequest> m_request;
    WebCore::IntRect m_elementRect;
};

} // namespace WebKit

#endif // WebKitColorChooser_h
