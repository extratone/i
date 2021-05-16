/*
    Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>

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
*/

#include "config.h"

#if ENABLE(SVG)
#include "JSSVGLength.h"

using namespace JSC;

namespace WebCore {

JSValuePtr JSSVGLength::value(ExecState* exec) const
{
    SVGLength imp(*impl());
    return jsNumber(exec, imp.value(context()));
}

JSValuePtr JSSVGLength::convertToSpecifiedUnits(ExecState* exec, const ArgList& args)
{
    JSSVGPODTypeWrapper<SVGLength>* wrapper = impl();

    SVGLength imp(*wrapper);
    imp.convertToSpecifiedUnits(args.at(exec, 0).toInt32(exec), context());

    wrapper->commitChange(imp, context());
    return jsUndefined();
}

}

#endif // ENABLE(SVG)
