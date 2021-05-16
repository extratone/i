/*
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "Pattern.h"

#include "TransformationMatrix.h"
#include "GraphicsContext.h"

#include <cairo.h>

namespace WebCore {

cairo_pattern_t* Pattern::createPlatformPattern(const TransformationMatrix& patternTransform) const
{
    cairo_surface_t* surface = tileImage()->nativeImageForCurrentFrame();
    if (!surface)
        return 0;

    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(surface);
    const TransformationMatrix& inverse = patternTransform.inverse();
    const cairo_matrix_t* pattern_matrix = reinterpret_cast<const cairo_matrix_t*>(&inverse);
    cairo_pattern_set_matrix(pattern, pattern_matrix);
    if (m_repeatX || m_repeatY)
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
    return pattern;
}

}
