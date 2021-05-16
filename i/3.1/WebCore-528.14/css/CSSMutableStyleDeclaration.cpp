/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSMutableStyleDeclaration.h"

#include "CSSImageValue.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyLonghand.h"
#include "CSSPropertyNames.h"
#include "CSSRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "StyledElement.h"

using namespace std;

namespace WebCore {

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration()
    : m_node(0)
    , m_variableDependentValueCount(0)
    , m_strictParsing(false)
#ifndef NDEBUG
    , m_iteratorCount(0)
#endif
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent)
    : CSSStyleDeclaration(parent)
    , m_node(0)
    , m_variableDependentValueCount(0)
    , m_strictParsing(!parent || parent->useStrictParsing())
#ifndef NDEBUG
    , m_iteratorCount(0)
#endif
{
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const Vector<CSSProperty>& properties, unsigned variableDependentValueCount)
    : CSSStyleDeclaration(parent)
    , m_properties(properties)
    , m_node(0)
    , m_variableDependentValueCount(variableDependentValueCount)
    , m_strictParsing(!parent || parent->useStrictParsing())
#ifndef NDEBUG
    , m_iteratorCount(0)
#endif
{
    m_properties.shrinkToFit();
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration::CSSMutableStyleDeclaration(CSSRule* parent, const CSSProperty* const * properties, int numProperties)
    : CSSStyleDeclaration(parent)
    , m_node(0)
    , m_variableDependentValueCount(0)
    , m_strictParsing(!parent || parent->useStrictParsing())
#ifndef NDEBUG
    , m_iteratorCount(0)
#endif
{
    m_properties.reserveInitialCapacity(numProperties);
    for (int i = 0; i < numProperties; ++i) {
        ASSERT(properties[i]);
        m_properties.append(*properties[i]);
        if (properties[i]->value()->isVariableDependentValue())
            m_variableDependentValueCount++;
    }
    // FIXME: This allows duplicate properties.
}

CSSMutableStyleDeclaration& CSSMutableStyleDeclaration::operator=(const CSSMutableStyleDeclaration& other)
{
    ASSERT(!m_iteratorCount);
    // don't attach it to the same node, just leave the current m_node value
    m_properties = other.m_properties;
    m_strictParsing = other.m_strictParsing;
    return *this;
}

String CSSMutableStyleDeclaration::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    // Shorthand and 4-values properties
    switch (propertyID) {
        case CSSPropertyBackgroundPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyBackgroundPositionX,
                                        CSSPropertyBackgroundPositionY };
            return getLayeredShorthandValue(properties, 2);
        }
        case CSSPropertyBackground: {
            const int properties[7] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, 
                                        CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyWebkitBackgroundClip,
                                        CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundColor };
            return getLayeredShorthandValue(properties, 7);
        }
        case CSSPropertyBorder: {
            const int properties[3][4] = {{ CSSPropertyBorderTopWidth,
                                            CSSPropertyBorderRightWidth,
                                            CSSPropertyBorderBottomWidth,
                                            CSSPropertyBorderLeftWidth },
                                          { CSSPropertyBorderTopStyle,
                                            CSSPropertyBorderRightStyle,
                                            CSSPropertyBorderBottomStyle,
                                            CSSPropertyBorderLeftStyle },
                                          { CSSPropertyBorderTopColor,
                                            CSSPropertyBorderRightColor,
                                            CSSPropertyBorderBottomColor,
                                            CSSPropertyBorderLeftColor }};
            String res;
            const int nrprops = sizeof(properties) / sizeof(properties[0]);
            for (int i = 0; i < nrprops; ++i) {
                String value = getCommonValue(properties[i], 4);
                if (!value.isNull()) {
                    if (!res.isNull())
                        res += " ";
                    res += value;
                }
            }
            return res;
        }
        case CSSPropertyBorderTop: {
            const int properties[3] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle,
                                        CSSPropertyBorderTopColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderRight: {
            const int properties[3] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle,
                                        CSSPropertyBorderRightColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderBottom: {
            const int properties[3] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle,
                                        CSSPropertyBorderBottomColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderLeft: {
            const int properties[3] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle,
                                        CSSPropertyBorderLeftColor};
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyOutline: {
            const int properties[3] = { CSSPropertyOutlineWidth, CSSPropertyOutlineStyle,
                                        CSSPropertyOutlineColor };
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyBorderColor: {
            const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor,
                                        CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
            return get4Values(properties);
        }
        case CSSPropertyBorderWidth: {
            const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth,
                                        CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
            return get4Values(properties);
        }
        case CSSPropertyBorderStyle: {
            const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle,
                                        CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
            return get4Values(properties);
        }
        case CSSPropertyMargin: {
            const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight,
                                        CSSPropertyMarginBottom, CSSPropertyMarginLeft };
            return get4Values(properties);
        }
        case CSSPropertyOverflow: {
            const int properties[2] = { CSSPropertyOverflowX, CSSPropertyOverflowY };
            return getCommonValue(properties, 2);
        }
        case CSSPropertyPadding: {
            const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight,
                                        CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
            return get4Values(properties);
        }
        case CSSPropertyListStyle: {
            const int properties[3] = { CSSPropertyListStyleType, CSSPropertyListStylePosition,
                                        CSSPropertyListStyleImage };
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyWebkitMaskPosition: {
            // FIXME: Is this correct? The code in cssparser.cpp is confusing
            const int properties[2] = { CSSPropertyWebkitMaskPositionX,
                                        CSSPropertyWebkitMaskPositionY };
            return getLayeredShorthandValue(properties, 2);
        }
        case CSSPropertyWebkitMask: {
            const int properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat, 
                                       CSSPropertyWebkitMaskAttachment, CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskClip,
                                       CSSPropertyWebkitMaskOrigin };
            return getLayeredShorthandValue(properties, 6);
        }
        case CSSPropertyWebkitTransformOrigin: {
            const int properties[3] = { CSSPropertyWebkitTransformOriginX,
                                        CSSPropertyWebkitTransformOriginY,
                                        CSSPropertyWebkitTransformOriginZ };
            return getShorthandValue(properties, 3);
        }
        case CSSPropertyWebkitTransition: {
            const int properties[4] = { CSSPropertyWebkitTransitionProperty, CSSPropertyWebkitTransitionDuration,
                                        CSSPropertyWebkitTransitionTimingFunction, CSSPropertyWebkitTransitionDelay };
            return getLayeredShorthandValue(properties, 4);
        }
        case CSSPropertyWebkitAnimation: {
            const int properties[6] = { CSSPropertyWebkitAnimationName, CSSPropertyWebkitAnimationDuration,
                                        CSSPropertyWebkitAnimationTimingFunction, CSSPropertyWebkitAnimationDelay,
                                        CSSPropertyWebkitAnimationIterationCount, CSSPropertyWebkitAnimationDirection };
            return getLayeredShorthandValue(properties, 6);
        }
#if ENABLE(SVG)
        case CSSPropertyMarker: {
            RefPtr<CSSValue> value = getPropertyCSSValue(CSSPropertyMarkerStart);
            if (value)
                return value->cssText();
        }
#endif
    }
    return String();
}

String CSSMutableStyleDeclaration::get4Values(const int* properties) const
{
    String res;
    for (int i = 0; i < 4; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);

            // apparently all 4 properties must be specified.
            if (!value)
                return String();

            if (!res.isNull())
                res += " ";
            res += value->cssText();
        }
    }
    return res;
}

String CSSMutableStyleDeclaration::getLayeredShorthandValue(const int* properties, unsigned number) const
{
    String res;

    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(number);
    size_t numLayers = 0;
    
    for (size_t i = 0; i < number; ++i) {
        values[i] = getPropertyCSSValue(properties[i]);
        if (values[i]) {
            if (values[i]->isValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(values[i].get());
                numLayers = max(valueList->length(), numLayers);
            } else
                numLayers = max<size_t>(1U, numLayers);
        }
    }
    
    // Now stitch the properties together.  Implicit initial values are flagged as such and
    // can safely be omitted.
    for (size_t i = 0; i < numLayers; i++) {
        String layerRes;
        for (size_t j = 0; j < number; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isValueList())
                    value = static_cast<CSSValueList*>(values[j].get())->itemWithoutBoundsCheck(i);
                else {
                    value = values[j];
                    
                    // Color only belongs in the last layer.
                    if (properties[j] == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = 0;
                    } else if (i != 0) // Other singletons only belong in the first layer.
                        value = 0;
                }
            }
            
            if (value && !value->isImplicitInitialValue()) {
                if (!layerRes.isNull())
                    layerRes += " ";
                layerRes += value->cssText();
            }
        }
        
        if (!layerRes.isNull()) {
            if (!res.isNull())
                res += ", ";
            res += layerRes;
        }
    }

    return res;
}

String CSSMutableStyleDeclaration::getShorthandValue(const int* properties, int number) const
{
    String res;
    for (int i = 0; i < number; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
            // FIXME: provide default value if !value
            if (value) {
                if (!res.isNull())
                    res += " ";
                res += value->cssText();
            }
        }
    }
    return res;
}

// only returns a non-null value if all properties have the same, non-null value
String CSSMutableStyleDeclaration::getCommonValue(const int* properties, int number) const
{
    String res;
    for (int i = 0; i < number; ++i) {
        if (!isPropertyImplicit(properties[i])) {
            RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
            if (!value)
                return String();
            String text = value->cssText();
            if (text.isNull())
                return String();
            if (res.isNull())
                res = text;
            else if (res != text)
                return String();
        }
    }
    return res;
}

PassRefPtr<CSSValue> CSSMutableStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->value() : 0;
}

bool CSSMutableStyleDeclaration::removeShorthandProperty(int propertyID, bool notifyChanged) 
{
    CSSPropertyLonghand longhand = longhandForProperty(propertyID);
    if (longhand.length()) {
        removePropertiesInSet(longhand.properties(), longhand.length(), notifyChanged);
        return true;
    }
    return false;
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, bool notifyChanged, bool returnText)
{
    ASSERT(!m_iteratorCount);

    if (removeShorthandProperty(propertyID, notifyChanged)) {
        // FIXME: Return an equivalent shorthand when possible.
        return String();
    }
   
    CSSProperty* foundProperty = findPropertyWithId(propertyID);
    if (!foundProperty)
        return String();
    
    String value = returnText ? foundProperty->value()->cssText() : String();

    if (foundProperty->value()->isVariableDependentValue())
        m_variableDependentValueCount--;

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_properties.remove(foundProperty - m_properties.data());

    if (notifyChanged)
        setChanged();

    return value;
}

void CSSMutableStyleDeclaration::setChanged()
{
    if (m_node) {
        // FIXME: Ideally, this should be factored better and there
        // should be a subclass of CSSMutableStyleDeclaration just
        // for inline style declarations that handles this
        bool isInlineStyleDeclaration = m_node->isStyledElement() && this == static_cast<StyledElement*>(m_node)->inlineStyleDecl();
        if (isInlineStyleDeclaration) {
            m_node->setChanged(InlineStyleChange);
            static_cast<StyledElement*>(m_node)->invalidateStyleAttribute();
        } else
            m_node->setChanged(FullStyleChange);
        return;
    }

    // FIXME: quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
    StyleBase* root = this;
    while (StyleBase* parent = root->parent())
        root = parent;
    if (root->isCSSStyleSheet())
        static_cast<CSSStyleSheet*>(root)->doc()->updateStyleSelector();
}

bool CSSMutableStyleDeclaration::getPropertyPriority(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->isImportant() : false;
}

int CSSMutableStyleDeclaration::getPropertyShorthand(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->shorthandID() : 0;
}

bool CSSMutableStyleDeclaration::isPropertyImplicit(int propertyID) const
{
    const CSSProperty* property = findPropertyWithId(propertyID);
    return property ? property->isImplicit() : false;
}

void CSSMutableStyleDeclaration::setProperty(int propertyID, const String& value, bool important, ExceptionCode& ec)
{
    ec = 0;
    setProperty(propertyID, value, important, true);
}

String CSSMutableStyleDeclaration::removeProperty(int propertyID, ExceptionCode& ec)
{
    ec = 0;
    return removeProperty(propertyID, true, true);
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, const String& value, bool important, bool notifyChanged)
{
    ASSERT(!m_iteratorCount);

    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty()) {
        removeProperty(propertyID, notifyChanged, false);
        return true;
    }

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    CSSParser parser(useStrictParsing());
    bool success = parser.parseValue(this, propertyID, value, important);
    if (!success) {
        // CSS DOM requires raising SYNTAX_ERR here, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
    } else if (notifyChanged)
        setChanged();

    return success;
}
    
void CSSMutableStyleDeclaration::setPropertyInternal(const CSSProperty& property, CSSProperty* slot)
{
    ASSERT(!m_iteratorCount);

    if (!removeShorthandProperty(property.id(), false)) {
        CSSProperty* toReplace = slot ? slot : findPropertyWithId(property.id());
        if (toReplace) {
            *toReplace = property;
            return;
        }
    }
    m_properties.append(property);
}

bool CSSMutableStyleDeclaration::setProperty(int propertyID, int value, bool important, bool notifyChanged)
{
    CSSProperty property(propertyID, CSSPrimitiveValue::createIdentifier(value), important);
    setPropertyInternal(property);
    if (notifyChanged)
        setChanged();
    return true;
}

void CSSMutableStyleDeclaration::setStringProperty(int propertyId, const String &value, CSSPrimitiveValue::UnitTypes type, bool important)
{
    ASSERT(!m_iteratorCount);

    setPropertyInternal(CSSProperty(propertyId, CSSPrimitiveValue::create(value, type), important));
    setChanged();
}

void CSSMutableStyleDeclaration::setImageProperty(int propertyId, const String& url, bool important)
{
    ASSERT(!m_iteratorCount);

    setPropertyInternal(CSSProperty(propertyId, CSSImageValue::create(url), important));
    setChanged();
}

void CSSMutableStyleDeclaration::parseDeclaration(const String& styleDeclaration)
{
    ASSERT(!m_iteratorCount);

    m_properties.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, styleDeclaration);
    setChanged();
}

void CSSMutableStyleDeclaration::addParsedProperties(const CSSProperty* const* properties, int numProperties)
{
    ASSERT(!m_iteratorCount);
    
    m_properties.reserveCapacity(numProperties);
    
    for (int i = 0; i < numProperties; ++i) {
        // Only add properties that have no !important counterpart present
        if (!getPropertyPriority(properties[i]->id()) || properties[i]->isImportant()) {
            removeProperty(properties[i]->id(), false);
            ASSERT(properties[i]);
            m_properties.append(*properties[i]);
            if (properties[i]->value()->isVariableDependentValue())
                m_variableDependentValueCount++;
        }
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSMutableStyleDeclaration::addParsedProperty(const CSSProperty& property)
{
    ASSERT(!m_iteratorCount);

    setPropertyInternal(property);
}

void CSSMutableStyleDeclaration::setLengthProperty(int propertyId, const String& value, bool important, bool /*multiLength*/)
{
    ASSERT(!m_iteratorCount);

    bool parseMode = useStrictParsing();
    setStrictParsing(false);
    setProperty(propertyId, value, important);
    setStrictParsing(parseMode);
}

unsigned CSSMutableStyleDeclaration::length() const
{
    return m_properties.size();
}

String CSSMutableStyleDeclaration::item(unsigned i) const
{
    if (i >= m_properties.size())
       return String();
    return getPropertyName(static_cast<CSSPropertyID>(m_properties[i].id()));
}

String CSSMutableStyleDeclaration::cssText() const
{
    String result = "";
    
    const CSSProperty* positionXProp = 0;
    const CSSProperty* positionYProp = 0;
    
    unsigned size = m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& prop = m_properties[n];
        if (prop.id() == CSSPropertyBackgroundPositionX)
            positionXProp = &prop;
        else if (prop.id() == CSSPropertyBackgroundPositionY)
            positionYProp = &prop;
        else
            result += prop.cssText();
    }
    
    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position in output.
    // It is required because background-position-x/y are non-standard properties and WebKit generated output 
    // would not work in Firefox (<rdar://problem/5143183>)
    // It would be a better solution if background-position was CSS_PAIR.
    if (positionXProp && positionYProp && positionXProp->isImportant() == positionYProp->isImportant()) {
        String positionValue;
        const int properties[2] = { CSSPropertyBackgroundPositionX, CSSPropertyBackgroundPositionY };
        if (positionXProp->value()->isValueList() || positionYProp->value()->isValueList()) 
            positionValue = getLayeredShorthandValue(properties, 2);
        else
            positionValue = positionXProp->value()->cssText() + " " + positionYProp->value()->cssText();
        result += "background-position: " + positionValue + (positionXProp->isImportant() ? " !important" : "") + "; ";
    } else {
        if (positionXProp) 
            result += positionXProp->cssText();
        if (positionYProp)
            result += positionYProp->cssText();
    }

    return result;
}

void CSSMutableStyleDeclaration::setCssText(const String& text, ExceptionCode& ec)
{
    ASSERT(!m_iteratorCount);

    ec = 0;
    m_properties.clear();
    CSSParser parser(useStrictParsing());
    parser.parseDeclaration(this, text);
    // FIXME: Detect syntax errors and set ec.
    setChanged();
}

void CSSMutableStyleDeclaration::merge(CSSMutableStyleDeclaration* other, bool argOverridesOnConflict)
{
    ASSERT(!m_iteratorCount);

    unsigned size = other->m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        CSSProperty& toMerge = other->m_properties[n];
        CSSProperty* old = findPropertyWithId(toMerge.id());
        if (old) {
            if (!argOverridesOnConflict && old->value())
                continue;
            setPropertyInternal(toMerge, old);
        } else
            m_properties.append(toMerge);
    }
    // FIXME: This probably should have a call to setChanged() if something changed. We may also wish to add
    // a notifyChanged argument to this function to follow the model of other functions in this class.
}

void CSSMutableStyleDeclaration::addSubresourceStyleURLs(ListHashSet<KURL>& urls)
{
    CSSStyleSheet* sheet = static_cast<CSSStyleSheet*>(stylesheet());
    size_t size = m_properties.size();
    for (size_t i = 0; i < size; ++i)
        m_properties[i].value()->addSubresourceStyleURLs(urls, sheet);
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const int blockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

const unsigned numBlockProperties = sizeof(blockProperties) / sizeof(blockProperties[0]);

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclaration::removeBlockProperties()
{
    removePropertiesInSet(blockProperties, numBlockProperties);
}

void CSSMutableStyleDeclaration::removePropertiesInSet(const int* set, unsigned length, bool notifyChanged)
{
    ASSERT(!m_iteratorCount);
    
    if (m_properties.isEmpty())
        return;
    
    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<int> toRemove;
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);
    
    Vector<CSSProperty> newProperties;
    newProperties.reserveInitialCapacity(m_properties.size());
    
    unsigned size = m_properties.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& property = m_properties[n];
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        if (!property.isImportant()) {
            if (toRemove.contains(property.id()))
                continue;
        }
        newProperties.append(property);
    }

    bool changed = newProperties.size() != m_properties.size();
    m_properties = newProperties;
    
    if (changed && notifyChanged)
        setChanged();
}

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::makeMutable()
{
    return this;
}

PassRefPtr<CSSMutableStyleDeclaration> CSSMutableStyleDeclaration::copy() const
{
    return adoptRef(new CSSMutableStyleDeclaration(0, m_properties, m_variableDependentValueCount));
}

const CSSProperty* CSSMutableStyleDeclaration::findPropertyWithId(int propertyID) const
{    
    for (int n = m_properties.size() - 1 ; n >= 0; --n) {
        if (propertyID == m_properties[n].m_id)
            return &m_properties[n];
    }
    return 0;
}

CSSProperty* CSSMutableStyleDeclaration::findPropertyWithId(int propertyID)
{
    for (int n = m_properties.size() - 1 ; n >= 0; --n) {
        if (propertyID == m_properties[n].m_id)
            return &m_properties[n];
    }
    return 0;
}

} // namespace WebCore
