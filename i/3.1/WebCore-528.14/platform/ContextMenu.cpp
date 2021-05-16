/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
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
#include "ContextMenu.h"

#include "ContextMenuController.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "Page.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TextIterator.h"
#include <memory>

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

ContextMenuController* ContextMenu::controller() const
{
    if (Node* node = m_hitTestResult.innerNonSharedNode())
        if (Frame* frame = node->document()->frame())
            if (Page* page = frame->page())
                return page->contextMenuController();
    return 0;
}

static auto_ptr<ContextMenuItem> separatorItem()
{
    return auto_ptr<ContextMenuItem>(new ContextMenuItem(SeparatorType, ContextMenuItemTagNoAction, String()));
}

static void createAndAppendFontSubMenu(const HitTestResult& result, ContextMenuItem& fontMenuItem)
{
    ContextMenu fontMenu(result);

#if PLATFORM(MAC)
    ContextMenuItem showFonts(ActionType, ContextMenuItemTagShowFonts, contextMenuItemTagShowFonts());
#endif
    ContextMenuItem bold(CheckableActionType, ContextMenuItemTagBold, contextMenuItemTagBold());
    ContextMenuItem italic(CheckableActionType, ContextMenuItemTagItalic, contextMenuItemTagItalic());
    ContextMenuItem underline(CheckableActionType, ContextMenuItemTagUnderline, contextMenuItemTagUnderline());
    ContextMenuItem outline(ActionType, ContextMenuItemTagOutline, contextMenuItemTagOutline());
#if PLATFORM(MAC)
    ContextMenuItem styles(ActionType, ContextMenuItemTagStyles, contextMenuItemTagStyles());
    ContextMenuItem showColors(ActionType, ContextMenuItemTagShowColors, contextMenuItemTagShowColors());
#endif

#if PLATFORM(MAC)
    fontMenu.appendItem(showFonts);
#endif
    fontMenu.appendItem(bold);
    fontMenu.appendItem(italic);
    fontMenu.appendItem(underline);
    fontMenu.appendItem(outline);
#if PLATFORM(MAC)
    fontMenu.appendItem(styles);
    fontMenu.appendItem(*separatorItem());
    fontMenu.appendItem(showColors);
#endif

    fontMenuItem.setSubMenu(&fontMenu);
}

#ifndef BUILDING_ON_TIGER
static void createAndAppendSpellingAndGrammarSubMenu(const HitTestResult& result, ContextMenuItem& spellingAndGrammarMenuItem)
{
    ContextMenu spellingAndGrammarMenu(result);

    ContextMenuItem showSpellingPanel(ActionType, ContextMenuItemTagShowSpellingPanel, 
        contextMenuItemTagShowSpellingPanel(true));
    ContextMenuItem checkSpelling(ActionType, ContextMenuItemTagCheckSpelling, 
        contextMenuItemTagCheckSpelling());
    ContextMenuItem checkAsYouType(CheckableActionType, ContextMenuItemTagCheckSpellingWhileTyping, 
        contextMenuItemTagCheckSpellingWhileTyping());
    ContextMenuItem grammarWithSpelling(CheckableActionType, ContextMenuItemTagCheckGrammarWithSpelling, 
        contextMenuItemTagCheckGrammarWithSpelling());

    spellingAndGrammarMenu.appendItem(showSpellingPanel);
    spellingAndGrammarMenu.appendItem(checkSpelling);
    spellingAndGrammarMenu.appendItem(checkAsYouType);
    spellingAndGrammarMenu.appendItem(grammarWithSpelling);

    spellingAndGrammarMenuItem.setSubMenu(&spellingAndGrammarMenu);
}
#else

static void createAndAppendSpellingSubMenu(const HitTestResult& result, ContextMenuItem& spellingMenuItem)
{
    ContextMenu spellingMenu(result);

    ContextMenuItem showSpellingPanel(ActionType, ContextMenuItemTagShowSpellingPanel, 
        contextMenuItemTagShowSpellingPanel(true));
    ContextMenuItem checkSpelling(ActionType, ContextMenuItemTagCheckSpelling, 
        contextMenuItemTagCheckSpelling());
    ContextMenuItem checkAsYouType(CheckableActionType, ContextMenuItemTagCheckSpellingWhileTyping, 
        contextMenuItemTagCheckSpellingWhileTyping());

    spellingMenu.appendItem(showSpellingPanel);
    spellingMenu.appendItem(checkSpelling);
    spellingMenu.appendItem(checkAsYouType);

    spellingMenuItem.setSubMenu(&spellingMenu);
}
#endif

#if PLATFORM(MAC)
static void createAndAppendSpeechSubMenu(const HitTestResult& result, ContextMenuItem& speechMenuItem)
{
    ContextMenu speechMenu(result);

    ContextMenuItem start(ActionType, ContextMenuItemTagStartSpeaking, contextMenuItemTagStartSpeaking());
    ContextMenuItem stop(ActionType, ContextMenuItemTagStopSpeaking, contextMenuItemTagStopSpeaking());

    speechMenu.appendItem(start);
    speechMenu.appendItem(stop);

    speechMenuItem.setSubMenu(&speechMenu);
}
#endif
 
#if !PLATFORM(GTK)
static void createAndAppendWritingDirectionSubMenu(const HitTestResult& result, ContextMenuItem& writingDirectionMenuItem)
{
    ContextMenu writingDirectionMenu(result);

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagDefaultDirection, 
        contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagRightToLeft, contextMenuItemTagRightToLeft());

    writingDirectionMenu.appendItem(defaultItem);
    writingDirectionMenu.appendItem(ltr);
    writingDirectionMenu.appendItem(rtl);

    writingDirectionMenuItem.setSubMenu(&writingDirectionMenu);
}

static void createAndAppendTextDirectionSubMenu(const HitTestResult& result, ContextMenuItem& textDirectionMenuItem)
{
    ContextMenu textDirectionMenu(result);

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagTextDirectionDefault, contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagTextDirectionLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagTextDirectionRightToLeft, contextMenuItemTagRightToLeft());

    textDirectionMenu.appendItem(defaultItem);
    textDirectionMenu.appendItem(ltr);
    textDirectionMenu.appendItem(rtl);

    textDirectionMenuItem.setSubMenu(&textDirectionMenu);
}
#endif

static bool selectionContainsPossibleWord(Frame* frame)
{
    // Current algorithm: look for a character that's not just a separator.
    for (TextIterator it(frame->selection()->toRange().get()); !it.atEnd(); it.advance()) {
        int length = it.length();
        const UChar* characters = it.characters();
        for (int i = 0; i < length; ++i)
            if (!(category(characters[i]) & (Separator_Space | Separator_Line | Separator_Paragraph)))
                return true;
    }
    return false;
}

void ContextMenu::populate()
{
    ContextMenuItem OpenLinkItem(ActionType, ContextMenuItemTagOpenLink, contextMenuItemTagOpenLink());
    ContextMenuItem OpenLinkInNewWindowItem(ActionType, ContextMenuItemTagOpenLinkInNewWindow, 
        contextMenuItemTagOpenLinkInNewWindow());
    ContextMenuItem DownloadFileItem(ActionType, ContextMenuItemTagDownloadLinkToDisk, 
        contextMenuItemTagDownloadLinkToDisk());
    ContextMenuItem CopyLinkItem(ActionType, ContextMenuItemTagCopyLinkToClipboard, 
        contextMenuItemTagCopyLinkToClipboard());
    ContextMenuItem OpenImageInNewWindowItem(ActionType, ContextMenuItemTagOpenImageInNewWindow, 
        contextMenuItemTagOpenImageInNewWindow());
    ContextMenuItem DownloadImageItem(ActionType, ContextMenuItemTagDownloadImageToDisk, 
        contextMenuItemTagDownloadImageToDisk());
    ContextMenuItem CopyImageItem(ActionType, ContextMenuItemTagCopyImageToClipboard, 
        contextMenuItemTagCopyImageToClipboard());
#if PLATFORM(MAC)
    ContextMenuItem SearchSpotlightItem(ActionType, ContextMenuItemTagSearchInSpotlight, 
        contextMenuItemTagSearchInSpotlight());
    ContextMenuItem LookInDictionaryItem(ActionType, ContextMenuItemTagLookUpInDictionary, 
        contextMenuItemTagLookUpInDictionary());
#endif
    ContextMenuItem SearchWebItem(ActionType, ContextMenuItemTagSearchWeb, contextMenuItemTagSearchWeb());
    ContextMenuItem CopyItem(ActionType, ContextMenuItemTagCopy, contextMenuItemTagCopy());
    ContextMenuItem BackItem(ActionType, ContextMenuItemTagGoBack, contextMenuItemTagGoBack());
    ContextMenuItem ForwardItem(ActionType, ContextMenuItemTagGoForward,  contextMenuItemTagGoForward());
    ContextMenuItem StopItem(ActionType, ContextMenuItemTagStop, contextMenuItemTagStop());
    ContextMenuItem ReloadItem(ActionType, ContextMenuItemTagReload, contextMenuItemTagReload());
    ContextMenuItem OpenFrameItem(ActionType, ContextMenuItemTagOpenFrameInNewWindow, 
        contextMenuItemTagOpenFrameInNewWindow());
    ContextMenuItem NoGuessesItem(ActionType, ContextMenuItemTagNoGuessesFound, 
        contextMenuItemTagNoGuessesFound());
    ContextMenuItem IgnoreSpellingItem(ActionType, ContextMenuItemTagIgnoreSpelling, 
        contextMenuItemTagIgnoreSpelling());
    ContextMenuItem LearnSpellingItem(ActionType, ContextMenuItemTagLearnSpelling, 
        contextMenuItemTagLearnSpelling());
    ContextMenuItem IgnoreGrammarItem(ActionType, ContextMenuItemTagIgnoreGrammar, 
        contextMenuItemTagIgnoreGrammar());
    ContextMenuItem CutItem(ActionType, ContextMenuItemTagCut, contextMenuItemTagCut());
    ContextMenuItem PasteItem(ActionType, ContextMenuItemTagPaste, contextMenuItemTagPaste());
#if PLATFORM(GTK)
    ContextMenuItem DeleteItem(ActionType, ContextMenuItemTagDelete, contextMenuItemTagDelete());
    ContextMenuItem SelectAllItem(ActionType, ContextMenuItemTagSelectAll, contextMenuItemTagSelectAll());
#endif
    
    HitTestResult result = hitTestResult();
    
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;
#if PLATFORM(GTK)
    if (!result.isContentEditable() && node->isControl())
        return;
#endif
    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    if (!result.isContentEditable()) {
        FrameLoader* loader = frame->loader();
        KURL linkURL = result.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem);
                appendItem(OpenLinkInNewWindowItem);
                appendItem(DownloadFileItem);
            }
            appendItem(CopyLinkItem);
        }

        KURL imageURL = result.absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(*separatorItem());

            appendItem(OpenImageInNewWindowItem);
            appendItem(DownloadImageItem);
            if (imageURL.isLocalFile() || m_hitTestResult.image())
                appendItem(CopyImageItem);
        }

        if (imageURL.isEmpty() && linkURL.isEmpty()) {
            if (result.isSelected()) {
                if (selectionContainsPossibleWord(frame)) {
#if PLATFORM(MAC)
                    appendItem(SearchSpotlightItem);
#endif
                    appendItem(SearchWebItem);
                    appendItem(*separatorItem());
#if PLATFORM(MAC)
                    appendItem(LookInDictionaryItem);
                    appendItem(*separatorItem());
#endif
                }
                appendItem(CopyItem);
            } else {
#if PLATFORM(GTK)
                appendItem(BackItem);
                appendItem(ForwardItem);
                appendItem(StopItem);
                appendItem(ReloadItem);
#else
                if (loader->canGoBackOrForward(-1))
                    appendItem(BackItem);

                if (loader->canGoBackOrForward(1))
                    appendItem(ForwardItem);

                // use isLoadingInAPISense rather than isLoading because Stop/Reload are
                // intended to match WebKit's API, not WebCore's internal notion of loading status
                if (loader->documentLoader()->isLoadingInAPISense())
                    appendItem(StopItem);
                else
                    appendItem(ReloadItem);
#endif

                if (frame->page() && frame != frame->page()->mainFrame())
                    appendItem(OpenFrameItem);
            }
        }
    } else { // Make an editing context menu
        SelectionController* selection = frame->selection();
        bool inPasswordField = selection->isInPasswordField();
        
        if (!inPasswordField) {
            // Consider adding spelling-related or grammar-related context menu items (never both, since a single selected range
            // is never considered a misspelling and bad grammar at the same time)
            bool misspelling = frame->editor()->isSelectionMisspelled();
            bool badGrammar = !misspelling && (frame->editor()->isGrammarCheckingEnabled() && frame->editor()->isSelectionUngrammatical());
            
            if (misspelling || badGrammar) {
                Vector<String> guesses = misspelling ? frame->editor()->guessesForMisspelledSelection()
                    : frame->editor()->guessesForUngrammaticalSelection();
                size_t size = guesses.size();
                if (size == 0) {
                    // If there's bad grammar but no suggestions (e.g., repeated word), just leave off the suggestions
                    // list and trailing separator rather than adding a "No Guesses Found" item (matches AppKit)
                    if (misspelling) {
                        appendItem(NoGuessesItem);
                        appendItem(*separatorItem());
                    }
                } else {
                    for (unsigned i = 0; i < size; i++) {
                        const String &guess = guesses[i];
                        if (!guess.isEmpty()) {
                            ContextMenuItem item(ActionType, ContextMenuItemTagSpellingGuess, guess);
                            appendItem(item);
                        }
                    }
                    appendItem(*separatorItem());                    
                }
                
                if (misspelling) {
                    appendItem(IgnoreSpellingItem);
                    appendItem(LearnSpellingItem);
                } else
                    appendItem(IgnoreGrammarItem);
                appendItem(*separatorItem());
            }
        }

        FrameLoader* loader = frame->loader();
        KURL linkURL = result.absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem);
                appendItem(OpenLinkInNewWindowItem);
                appendItem(DownloadFileItem);
            }
            appendItem(CopyLinkItem);
            appendItem(*separatorItem());
        }

        if (result.isSelected() && !inPasswordField && selectionContainsPossibleWord(frame)) {
#if PLATFORM(MAC)
            appendItem(SearchSpotlightItem);
#endif
            appendItem(SearchWebItem);
            appendItem(*separatorItem());
     
#if PLATFORM(MAC)
            appendItem(LookInDictionaryItem);
            appendItem(*separatorItem());
#endif
        }

        appendItem(CutItem);
        appendItem(CopyItem);
        appendItem(PasteItem);
#if PLATFORM(GTK)
        appendItem(DeleteItem);
        appendItem(*separatorItem());
        appendItem(SelectAllItem);
#endif

        if (!inPasswordField) {
            appendItem(*separatorItem());
#ifndef BUILDING_ON_TIGER
            ContextMenuItem SpellingAndGrammarMenuItem(SubmenuType, ContextMenuItemTagSpellingMenu, 
                contextMenuItemTagSpellingMenu());
            createAndAppendSpellingAndGrammarSubMenu(m_hitTestResult, SpellingAndGrammarMenuItem);
            appendItem(SpellingAndGrammarMenuItem);
#else
            ContextMenuItem SpellingMenuItem(SubmenuType, ContextMenuItemTagSpellingMenu, 
                contextMenuItemTagSpellingMenu());
            createAndAppendSpellingSubMenu(m_hitTestResult, SpellingMenuItem);
            appendItem(SpellingMenuItem);
#endif
            ContextMenuItem  FontMenuItem(SubmenuType, ContextMenuItemTagFontMenu, 
                contextMenuItemTagFontMenu());
            createAndAppendFontSubMenu(m_hitTestResult, FontMenuItem);
            appendItem(FontMenuItem);
#if PLATFORM(MAC)
            ContextMenuItem SpeechMenuItem(SubmenuType, ContextMenuItemTagSpeechMenu, 
                contextMenuItemTagSpeechMenu());
            createAndAppendSpeechSubMenu(m_hitTestResult, SpeechMenuItem);
            appendItem(SpeechMenuItem);
#endif
#if !PLATFORM(GTK)
            ContextMenuItem WritingDirectionMenuItem(SubmenuType, ContextMenuItemTagWritingDirectionMenu, 
                contextMenuItemTagWritingDirectionMenu());
            createAndAppendWritingDirectionSubMenu(m_hitTestResult, WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem);
            if (Page* page = frame->page()) {
                if (Settings* settings = page->settings()) {
                    bool includeTextDirectionSubmenu = settings->textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAlwaysIncluded
                        || settings->textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAutomaticallyIncluded && frame->editor()->hasBidiSelection();
                    if (includeTextDirectionSubmenu) {
                        ContextMenuItem TextDirectionMenuItem(SubmenuType, ContextMenuItemTagTextDirectionMenu, 
                            contextMenuItemTagTextDirectionMenu());
                        createAndAppendTextDirectionSubMenu(m_hitTestResult, TextDirectionMenuItem);
                        appendItem(TextDirectionMenuItem);
                    }
                }
            }
#endif
        }
    }
}

void ContextMenu::addInspectElementItem()
{
    Node* node = m_hitTestResult.innerNonSharedNode();
    if (!node)
        return;

    Frame* frame = node->document()->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    if (!page->inspectorController())
        return;

    ContextMenuItem InspectElementItem(ActionType, ContextMenuItemTagInspectElement, contextMenuItemTagInspectElement());
    appendItem(*separatorItem());
    appendItem(InspectElementItem);
}

void ContextMenu::checkOrEnableIfNeeded(ContextMenuItem& item) const
{
    if (item.type() == SeparatorType)
        return;
    
    Frame* frame = m_hitTestResult.innerNonSharedNode()->document()->frame();
    if (!frame)
        return;

    bool shouldEnable = true;
    bool shouldCheck = false; 

    switch (item.action()) {
        case ContextMenuItemTagCheckSpelling:
            shouldEnable = frame->editor()->canEdit();
            break;
        case ContextMenuItemTagDefaultDirection:
            shouldCheck = false;
            shouldEnable = false;
            break;
        case ContextMenuItemTagLeftToRight:
        case ContextMenuItemTagRightToLeft: {
            ExceptionCode ec = 0;
            RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
            String direction = item.action() == ContextMenuItemTagLeftToRight ? "ltr" : "rtl";
            style->setProperty(CSSPropertyDirection, direction, false, ec);
            shouldCheck = frame->editor()->selectionHasStyle(style.get()) != FalseTriState;
            shouldEnable = true;
            break;
        }
        case ContextMenuItemTagTextDirectionDefault: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionNatural");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionLeftToRight: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionLeftToRight");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionRightToLeft: {
            Editor::Command command = frame->editor()->command("MakeTextWritingDirectionRightToLeft");
            shouldCheck = command.state() == TrueTriState;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagCopy:
            shouldEnable = false;
            break;
        case ContextMenuItemTagCut:
            shouldEnable = false;
            break;
        case ContextMenuItemTagIgnoreSpelling:
        case ContextMenuItemTagLearnSpelling:
            shouldEnable = frame->selection()->isRange();
            break;
        case ContextMenuItemTagPaste:
            shouldEnable = false;
            break;
#if PLATFORM(GTK)
        case ContextMenuItemTagDelete:
            shouldEnable = frame->editor()->canDelete();
            break;
        case ContextMenuItemTagSelectAll:
        case ContextMenuItemTagInputMethods:
        case ContextMenuItemTagUnicode:
            shouldEnable = true;
            break;
#endif
        case ContextMenuItemTagUnderline: {
            ExceptionCode ec = 0;
            RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
            style->setProperty(CSSPropertyWebkitTextDecorationsInEffect, "underline", false, ec);
            shouldCheck = frame->editor()->selectionHasStyle(style.get()) != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagLookUpInDictionary:
            shouldEnable = frame->selection()->isRange();
            break;
        case ContextMenuItemTagCheckGrammarWithSpelling:
#ifndef BUILDING_ON_TIGER
            if (frame->editor()->isGrammarCheckingEnabled())
                shouldCheck = true;
            shouldEnable = true;
#endif
            break;
        case ContextMenuItemTagItalic: {
            ExceptionCode ec = 0;
            RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
            style->setProperty(CSSPropertyFontStyle, "italic", false, ec);
            shouldCheck = frame->editor()->selectionHasStyle(style.get()) != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagBold: {
            ExceptionCode ec = 0;
            RefPtr<CSSStyleDeclaration> style = frame->document()->createCSSStyleDeclaration();
            style->setProperty(CSSPropertyFontWeight, "bold", false, ec);
            shouldCheck = frame->editor()->selectionHasStyle(style.get()) != FalseTriState;
            shouldEnable = frame->editor()->canEditRichly();
            break;
        }
        case ContextMenuItemTagOutline:
            shouldEnable = false;
            break;
        case ContextMenuItemTagShowSpellingPanel:
#ifndef BUILDING_ON_TIGER
            if (frame->editor()->spellingPanelIsShowing())
                item.setTitle(contextMenuItemTagShowSpellingPanel(false));
            else
                item.setTitle(contextMenuItemTagShowSpellingPanel(true));
#endif
            shouldEnable = frame->editor()->canEdit();
            break;
        case ContextMenuItemTagNoGuessesFound:
            shouldEnable = false;
            break;
        case ContextMenuItemTagCheckSpellingWhileTyping:
            shouldCheck = frame->editor()->isContinuousSpellCheckingEnabled();
            break;
#if PLATFORM(GTK)
        case ContextMenuItemTagGoBack:
            shouldEnable = frame->loader()->canGoBackOrForward(-1);
            break;
        case ContextMenuItemTagGoForward:
            shouldEnable = frame->loader()->canGoBackOrForward(1);
            break;
        case ContextMenuItemTagStop:
            shouldEnable = frame->loader()->documentLoader()->isLoadingInAPISense();
            break;
        case ContextMenuItemTagReload:
            shouldEnable = !frame->loader()->documentLoader()->isLoadingInAPISense();
            break;
        case ContextMenuItemTagFontMenu:
            shouldEnable = frame->editor()->canEditRichly();
            break;
#else
        case ContextMenuItemTagGoBack:
        case ContextMenuItemTagGoForward:
        case ContextMenuItemTagStop:
        case ContextMenuItemTagReload:
        case ContextMenuItemTagFontMenu:
#endif
        case ContextMenuItemTagNoAction:
        case ContextMenuItemTagOpenLinkInNewWindow:
        case ContextMenuItemTagDownloadLinkToDisk:
        case ContextMenuItemTagCopyLinkToClipboard:
        case ContextMenuItemTagOpenImageInNewWindow:
        case ContextMenuItemTagDownloadImageToDisk:
        case ContextMenuItemTagCopyImageToClipboard:
        case ContextMenuItemTagOpenFrameInNewWindow:
        case ContextMenuItemTagSpellingGuess:
        case ContextMenuItemTagOther:
        case ContextMenuItemTagSearchInSpotlight:
        case ContextMenuItemTagSearchWeb:
        case ContextMenuItemTagOpenWithDefaultApplication:
        case ContextMenuItemPDFActualSize:
        case ContextMenuItemPDFZoomIn:
        case ContextMenuItemPDFZoomOut:
        case ContextMenuItemPDFAutoSize:
        case ContextMenuItemPDFSinglePage:
        case ContextMenuItemPDFFacingPages:
        case ContextMenuItemPDFContinuous:
        case ContextMenuItemPDFNextPage:
        case ContextMenuItemPDFPreviousPage:
        case ContextMenuItemTagOpenLink:
        case ContextMenuItemTagIgnoreGrammar:
        case ContextMenuItemTagSpellingMenu:
        case ContextMenuItemTagShowFonts:
        case ContextMenuItemTagStyles:
        case ContextMenuItemTagShowColors:
        case ContextMenuItemTagSpeechMenu:
        case ContextMenuItemTagStartSpeaking:
        case ContextMenuItemTagStopSpeaking:
        case ContextMenuItemTagWritingDirectionMenu:
        case ContextMenuItemTagTextDirectionMenu:
        case ContextMenuItemTagPDFSinglePageScrolling:
        case ContextMenuItemTagPDFFacingPagesScrolling:
        case ContextMenuItemTagInspectElement:
        case ContextMenuItemBaseApplicationTag:
            break;
    }

    item.setChecked(shouldCheck);
    item.setEnabled(shouldEnable);
}

}
