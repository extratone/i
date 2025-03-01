/*
 * Copyright (C) 2004, 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import <WebCore/DOMDocument.h>
#import <WebCore/DOMNode.h>
#import <WebCore/DOMObject.h>
#import <WebCore/DOMViews.h>

#import <WebCore/DOMEvent.h>
#import <WebCore/DOMEventException.h>
#import <WebCore/DOMEventListener.h>
#import <WebCore/DOMEventTarget.h>
#import <WebCore/DOMKeyboardEvent.h>
#import <WebCore/DOMMouseEvent.h>
#import <WebCore/DOMMutationEvent.h>
#import <WebCore/DOMOverflowEvent.h>
#import <WebCore/DOMProgressEvent.h>
#import <WebCore/DOMUIEvent.h>
#import <WebCore/DOMWheelEvent.h>

#if ENABLE(TOUCH_EVENTS)
#import <WebCore/DOMTouchEvent.h>
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
#import <WebCore/DOMGestureEvent.h>
#endif
