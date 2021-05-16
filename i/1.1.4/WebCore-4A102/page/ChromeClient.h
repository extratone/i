// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006-2007 Apple, Inc.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include "Chrome.h"
#include "FocusDirection.h"

namespace WebCore {
    
    class FloatRect;
    class Frame;
    class IntRect;
    class Page;
    class String;
    
    struct FrameLoadRequest;
    
    class ChromeClient {
public:
        virtual ~ChromeClient() {  }
        virtual void chromeDestroyed() = 0;
        virtual void addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID) = 0;
    };
    
}

#endif // ChromeClient_h
