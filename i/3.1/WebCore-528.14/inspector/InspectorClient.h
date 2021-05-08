/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef InspectorClient_h
#define InspectorClient_h

#include "InspectorController.h"

namespace WebCore {

class Node;
class Page;
class String;

class InspectorClient {
public:
    virtual ~InspectorClient() {  }

    virtual void inspectorDestroyed() = 0;

    virtual Page* createPage() = 0;

    virtual String localizedStringsURL() = 0;

    virtual void showWindow() = 0;
    virtual void closeWindow() = 0;

    virtual void attachWindow() = 0;
    virtual void detachWindow() = 0;

    virtual void setAttachedWindowHeight(unsigned height) = 0;

    virtual void highlight(Node*) = 0;
    virtual void hideHighlight() = 0;

    virtual void inspectedURLChanged(const String& newURL) = 0;

    virtual void populateSetting(const String& key, InspectorController::Setting&) = 0;
    virtual void storeSetting(const String& key, const InspectorController::Setting&) = 0;
    virtual void removeSetting(const String& key) = 0;
};

} // namespace WebCore

#endif // !defined(InspectorClient_h)
