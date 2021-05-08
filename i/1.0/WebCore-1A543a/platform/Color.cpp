/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc. All rights reserved.
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
#include "Color.h"

#include "DeprecatedString.h"
#include "PlatformString.h"
#include <wtf/Assertions.h>

#include "ColorData.c"

using namespace std;

namespace WebCore {

RGBA32 makeRGB(int r, int g, int b)
{
    return 0xFF000000 | max(0, min(r, 255)) << 16 | max(0, min(g, 255)) << 8 | max(0, min(b, 255));
}

RGBA32 makeRGBA(int r, int g, int b, int a)
{
    return max(0, min(a, 255)) << 24 | max(0, min(r, 255)) << 16 | max(0, min(g, 255)) << 8 | max(0, min(b, 255));
}

double calcHue(double temp1, double temp2, double hueVal)
{
    if (hueVal < 0)
        hueVal++;
    else if (hueVal > 1)
        hueVal--;
    if (hueVal * 6 < 1)
        return temp1 + (temp2 - temp1) * hueVal * 6;
    if (hueVal * 2 < 1)
        return temp2;
    if (hueVal * 3 < 2)
        return temp1 + (temp2 - temp1) * (2.0 / 3.0 - hueVal) * 6;
    return temp1;
}

// Explanation of this algorithm can be found in the CSS3 Color Module
// specification at http://www.w3.org/TR/css3-color/#hsl-color with further
// explanation available at http://en.wikipedia.org/wiki/HSL_color_space 

// all values are in the range of 0 to 1.0
RGBA32 makeRGBAFromHSLA(double h, double s, double l, double a)
{
    double temp2 = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    double temp1 = 2.0 * l - temp2;
    
    return makeRGBA(static_cast<int>(calcHue(temp1, temp2, h + 1.0 / 3.0) * 255), 
                    static_cast<int>(calcHue(temp1, temp2, h) * 255),
                    static_cast<int>(calcHue(temp1, temp2, h - 1.0 / 3.0) * 255),
                    static_cast<int>(a * 255));
}

// originally moved here from the CSS parser
static inline bool parseHexColor(const String& name, RGBA32& rgb)
{
    int len = name.length();
    if (!len)
        return false;

    if (len == 3 || len == 6) {
        bool ok;
        int val = name.deprecatedString().toInt(&ok, 16);
        if (ok) {
            if (len == 6) {
                rgb = 0xFF000000 | val;
                return true;
            }
            // #abc converts to #aabbcc according to the specs
            rgb = 0xFF000000
                | (val & 0xF00) << 12 | (val & 0xF00) << 8
                | (val & 0xF0) << 8 | (val & 0xF0) << 4
                | (val & 0xF) << 4 | (val & 0xF);
            return true;
        }
    }
    return false;
}

int differenceSquared(const Color& c1, const Color& c2)
{
    int dR = c1.red() - c2.red();
    int dG = c1.green() - c2.green();
    int dB = c1.blue() - c2.blue();
    return dR * dR + dG * dG + dB * dB;
}

Color::Color(const String& name)
{
    if (name.startsWith("#"))
        valid = parseHexColor(name.substring(1), color);
    else
        setNamedColor(name);
}

Color::Color(const char* name)
{
    if (name[0] == '#')
        valid = parseHexColor(&name[1], color);
    else {
        const NamedColor* foundColor = findColor(name, strlen(name));
        color = foundColor ? foundColor->RGBValue : 0;
        color |= 0xFF000000;
        valid = foundColor;
    }
}

String Color::name() const
{
    String name;
    if (alpha() < 0xFF)
        name = String::sprintf("#%02X%02X%02X%02X", red(), green(), blue(), alpha());
    else
        name = String::sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

void Color::setNamedColor(const String& name)
{
    DeprecatedString dname = name.deprecatedString();
    const NamedColor* foundColor = dname.isAllASCII() ? findColor(dname.latin1(), dname.length()) : 0;
    color = foundColor ? foundColor->RGBValue : 0;
    color |= 0xFF000000;
    valid = foundColor;
}

const float undefinedHue = -1;

// Explanations of these algorithms can be found at http://www.acm.org/jgt/papers/SmithLyons96/hsv_rgb.html
static void convertRGBToHSV(float r, float g, float b, float& h, float& s, float& v)
{
    float x = min(r, min(g, b));
    v = max(r, max(g, b));

    if (v == x) {
        h = undefinedHue;
        s = 0;
    } else {
        float f = (r == x) ? g - b : ((g == x) ? b - r : r - g); 
        int i = (r == x) ? 3 : ((g == x) ? 5 : 1); 
        h = i - f / (v - x);
        if (v != 0)
            s = (v - x) / v;
        else
            s = 0;
    }
}

static void convertHSVToRGB(float h, float s, float v, float& r, float& g, float& b)
{
    if (h == undefinedHue) {
        r = v;
        g = v;
        b = v;
        return;
    }

    int i = static_cast<int>(h); // sector 0 to 5
    float f = h - i; // fractional part of h
    if (!(i & 1))
        f = 1 - f;
    float m = v * (1 - s);
    float n = v * (1 - s * f);

    switch (i) {
        default: // 0 and 6
            r = v;
            g = n;
            b = m;
            break;
        case 1:
            r = n;
            g = v;
            b = m;
            break;
        case 2:
            r = m;
            g = v;
            b = n;
            break;
        case 3:
            r = m;
            g = n;
            b = v;
            break;
        case 4:
            r = n;
            g = m;
            b = v;
            break;
        case 5:
            r = v;
            g = m;
            b = n;
            break;
    }
}


Color Color::light() const
{
    float r, g, b, a, h, s, v;
    getRGBA(r, g, b, a);
    convertRGBToHSV(r, g, b, h, s, v);
    v = max(0.0f, min(v + 0.33f, 1.0f));
    convertHSVToRGB(h, s, v, r, g, b);
    return Color((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

Color Color::dark() const
{
    float r, g, b, a, h, s, v;
    getRGBA(r, g, b, a);
    convertRGBToHSV(r, g, b, h, s, v);
    v = max(0.0f, min(v - 0.33f, 1.0f));
    convertHSVToRGB(h, s, v, r, g, b);
    return Color((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

bool Color::isDark() const
{
    float r, g, b, a, h, s, v;
    getRGBA(r, g, b, a);
    convertRGBToHSV(r, g, b, h, s, v);
    
    return a > 0.5 && v < 0.5;
}

static int blend(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c / alpha);
}

const int cStartAlpha = 153; // 60%
const int cEndAlpha = 204; // 80%;
const int cAlphaIncrement = 17; // Increments in between.

Color Color::blendWithWhite() const
{
    // If the color contains alpha already, we leave it alone.
    if (hasAlpha())
        return *this;

    Color newColor;
    for (int alpha = cStartAlpha; alpha <= cEndAlpha; alpha += cAlphaIncrement) {
        // We have a solid color.  Convert to an equivalent color that looks the same when blended with white
        // at the current alpha.  Try using less transparency if the numbers end up being negative.
        int r = blend(red(), alpha);
        int g = blend(green(), alpha);
        int b = blend(blue(), alpha);
        
        newColor = Color(r, g, b, alpha);

        if (r >= 0 && g >= 0 && b >= 0)
            break;
    }
    return newColor;
}

void Color::getRGBA(float& r, float& g, float& b, float& a) const
{
    r = red() / 255.0f;
    g = green() / 255.0f;
    b = blue() / 255.0f;
    a = alpha() / 255.0f;
}

void Color::getRGBA(double& r, double& g, double& b, double& a) const
{
    r = red() / 255.0;
    g = green() / 255.0;
    b = blue() / 255.0;
    a = alpha() / 255.0;
}

}
