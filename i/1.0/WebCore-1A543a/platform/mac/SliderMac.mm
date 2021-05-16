/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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


#import "config.h"
#import "Slider.h"

#include "IntSize.h"

using namespace WebCore;

Slider::Slider()
    : m_minVal(0.0), m_maxVal(100.0), m_val(50.0)
{
    // Not implemented.
}

Slider::~Slider()
{
    // Not implemented.
}

void Slider::setFont(const Font& f)
{
    // Not implemented.
}

Widget::FocusPolicy Slider::focusPolicy() const
{
    // Not implemented.
    
    return Widget::focusPolicy();
}

IntSize Slider::sizeHint() const 
{
    // Not implemented.
    
    return IntSize(0, 0);
}

void Slider::setValue(double v)
{
    // Not implemented.
}

void Slider::setMinValue(double v)
{
    // Not implemented.
}

void Slider::setMaxValue(double v)
{
    // Not implemented.
}

double Slider::value() const
{
    // Not implemented.
    
    return 0;
}

double Slider::minValue() const
{
    // Not implemented.
    return 0;
}

double Slider::maxValue() const
{
    // Not implemented.
    return 0;
}

void Slider::sliderValueChanged()
{
    // Not implemented.
}

const int* Slider::dimensions() const
{
    // Not implemented.
    return (const int *)0;
}

