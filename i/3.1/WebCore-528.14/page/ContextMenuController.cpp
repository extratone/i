/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "ContextMenuController.h"

#include "Chrome.h"
#include "ContextMenu.h"
#include "ContextMenuClient.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "HTMLFormElement.h"
#include "InspectorController.h"
#include "MouseEvent.h"
#include "Node.h"
#include "Page.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextIterator.h"
#include "WindowFeatures.h"
#include "markup.h"

namespace WebCore {

ContextMenuController::ContextMenuController(Page* page, ContextMenuClient* client)
    : m_page(page)
    , m_client(client)
    , m_contextMenu(0)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

ContextMenuController::~ContextMenuController()
{
    m_client->contextMenuDestroyed();
}

void ContextMenuController::clearContextMenu()
{
    m_contextMenu.set(0);
}

void ContextMenuController::handleContextMenuEvent(Event* event)
{
    ASSERT(event->type() == eventNames().contextmenuEvent);
    if (!event->isMouseEvent())
        return;
    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    IntPoint point = IntPoint(mouseEvent->pageX(), mouseEvent->pageY());
    HitTestResult result(point);

    if (Frame* frame = event->target()->toNode()->document()->frame()) {
        float zoomFactor = frame->pageZoomFactor();
        point.setX(static_cast<int>(point.x() * zoomFactor));
        point.setY(static_cast<int>(point.y() * zoomFactor));
        result = frame->eventHandler()->hitTestResultAtPoint(point, false);
    }

    if (!result.innerNonSharedNode())
        return;

    m_contextMenu.set(new ContextMenu(result));
    m_contextMenu->populate();
    if (m_page->inspectorController()->enabled())
        m_contextMenu->addInspectElementItem();

    PlatformMenuDescription customMenu = m_client->getCustomMenuFromDefaultItems(m_contextMenu.get());
    m_contextMenu->setPlatformDescription(customMenu);

    event->setDefaultHandled();
}

static void openNewWindow(const KURL& urlToLoad, Frame* frame)
{
    if (Page* oldPage = frame->page()) {
        WindowFeatures features;
        // userGesture == true because this is called from a context menu handler
        if (Page* newPage = oldPage->chrome()->createWindow(frame,
                FrameLoadRequest(ResourceRequest(urlToLoad, frame->loader()->outgoingReferrer())), features, true))
            newPage->chrome()->show();
    }
}

void ContextMenuController::contextMenuItemSelected(ContextMenuItem* item)
{
    ASSERT(item->type() == ActionType || item->type() == CheckableActionType);

    if (item->action() >= ContextMenuItemBaseApplicationTag) {
        m_client->contextMenuItemSelected(item, m_contextMenu.get());
        return;
    }

    HitTestResult result = m_contextMenu->hitTestResult();
    Frame* frame = result.innerNonSharedNode()->document()->frame();
    if (!frame)
        return;
    
    switch (item->action()) {
        case ContextMenuItemTagOpenLinkInNewWindow: 
            openNewWindow(result.absoluteLinkURL(), frame);
            break;
        case ContextMenuItemTagDownloadLinkToDisk:
            // FIXME: Some day we should be able to do this from within WebCore.
            m_client->downloadURL(result.absoluteLinkURL());
            break;
        case ContextMenuItemTagCopyLinkToClipboard:
            break;
        case ContextMenuItemTagOpenImageInNewWindow:
            openNewWindow(result.absoluteImageURL(), frame);
            break;
        case ContextMenuItemTagDownloadImageToDisk:
            // FIXME: Some day we should be able to do this from within WebCore.
            m_client->downloadURL(result.absoluteImageURL());
            break;
        case ContextMenuItemTagCopyImageToClipboard:
            // FIXME: The Pasteboard class is not written yet
            // For now, call into the client. This is temporary!
            break;
        case ContextMenuItemTagOpenFrameInNewWindow: {
            DocumentLoader* loader = frame->loader()->documentLoader();
            if (!loader->unreachableURL().isEmpty())
                openNewWindow(loader->unreachableURL(), frame);
            else
                openNewWindow(loader->url(), frame);
            break;
        }
        case ContextMenuItemTagCopy:
            frame->editor()->copy();
            break;
        case ContextMenuItemTagGoBack:
            frame->loader()->goBackOrForward(-1);
            break;
        case ContextMenuItemTagGoForward:
            frame->loader()->goBackOrForward(1);
            break;
        case ContextMenuItemTagStop:
            frame->loader()->stop();
            break;
        case ContextMenuItemTagReload:
            frame->loader()->reload();
            break;
        case ContextMenuItemTagCut:
            frame->editor()->cut();
            break;
        case ContextMenuItemTagPaste:
            frame->editor()->paste();
            break;
#if PLATFORM(GTK)
        case ContextMenuItemTagDelete:
            frame->editor()->performDelete();
            break;
        case ContextMenuItemTagSelectAll:
            frame->editor()->command("SelectAll").execute();
            break;
#endif
        case ContextMenuItemTagSpellingGuess:
            ASSERT(frame->selectedText().length());
            if (frame->editor()->shouldInsertText(item->title(), frame->selection()->toRange().get(),
                EditorInsertActionPasted)) {
                Document* document = frame->document();
                RefPtr<ReplaceSelectionCommand> command =
                    ReplaceSelectionCommand::create(document, createFragmentFromMarkup(document, item->title(), ""),
                                                                                   true, false, true);
                applyCommand(command);
                frame->revealSelection(RenderLayer::gAlignToEdgeIfNeeded);
            }
            break;
        case ContextMenuItemTagIgnoreSpelling:
            frame->editor()->ignoreSpelling();
            break;
        case ContextMenuItemTagLearnSpelling:
            frame->editor()->learnSpelling();
            break;
        case ContextMenuItemTagSearchWeb:
            m_client->searchWithGoogle(frame);
            break;
        case ContextMenuItemTagLookUpInDictionary:
            // FIXME: Some day we may be able to do this from within WebCore.
            m_client->lookUpInDictionary(frame);
            break;
        case ContextMenuItemTagOpenLink:
            if (Frame* targetFrame = result.targetFrame()) {
                targetFrame->loader()->loadFrameRequest(FrameLoadRequest(ResourceRequest(result.absoluteLinkURL(), 
                    frame->loader()->outgoingReferrer())), false, false, true, 0, 0);
            } else
                openNewWindow(result.absoluteLinkURL(), frame);
            break;
        case ContextMenuItemTagBold:
            frame->editor()->command("ToggleBold").execute();
            break;
        case ContextMenuItemTagItalic:
            frame->editor()->command("ToggleItalic").execute();
            break;
        case ContextMenuItemTagUnderline:
            frame->editor()->toggleUnderline();
            break;
        case ContextMenuItemTagOutline:
            // We actually never enable this because CSS does not have a way to specify an outline font,
            // which may make this difficult to implement. Maybe a special case of text-shadow?
            break;
        case ContextMenuItemTagStartSpeaking: {
            ExceptionCode ec;
            RefPtr<Range> selectedRange = frame->selection()->toRange();
            if (!selectedRange || selectedRange->collapsed(ec)) {
                Document* document = result.innerNonSharedNode()->document();
                selectedRange = document->createRange();
                selectedRange->selectNode(document->documentElement(), ec);
            }
            m_client->speak(plainText(selectedRange.get()));
            break;
        }
        case ContextMenuItemTagStopSpeaking:
            m_client->stopSpeaking();
            break;
        case ContextMenuItemTagDefaultDirection:
            frame->editor()->setBaseWritingDirection(NaturalWritingDirection);
            break;
        case ContextMenuItemTagLeftToRight:
            frame->editor()->setBaseWritingDirection(LeftToRightWritingDirection);
            break;
        case ContextMenuItemTagRightToLeft:
            frame->editor()->setBaseWritingDirection(RightToLeftWritingDirection);
            break;
        case ContextMenuItemTagTextDirectionDefault:
            frame->editor()->command("MakeTextWritingDirectionNatural").execute();
            break;
        case ContextMenuItemTagTextDirectionLeftToRight:
            frame->editor()->command("MakeTextWritingDirectionLeftToRight").execute();
            break;
        case ContextMenuItemTagTextDirectionRightToLeft:
            frame->editor()->command("MakeTextWritingDirectionRightToLeft").execute();
            break;
#if PLATFORM(MAC)
        case ContextMenuItemTagSearchInSpotlight:
            m_client->searchWithSpotlight();
            break;
#endif
        case ContextMenuItemTagShowSpellingPanel:
            frame->editor()->showSpellingGuessPanel();
            break;
        case ContextMenuItemTagCheckSpelling:
            break;
        case ContextMenuItemTagCheckSpellingWhileTyping:
            frame->editor()->toggleContinuousSpellChecking();
            break;
#ifndef BUILDING_ON_TIGER
        case ContextMenuItemTagCheckGrammarWithSpelling:
            frame->editor()->toggleGrammarChecking();
            break;
#endif
#if PLATFORM(MAC)
        case ContextMenuItemTagShowFonts:
            frame->editor()->showFontPanel();
            break;
        case ContextMenuItemTagStyles:
            frame->editor()->showStylesPanel();
            break;
        case ContextMenuItemTagShowColors:
            frame->editor()->showColorPanel();
            break;
#endif
        case ContextMenuItemTagInspectElement:
            if (Page* page = frame->page())
                page->inspectorController()->inspect(result.innerNonSharedNode());
            break;
        default:
            break;
    }
}

} // namespace WebCore
