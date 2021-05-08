/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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
#include "MediaQueryExp.h"

#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"

namespace WebCore {

MediaQueryExp::MediaQueryExp(const AtomicString& mediaFeature, CSSParserValueList* valueList)
    : m_mediaFeature(mediaFeature)
    , m_value(0)
{
    if (valueList) {
        if (valueList->size() == 1) {
            CSSParserValue* value = valueList->current();

            if (value->id != 0)
                m_value = CSSPrimitiveValue::createIdentifier(value->id);
            else if (value->unit == CSSPrimitiveValue::CSS_STRING)
                m_value = CSSPrimitiveValue::create(value->string, (CSSPrimitiveValue::UnitTypes) value->unit);
            else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER &&
                      value->unit <= CSSPrimitiveValue::CSS_KHZ)
                m_value = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);

            valueList->next();
        } else if (valueList->size() > 1) {
            // create list of values
            // currently accepts only <integer>/<integer>

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            CSSParserValue* value = 0;
            bool isValid = true;

            while ((value = valueList->current()) && isValid) {
                if (value->unit == CSSParserValue::Operator && value->iValue == '/')
                    list->append(CSSPrimitiveValue::create("/", CSSPrimitiveValue::CSS_STRING));
                else if (value->unit == CSSPrimitiveValue::CSS_NUMBER)
                    list->append(CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER));
                else
                    isValid = false;

                value = valueList->next();
            }

            if (isValid)
                m_value = list.release();
        }
    }
}

MediaQueryExp::~MediaQueryExp()
{
}

} // namespace
