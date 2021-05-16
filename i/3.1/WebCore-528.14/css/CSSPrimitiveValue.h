/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#ifndef CSSPrimitiveValue_h
#define CSSPrimitiveValue_h

#include "CSSValue.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class Counter;
class DashboardRegion;
class Pair;
class Rect;
class RenderStyle;
class StringImpl;

struct Length;

class CSSPrimitiveValue : public CSSValue {
public:
    enum UnitTypes {
        CSS_UNKNOWN = 0,
        CSS_NUMBER = 1,
        CSS_PERCENTAGE = 2,
        CSS_EMS = 3,
        CSS_EXS = 4,
        CSS_PX = 5,
        CSS_CM = 6,
        CSS_MM = 7,
        CSS_IN = 8,
        CSS_PT = 9,
        CSS_PC = 10,
        CSS_DEG = 11,
        CSS_RAD = 12,
        CSS_GRAD = 13,
        CSS_MS = 14,
        CSS_S = 15,
        CSS_HZ = 16,
        CSS_KHZ = 17,
        CSS_DIMENSION = 18,
        CSS_STRING = 19,
        CSS_URI = 20,
        CSS_IDENT = 21,
        CSS_ATTR = 22,
        CSS_COUNTER = 23,
        CSS_RECT = 24,
        CSS_RGBCOLOR = 25,
        CSS_PAIR = 100, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_DASHBOARD_REGION = 101, // FIXME: Dashboard region should not be a primitive value.
        CSS_UNICODE_RANGE = 102,
        
        // These next types are just used internally to allow us to translate back and forth from CSSPrimitiveValues to CSSParserValues.
        CSS_PARSER_OPERATOR = 103,
        CSS_PARSER_INTEGER = 104,
        CSS_PARSER_VARIABLE_FUNCTION_SYNTAX = 105,
        CSS_PARSER_HEXCOLOR = 106,
        
        // This is used internally for unknown identifiers 
        CSS_PARSER_IDENTIFIER = 107,
        
        // This unit is in CSS 3, but that isn't a finished standard yet
        CSS_TURN = 108
    };
    
    static bool isUnitTypeLength(int type) { return type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG; }

    static PassRefPtr<CSSPrimitiveValue> createIdentifier(int ident);
    static PassRefPtr<CSSPrimitiveValue> createColor(unsigned rgbValue);
    static PassRefPtr<CSSPrimitiveValue> create(double value, UnitTypes type);
    static PassRefPtr<CSSPrimitiveValue> create(const String& value, UnitTypes type);
    
    template<typename T> static PassRefPtr<CSSPrimitiveValue> create(T value)
    {
        return adoptRef(new CSSPrimitiveValue(value));
    }

    virtual ~CSSPrimitiveValue();

    void cleanup();

    unsigned short primitiveType() const { return m_type; }

    bool isVariable() const { return m_type == CSS_PARSER_VARIABLE_FUNCTION_SYNTAX; }

    /*
     * computes a length in pixels out of the given CSSValue. Need the RenderStyle to get
     * the fontinfo in case val is defined in em or ex.
     *
     * The metrics have to be a bit different for screen and printer output.
     * For screen output we assume 1 inch == 72 px, for printer we assume 300 dpi
     *
     * this is screen/printer dependent, so we probably need a config option for this,
     * and some tool to calibrate.
     */
    int computeLengthInt(RenderStyle*);
    int computeLengthInt(RenderStyle*, double multiplier);
    int computeLengthIntForLength(RenderStyle*);
    int computeLengthIntForLength(RenderStyle*, double multiplier);
    short computeLengthShort(RenderStyle*);
    short computeLengthShort(RenderStyle*, double multiplier);
    float computeLengthFloat(RenderStyle*, bool computingFontSize = false);
    float computeLengthFloat(RenderStyle*, double multiplier, bool computingFontSize = false);
    double computeLengthDouble(RenderStyle*, double multiplier = 1.0, bool computingFontSize = false);

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    
    double getDoubleValue(unsigned short unitType, ExceptionCode&);
    double getDoubleValue(unsigned short unitType);
    double getDoubleValue() const { return m_value.num; }

    void setFloatValue(unsigned short unitType, double floatValue, ExceptionCode&);
    float getFloatValue(unsigned short unitType, ExceptionCode& ec) { return static_cast<float>(getDoubleValue(unitType, ec)); }
    float getFloatValue(unsigned short unitType) { return static_cast<float>(getDoubleValue(unitType)); }
    float getFloatValue() const { return static_cast<float>(m_value.num); }

    int getIntValue(unsigned short unitType, ExceptionCode& ec) { return static_cast<int>(getDoubleValue(unitType, ec)); }
    int getIntValue(unsigned short unitType) { return static_cast<int>(getDoubleValue(unitType)); }
    int getIntValue() const { return static_cast<int>(m_value.num); }

    void setStringValue(unsigned short stringType, const String& stringValue, ExceptionCode&);
    String getStringValue(ExceptionCode&) const;
    String getStringValue() const;

    Counter* getCounterValue(ExceptionCode&) const;
    Counter* getCounterValue() const { return m_type != CSS_COUNTER ? 0 : m_value.counter; }

    Rect* getRectValue(ExceptionCode&) const;
    Rect* getRectValue() const { return m_type != CSS_RECT ? 0 : m_value.rect; }

    unsigned getRGBColorValue(ExceptionCode&) const;
    unsigned getRGBColorValue() const { return m_type != CSS_RGBCOLOR ? 0 : m_value.rgbcolor; }

    Pair* getPairValue(ExceptionCode&) const;
    Pair* getPairValue() const { return m_type != CSS_PAIR ? 0 : m_value.pair; }

    DashboardRegion* getDashboardRegionValue() const { return m_type != CSS_DASHBOARD_REGION ? 0 : m_value.region; }

    int getIdent();
    template<typename T> operator T() const; // Defined in CSSPrimitiveValueMappings.h

    virtual bool parseString(const String&, bool = false);
    virtual String cssText() const;

    virtual bool isQuirkValue() { return false; }

    virtual CSSParserValue parserValue() const;

    virtual void addSubresourceStyleURLs(ListHashSet<KURL>&, const CSSStyleSheet*);

protected:
    // FIXME: int vs. unsigned overloading is too subtle to distinguish the color and identifier cases.
    CSSPrimitiveValue(int ident);
    CSSPrimitiveValue(double, UnitTypes);
    CSSPrimitiveValue(const String&, UnitTypes);

private:
    CSSPrimitiveValue();
    CSSPrimitiveValue(unsigned color); // RGB value
    CSSPrimitiveValue(const Length&);

    template<typename T> CSSPrimitiveValue(T); // Defined in CSSPrimitiveValueMappings.h
    template<typename T> CSSPrimitiveValue(T* val) { init(PassRefPtr<T>(val)); }
    template<typename T> CSSPrimitiveValue(PassRefPtr<T> val) { init(val); }

    static void create(int); // compile-time guard
    static void create(unsigned); // compile-time guard
    template<typename T> operator T*(); // compile-time guard

    void init(PassRefPtr<Counter>);
    void init(PassRefPtr<Rect>);
    void init(PassRefPtr<Pair>);
    void init(PassRefPtr<DashboardRegion>); // FIXME: Dashboard region should not be a primitive value.

    virtual bool isPrimitiveValue() const { return true; }

    virtual unsigned short cssValueType() const;

    int m_type;
    union {
        int ident;
        double num;
        StringImpl* string;
        Counter* counter;
        Rect* rect;
        unsigned rgbcolor;
        Pair* pair;
        DashboardRegion* region;
    } m_value;
};

} // namespace WebCore

#endif // CSSPrimitiveValue_h
