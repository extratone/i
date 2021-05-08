/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef FrameMac_h
#define FrameMac_h

#import <GraphicsServices/GraphicsServices.h>
#import "Frame.h"
#import "IntRect.h"
#import "PlatformMouseEvent.h"
#import "StringHash.h"
#import "WebCoreKeyboardAccess.h"

class NPObject;

namespace KJS {
    class PausedTimeouts;
    class SavedProperties;
    class SavedBuiltins;
    class ScheduledAction;
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#ifndef NSView
#define NSView WAKView
#endif

#ifdef __OBJC__

// Avoid clashes with KJS::DOMElement in KHTML code.
@class DOMElement;
typedef DOMElement ObjCDOMElement;

@class WebCorePageState;
@class NSArray;
@class NSDictionary;
@class NSFont;
@class NSMutableDictionary;
@class NSResponder;
@class NSString;
@class WAKView;
@class WebCoreFrameBridge;
@class WebScriptObject;

#else

// Avoid clashes with KJS::DOMElement in KHTML code.
class ObjCDOMElement;

class WebCorePageState;
class NSArray;
class NSDictionary;
class NSFont;
class WAKView;
class NSMutableDictionary;
class NSResponder;
class NSString;
class NSView;
class WebCoreFrameBridge;
class WebScriptObject;
typedef int NSWritingDirection;

#endif

namespace WebCore {

class DocumentFragment;
class FramePrivate;
class HTMLTableCellElement;
class RenderObject;
class RenderStyle;
class VisiblePosition;

struct DashboardRegionValue;

enum SelectionDirection {
    SelectingNext,
    SelectingPrevious
};

typedef Node * (*NodeQualifier)(RenderObject::NodeInfo aNodeInfo, Node * terminationNode, IntRect * frame);

class FrameMac : public Frame
{
public:
    FrameMac(Page*, Element*);
    ~FrameMac();
    
    void clear();

    void setBridge(WebCoreFrameBridge* p);
    WebCoreFrameBridge* bridge() const { return _bridge; }
    virtual void setView(FrameView*);
    virtual void frameDetached();

    virtual bool openURL(const KURL&);
    
    virtual void openURLRequest(const ResourceRequest&);
    virtual void submitForm(const ResourceRequest&);

    int indexCountOfWordPrecedingSelection(NSString *word);
    NSArray *wordsInCurrentParagraph();

    
    virtual void setTitle(const String&);
    virtual void setStatusBarText(const String&);

    virtual void urlSelected(const ResourceRequest&);

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);
    virtual Plugin* createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType);
    virtual void redirectDataToPlugin(Widget* pluginWidget);

    virtual Frame* createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer);

    virtual void scheduleClose();

    virtual void focusWindow();
    virtual void unfocusWindow();
    
    void openURLFromPageCache(WebCorePageState*);

    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);
    
    NSView* nextKeyView(Node* startingPoint, SelectionDirection);
    NSView* nextKeyViewInFrameHierarchy(Node* startingPoint, SelectionDirection);
    static NSView* nextKeyViewForWidget(Widget* startingPoint, SelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView* view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(Widget* candidate);
    
    virtual void runJavaScriptAlert(const String&);
    virtual bool runJavaScriptConfirm(const String&);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool shouldInterruptJavaScript();    
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    bool shouldClose();

    virtual void createEmptyDocument();

    static WebCoreFrameBridge* bridgeForWidget(const Widget*);
    
    virtual String incomingReferrer() const;
    virtual String userAgent() const;

    virtual String mimeTypeForFileName(const String&) const;

    bool dispatchDragSrcEvent(const AtomicString &eventType, const PlatformMouseEvent&) const;

    GSFontRef fontForSelection(bool *hasMultipleFonts) const;
    NSDictionary* fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const SelectionController&);


    CGRect renderRectForPoint(CGPoint point, bool *isReplaced, float *fontSize);
    void mouseDown(GSEventRef);
    void mouseDragged(GSEventRef);
    void mouseUp(GSEventRef);
    void mouseMoved(GSEventRef);
    bool keyEvent(GSEventRef);
    bool wheelEvent(GSEventRef);

    inline void betterApproximateNode(int x, int y, NodeQualifier aQualifer, Node * & best, Node * failedNode, IntPoint &bestPoint, IntRect &bestRect, IntRect& testRect);
    inline Node * qualifyingNodeAtViewportLocation(CGPoint * aViewportLocation, NodeQualifier aQualifer, bool shouldApproximate);
    
    Node * nodeRespondingToClickEvents(CGPoint * aViewportLocation);
    Node * nodeRespondingToScrollWheelEvents(CGPoint * aViewportLocation);

    void sendFakeEventsAfterWidgetTracking(GSEventRef initiatingEvent);

    virtual bool lastEventIsMouseUp() const;
    void setActivationEventNumber(int num) { _activationEventNumber = num; }

    bool dragHysteresisExceeded(float dragLocationX, float dragLocationY) const;
    bool mouseDownMayStartDrag();
    
    bool sendContextMenuEvent(GSEventRef);

    virtual bool passMouseDownEventToWidget(Widget*);
    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframePart);
    virtual bool passWheelEventToChildWidget(Node*);
    void passEventToScrollView();
    
    NSString* searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element* element);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element* element);

    virtual void tokenizerProcessedData();

    virtual String overrideMediaType() const;
    
    CGColorRef bodyBackgroundColor() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void didTellBridgeAboutLoad(const String& URL);
    bool haveToldBridgeAboutLoad(const String& URL);

#if BINDINGS
    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    void addPluginRootObject(KJS::Bindings::RootObject* root);
    void cleanupPluginRootObjects();
#endif
    
    virtual void registerCommandForUndo(const EditCommandPtr&);
    virtual void registerCommandForRedo(const EditCommandPtr&);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const SelectionController &oldSelection, bool closeTyping);
    virtual void respondToChangedContents();
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const SelectionController &oldSelection, const SelectionController &newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldDeleteSelection(const SelectionController&) const;
    virtual bool shouldBeginEditing(const Range*) const;
    virtual bool shouldEndEditing(const Range*) const;
    virtual void didBeginEditing() const;
    virtual void didEndEditing() const;
    
    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);
    
    virtual void formElementDidFocus(Element*);
    virtual void formElementDidBlur(Element*);
    
    virtual void didReceiveViewportArguments(ViewportArguments);
    
    virtual ViewportArguments viewportArguments();
    virtual void setViewportArguments(ViewportArguments);
    virtual NSDictionary * dictionaryForViewportArguments(ViewportArguments arguments);
    
    virtual void deferredContentChangeObserved();

#if BINDINGS
    KJS::Bindings::RootObject* executionContextForDOM();
    KJS::Bindings::RootObject* bindingRootObject();
    
    WebScriptObject* windowScriptObject();
#endif
#if NETSCAPE_API
    NPObject* windowScriptNPObject();
#endif
    
    virtual void partClearedInBegin();
    

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
    virtual Range* markedTextRange() const { return m_markedTextRange.get(); }

    virtual bool canGoBackOrForward(int distance) const;

    void didParse(double duration);
    void didLayout(bool firstLayout, double duration);
    void didForcedLayout();
    void didReceiveDocType();
    char *windowState();
    
    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();
    
    virtual bool isCharacterSmartReplaceExempt(const DeprecatedChar &, bool);
    
    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();

    FloatRect customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect);
    void paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line);

    GSEventRef currentEvent() { return _currentEvent; }

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();

private:
    virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
    
    NSView* mouseDownViewIfStillGood();

    NSView* nextKeyViewInFrame(Node* startingPoint, SelectionDirection, bool* focusCallResultedInViewBeingCreated = 0);
    static NSView* documentViewForNode(Node*);
    

    void registerCommandForUndoOrRedo(const EditCommandPtr &cmd, bool isRedo);

    WebCoreFrameBridge* _bridge;
    
    NSView* _mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartDrag;
    bool _mouseDownMayStartSelect;
    PlatformMouseEvent m_mouseDown;
    // in our view's coords
    IntPoint m_mouseDownPos;
    float _mouseDownTimestamp;
    int _activationEventNumber;
    
    static GSEventRef _currentEvent;

    bool _haveUndoRedoOperations;
    
    HashSet<RefPtr<StringImpl> > urlsBridgeKnowsAbout;

    friend class Frame;

#if BINDINGS
    KJS::Bindings::RootObject* _bindingRoot;  // The root object used for objects
                                            // bound outside the context of a plugin.
    Vector<KJS::Bindings::RootObject*> m_rootObjects;
    WebScriptObject* _windowScriptObject;
#endif
#if NETSCAPE_API
    NPObject* _windowScriptNPObject;
#endif
    
    RefPtr<Node> _dragSrc;     // element that may be a drag source, for the current mouse gesture
    bool _dragSrcIsLink;
    bool _dragSrcIsImage;
    bool _dragSrcInSelection;
    bool _dragSrcMayBeDHTML, _dragSrcMayBeUA;   // Are DHTML and/or the UserAgent allowed to drag out?
    bool _dragSrcIsDHTML;
    
    RefPtr<Range> m_markedTextRange;
    virtual int maximumImageSize();

  virtual void notifySelectionLayoutChanged() const;    

};

inline FrameMac* Mac(Frame* frame) { return static_cast<FrameMac*>(frame); }
inline const FrameMac* Mac(const Frame* frame) { return static_cast<const FrameMac*>(frame); }

}

#endif
