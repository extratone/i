/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
#include "PerspectiveTransformOperation.h"

#include <algorithm>

using namespace std;

namespace WebCore {

PassRefPtr<TransformOperation> PerspectiveTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToIdentity)
        return PerspectiveTransformOperation::create(m_p + (1. - m_p) * progress);
    
    const PerspectiveTransformOperation* fromOp = static_cast<const PerspectiveTransformOperation*>(from);
    double fromP = fromOp ? fromOp->m_p : 0;
    double toP = m_p;

    TransformationMatrix fromT;
    TransformationMatrix toT;
    fromT.applyPerspective(fromP);
    toT.applyPerspective(toP);
    toT.blend(fromT, progress);
    TransformationMatrix::DecomposedType decomp;
    toT.decompose(decomp);
    
    return PerspectiveTransformOperation::create(decomp.perspectiveZ ? -1.0 / decomp.perspectiveZ : 0.0);
}

} // namespace WebCore
