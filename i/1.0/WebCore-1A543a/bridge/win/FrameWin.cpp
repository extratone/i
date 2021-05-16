/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "FrameWin.h"

#include "BrowserExtensionWin.h"
#include "Decoder.h"
#include "Document.h"
#include "FramePrivate.h"
#include "Settings.h"
#include "PlatformKeyboardEvent.h"
#include "Plugin.h"
#include "RenderFrame.h"
#include "TransferJob.h"
#include <windows.h>

namespace WebCore {

FrameWin::FrameWin(Page* page, Element* ownerElement, FrameWinClient* client)
    : Frame(page, ownerElement)
{
    d->m_extension = new BrowserExtensionWin(this);
    Settings* settings = new Settings();
    settings->setAutoLoadImages(true);
    settings->setMediumFixedFontSize(13);
    settings->setMediumFontSize(16);
    settings->setSerifFontName("Times New Roman");
    settings->setFixedFontName("Courier New");
    settings->setSansSerifFontName("Arial");
    settings->setStdFontName("Times New Roman");
    settings->setIsJavaScriptEnabled(true);

    setSettings(settings);
    m_client = client;
}

FrameWin::~FrameWin()
{
    setView(0);
    clearRecordedFormValues();    
}

void FrameWin::urlSelected(const ResourceRequest& request)
{
    if (m_client)
        m_client->openURL(request.url().url(), request.lockHistory());
}

void FrameWin::submitForm(const ResourceRequest& request)
{
    // FIXME: this is a hack inherited from FrameMac, and should be pushed into Frame
    if (d->m_submittedFormURL == request.url())
        return;
    d->m_submittedFormURL = request.url();

    if (m_client)
        m_client->submitForm(request.doPost() ? "POST" : "GET", request.url(), &request.postData);

    clearRecordedFormValues();
}

String FrameWin::userAgent() const
{
    return "Mozilla/5.0 (PC; U; Intel; Windows; en) AppleWebKit/420+ (KHTML, like Gecko)";
}

void FrameWin::runJavaScriptAlert(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    UChar nullChar = 0;
    text += String(&nullChar, 1);
    MessageBox(view()->windowHandle(), text.characters(), L"JavaScript Alert", MB_OK);
}

bool FrameWin::runJavaScriptConfirm(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    UChar nullChar = 0;
    text += String(&nullChar, 1);
    return MessageBox(view()->windowHandle(), text.characters(), L"JavaScript Alert", MB_OKCANCEL) == IDOK;
}

// FIXME: This needs to be unified with the keyPress method on FrameMac
bool FrameWin::keyPress(const PlatformKeyboardEvent& keyEvent)
{
    bool result;
    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document *doc = document();
    if (!doc)
        return false;
    Node *node = doc->focusNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }
    
    if (!keyEvent.isKeyUp())
        prepareForUserAction();

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.

    return result;
}

void FrameWin::setTitle(const String &title)
{
    String text = title;
    text.replace('\\', backslashAsCurrencySymbol());

    m_client->setTitle(text);
}

void FrameWin::setStatusBarText(const String& status)
{
    String text = status;
    text.replace('\\', backslashAsCurrencySymbol());

    m_client->setStatusText(text);
}


}
