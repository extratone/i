// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef Chrome_h
#define Chrome_h

#include "FocusDirection.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#ifndef __OBJC__
class NSView;
#endif
#endif

namespace WebCore {
    
    class ChromeClient;
    class ContextMenu;
    class FloatRect;
    class Frame;
    class IntRect;
    class Page;
    class String;
    
    struct FrameLoadRequest;
    
    enum MessageSource {
        HTMLMessageSource,
        XMLMessageSource,
        JSMessageSource,
        CSSMessageSource,
        OtherMessageSource
    };
    
    enum MessageLevel {
        TipMessageLevel,
        LogMessageLevel,
        WarningMessageLevel,
        ErrorMessageLevel
    };
    
    class Chrome {
public:
        Chrome(Page*, ChromeClient*);
        ~Chrome();
        
        ChromeClient* client() { return m_client; }
        void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
private:
        Page* m_page;
        ChromeClient* m_client;
    };
}

#endif // Chrome_h
