/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#import "config.h"

#import "DOMDocumentFragmentInternal.h"
#import "DOMExtensions.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLDocumentInternal.h"
#import "DOMHTMLInputElementInternal.h"
#import "DOMHTMLSelectElementInternal.h"
#import "DOMHTMLTextAreaElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "DocumentFragment.h"
#import "FrameView.h"
#import "HTMLCollection.h"
#import "HTMLDocument.h"
#import "HTMLInputElement.h"
#import "HTMLParserIdioms.h"
#import "HTMLSelectElement.h"
#import "HTMLTextAreaElement.h"
#import "Page.h"
#import "Range.h"
#import "RenderTextControl.h"
#import "Settings.h"
#import "markup.h"

#if PLATFORM(IOS)
#import "Autocapitalize.h"
#import "DOMHTMLElementInternal.h"
#import "HTMLTextFormControlElement.h"
#import "JSMainThreadExecState.h"
#import "RenderLayer.h"
#import "WAKWindow.h"
#import "WebCoreThreadMessage.h"
#endif

#if PLATFORM(IOS)

using namespace WebCore;

@implementation DOMHTMLElement (DOMHTMLElementExtensions)

- (int)scrollXOffset
{
    RenderObject *renderer = core(self)->renderer();
    if (!renderer)
        return 0;

    if (!renderer->isBlockFlow())
        renderer = renderer->containingBlock();

    if (!renderer->isBox() || !renderer->hasOverflowClip())
        return 0;

    RenderBox *renderBox = toRenderBox(renderer);
    return renderBox->layer()->scrollXOffset();
}

- (int)scrollYOffset
{
    RenderObject *renderer = core(self)->renderer();
    if (!renderer)
        return 0;

    if (!renderer->isBlockFlow())
        renderer = renderer->containingBlock();
    if (!renderer->isBox() || !renderer->hasOverflowClip())
        return 0;

    RenderBox *renderBox = toRenderBox(renderer);
    return renderBox->layer()->scrollYOffset();
}

- (void)setScrollXOffset:(int)x scrollYOffset:(int)y
{
    [self setScrollXOffset:x scrollYOffset:y adjustForPurpleCaret:NO];
}

- (void)setScrollXOffset:(int)x scrollYOffset:(int)y adjustForPurpleCaret:(BOOL)adjustForPurpleCaret
{
    RenderObject *renderer = core(self)->renderer();
    if (!renderer)
        return;

    if (!renderer->isBlockFlow())
        renderer = renderer->containingBlock();
    if (!renderer->hasOverflowClip() || !renderer->isBox())
        return;

    RenderBox *renderBox = toRenderBox(renderer);
    RenderLayer *layer = renderBox->layer();
    if (adjustForPurpleCaret)
        layer->setAdjustForPurpleCaretWhenScrolling(true);
    layer->scrollToOffset(IntSize(x, y));
    if (adjustForPurpleCaret)
        layer->setAdjustForPurpleCaretWhenScrolling(false);
}

- (void)absolutePosition:(int *)x :(int *)y :(int *)w :(int *)h {
    RenderBox *renderer = core(self)->renderBox();
    if (renderer) {
        if (w)
            *w = renderer->width();
        if (h)
            *h = renderer->width();
        if (x && y) {
            FloatPoint floatPoint(*x, *y);
            renderer->localToAbsolute(floatPoint);
            IntPoint point = roundedIntPoint(floatPoint);
            *x = point.x();
            *y = point.y();
        }
    }
}

@end

#endif // PLATFORM(IOS)

//------------------------------------------------------------------------------------------
// DOMHTMLDocument

@implementation DOMHTMLDocument (DOMHTMLDocumentExtensions)

- (DOMDocumentFragment *)createDocumentFragmentWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL
{
    return kit(createFragmentFromMarkup(core(self), markupString, [baseURL absoluteString]).get());
}

- (DOMDocumentFragment *)createDocumentFragmentWithText:(NSString *)text
{
    // FIXME: Since this is not a contextual fragment, it won't handle whitespace properly.
    return kit(createFragmentFromText(core(self)->createRange().get(), text).get());
}

@end

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    NSURL *baseURL = core(self)->completeURL(WebCore::stripLeadingAndTrailingHTMLSpaces(baseURLString));
    return [self createDocumentFragmentWithMarkupString:markupString baseURL:baseURL];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [self createDocumentFragmentWithText:text];
}

@end


@implementation DOMHTMLInputElement (FormAutoFillTransition)

- (BOOL)_isTextField
{
    return core(self)->isTextField();
}

#if !PLATFORM(IOS)
- (NSRect)_rectOnScreen
{
    // Returns bounding rect of text field, in screen coordinates.
    NSRect result = [self boundingBox];
    if (!core(self)->document()->view())
        return result;

    NSView* view = core(self)->document()->view()->documentView();
    result = [view convertRect:result toView:nil];
    result.origin = [[view window] convertBaseToScreen:result.origin];
    return result;
}
#endif

- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index
{
    WebCore::HTMLInputElement* inputElement = core(self);
    if (inputElement) {
        WTF::String newValue = inputElement->value();
        newValue.replace(targetRange.location, targetRange.length, replacementString);
        inputElement->setValue(newValue);
        inputElement->setSelectionRange(index, newValue.length());
    }
}

- (NSRange)_selectedRange
{
    WebCore::HTMLInputElement* inputElement = core(self);
    if (inputElement) {
        int start = inputElement->selectionStart();
        int end = inputElement->selectionEnd();
        return NSMakeRange(start, end - start); 
    }
    return NSMakeRange(NSNotFound, 0);
}

- (BOOL)_isAutofilled
{
    return core(self)->isAutofilled();
}

- (void)_setAutofilled:(BOOL)filled
{
    // This notifies the input element that the content has been autofilled
    // This allows WebKit to obey the -webkit-autofill pseudo style, which
    // changes the background color.
    core(self)->setAutofilled(filled);
}

@end

@implementation DOMHTMLSelectElement (FormAutoFillTransition)

- (void)_activateItemAtIndex:(int)index
{
    // Use the setSelectedIndexByUser function so a change event will be fired. <rdar://problem/6760590>
    if (WebCore::HTMLSelectElement* select = core(self))
        select->optionSelectedByUser(index, true);
}

- (void)_activateItemAtIndex:(int)index allowMultipleSelection:(BOOL)allowMultipleSelection
{
    // Use the setSelectedIndexByUser function so a change event will be fired. <rdar://problem/6760590>
    // If this is a <select multiple> the allowMultipleSelection flag will allow setting multiple
    // selections without clearing the other selections.
    if (WebCore::HTMLSelectElement* select = core(self))
        select->optionSelectedByUser(index, true, allowMultipleSelection);
}

@end

@implementation DOMHTMLInputElement (FormPromptAdditions)

- (BOOL)_isEdited
{
    return core(self)->lastChangeWasUserEdit();
}

@end

@implementation DOMHTMLTextAreaElement (FormPromptAdditions)

- (BOOL)_isEdited
{
    return core(self)->lastChangeWasUserEdit();
}

@end

#if PLATFORM(IOS)
@implementation DOMHTMLInputElement (AutocapitalizeAdditions)

- (WebAutocapitalizeType)_autocapitalizeType
{
    WebCore::HTMLInputElement* inputElement = core(self);
    return static_cast<WebAutocapitalizeType>(inputElement->autocapitalizeType());
}

@end

@implementation DOMHTMLTextAreaElement (AutocapitalizeAdditions)

- (WebAutocapitalizeType)_autocapitalizeType
{
    WebCore::HTMLTextAreaElement* textareaElement = core(self);
    return static_cast<WebAutocapitalizeType>(textareaElement->autocapitalizeType());
}

@end

@implementation DOMHTMLInputElement (WebInputChangeEventAdditions)

- (void)setValueWithChangeEvent:(NSString *)newValue
{
    WebCore::JSMainThreadNullState state;
    core(self)->setValue(newValue, DispatchInputAndChangeEvent);
}

- (void)setValueAsNumberWithChangeEvent:(double)newValueAsNumber
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    core(self)->setValueAsNumber(newValueAsNumber, ec, DispatchInputAndChangeEvent);
}

@end
#endif

Class kitClass(WebCore::HTMLCollection* collection)
{
    if (collection->type() == WebCore::SelectOptions)
        return [DOMHTMLOptionsCollection class];
    return [DOMHTMLCollection class];
}
