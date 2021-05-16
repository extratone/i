/*
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 *
 */

#include "config.h"
#include "FormControlElement.h"

#include "Element.h"
#include "HTMLFormControlElement.h"
#include <wtf/Assertions.h>

#if ENABLE(WML)
#include "WMLFormControlElement.h"
#endif

namespace WebCore {

FormControlElement* toFormControlElement(Element* element)
{
    if (!element->isFormControlElement())
        return 0;

    if (element->isHTMLElement())
        return static_cast<HTMLFormControlElement*>(element);

#if ENABLE(WML)
    if (element->isWMLElement())
        return static_cast<WMLFormControlElement*>(element);
#endif

    return 0;
}

}
