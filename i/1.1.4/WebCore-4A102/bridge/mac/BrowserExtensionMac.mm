/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "BrowserExtensionMac.h"

#import "BlockExceptions.h"
#import "FloatRect.h"
#import "FrameMac.h"
#import "FrameTree.h"
#import "Page.h"
#import "Screen.h"
#import "WebCoreFrameBridge.h"
#import "WebCorePageBridge.h"

#import "WKScreen.h"
namespace WebCore {

BrowserExtensionMac::BrowserExtensionMac(Frame *frame)
    : m_frame(Mac(frame))
{
}

void BrowserExtensionMac::createNewWindow(const ResourceRequest& request) 
{
    createNewWindow(request, WindowArgs(), NULL, false);
}

void BrowserExtensionMac::createNewWindow(const ResourceRequest& request, 
                                          const WindowArgs& winArgs, 
                                          Frame*& part
                                                , bool userGesture)
{
    createNewWindow(request, winArgs, &part, userGesture);
}


void BrowserExtensionMac::createNewWindow(const ResourceRequest& request, 
                                          const WindowArgs& winArgs, 
                                          WebCore::Frame** partResult, bool userGesture)
{ 
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(!winArgs.dialog || request.frameName.isEmpty());

    if (partResult)
        *partResult = NULL;
    
    const KURL& url = request.url();

    NSString *frameName = request.frameName.isEmpty() ? nil : (NSString*)request.frameName;
    if (frameName) {
        if (Frame* frame = m_frame->tree()->find(frameName)) {
            WebCoreFrameBridge *frameBridge = Mac(frame)->bridge();
            if (![m_frame->bridge() canTargetLoadInFrame:frameBridge])
                return;
            if (!url.isEmpty()) {
                String argsReferrer = request.referrer();
                NSString *referrer;
                if (!argsReferrer.isEmpty())
                    referrer = argsReferrer;
                else
                    referrer = [frameBridge referrer];

                [frameBridge loadURL:url.getNSURL() 
                       referrer:referrer 
                         reload:request.reload 
                    userGesture:userGesture 
                         target:nil 
                triggeringEvent:nil 
                           form:nil 
                     formValues:nil];
            }

            [frameBridge activateWindow:userGesture];
            if (partResult)
                *partResult = [frameBridge impl];

            return;
        }
    }
    
    WebCorePageBridge *pageBridge;
    if (winArgs.dialog)
        pageBridge = [m_frame->bridge() createModalDialogWithURL:url.getNSURL() userGesture:userGesture];
    else
        pageBridge = [m_frame->bridge() createWindowWithURL:url.getNSURL() userGesture:userGesture];
    if (!pageBridge)
        return;
    
    WebCoreFrameBridge *frameBridge = [pageBridge mainFrame];
    if ([frameBridge impl])
        [frameBridge impl]->tree()->setName(AtomicString(request.frameName));
    
    if (partResult)
        *partResult = [frameBridge impl];
    
    [frameBridge setToolbarsVisible:winArgs.toolBarVisible || winArgs.locationBarVisible];
    [frameBridge setStatusbarVisible:winArgs.statusBarVisible];
    [frameBridge setScrollbarsVisible:winArgs.scrollBarsVisible];
    [frameBridge setWindowIsResizable:winArgs.resizable];
    
	CGRect windowRect = [pageBridge impl]->windowRect();
    if (winArgs.xSet)
      windowRect.origin.x = winArgs.x;
    if (winArgs.ySet)
      windowRect.origin.y = winArgs.y;
    
    // 'width' and 'height' specify the dimensions of the WebView, but we can only resize the window, 
    // so we compute a WebView delta and apply it to the window.
    ViewportArguments arguments;
    
    if (winArgs.widthSet)
        arguments.width = winArgs.width;
    if (winArgs.heightSet)
        arguments.height = winArgs.height;

    [frameBridge impl]->setViewportArguments(arguments);
    
    [frameBridge showWindow];
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void BrowserExtensionMac::setIconURL(const KURL &url)
{
}

void BrowserExtensionMac::setTypedIconURL(const KURL &url, const String &type)
{
}

int BrowserExtensionMac::getHistoryLength()
{
    return [m_frame->bridge() historyLength];
}

void BrowserExtensionMac::goBackOrForward(int distance)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() goBackOrForward:distance];
    END_BLOCK_OBJC_EXCEPTIONS;
}

KURL BrowserExtensionMac::historyURL(int distance)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return KURL([m_frame->bridge() historyURL:distance]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return KURL();
}

bool BrowserExtensionMac::canRunModal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_frame->bridge() canRunModal];
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

bool BrowserExtensionMac::canRunModalNow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_frame->bridge() canRunModalNow];
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

void BrowserExtensionMac::runModal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() runModal];
    END_BLOCK_OBJC_EXCEPTIONS;
}

}
