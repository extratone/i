/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Document_h
#define Document_h

#include "Attr.h"
#include "Color.h"
#include "DocumentMarker.h"
#include "HTMLCollection.h"
#include "HTMLFormElement.h"
#include "ScriptExecutionContext.h"
#include "StringHash.h"
#include "TextResourceDecoder.h"
#include "Timer.h"
#include <wtf/HashCountedSet.h>
#include <wtf/ListHashSet.h>

#include "RenderStyle.h"

// FIXME: We should move Mac off of the old Frame-based user stylesheet loading
// code and onto the new code in Page. We can't do that until the code in Page
// supports non-file: URLs, however.
#if PLATFORM(MAC) || PLATFORM(QT)
#define FRAME_LOADS_USER_STYLESHEET 1
#else
#define FRAME_LOADS_USER_STYLESHEET 0
#endif

namespace WebCore {

    class AXObjectCache;
    class Attr;
    class Attribute;
    class CDATASection;
    class CachedCSSStyleSheet;
    class CanvasRenderingContext2D;
    class CharacterData;
    class CSSStyleDeclaration;
    class CSSStyleSelector;
    class CSSStyleSheet;
    class Comment;
    class Database;
    class DOMImplementation;
    class DOMSelection;
    class DOMTimer;
    class DOMWindow;
    class DatabaseThread;
    class DocLoader;
    class DocumentFragment;
    class DocumentType;
    class EditingText;
    class Element;
    class EntityReference;
    class Event;
    class EventListener;
    class FormControlElementWithState;
    class Frame;
    class FrameView;
    class HTMLCanvasElement;
    class HTMLDocument;
    class HTMLElement;
    class HTMLFormElement;
    class HTMLHeadElement;
    class HTMLInputElement;
    class HTMLMapElement;
    class ImageLoader;
    class IntPoint;
    class JSNode;
    class MouseEventWithHitTestResults;
    class NodeFilter;
    class NodeIterator;
    class Page;
    class PlatformMouseEvent;
    class ProcessingInstruction;
    class Range;
    class RegisteredEventListener;
    class RenderArena;
    class RenderView;
    class SecurityOrigin;
    class Settings;
    class StyleSheet;
    class StyleSheetList;
    class Text;
    class TextResourceDecoder;
    class Tokenizer;
    class TreeWalker;
    class XMLHttpRequest;

#if ENABLE(SVG)
    class SVGDocumentExtensions;
#endif
    
#if ENABLE(XBL)
    class XBLBindingManager;
#endif

#if ENABLE(XPATH)
    class XPathEvaluator;
    class XPathExpression;
    class XPathNSResolver;
    class XPathResult;
#endif

#if ENABLE(DASHBOARD_SUPPORT)
    struct DashboardRegionValue;
#endif
    struct HitTestRequest;

#if ENABLE(TOUCH_EVENTS)
#include "DocumentIPhoneForward.h"
#endif
    
    typedef int ExceptionCode;

    class CachedImage;
    extern const int cLayoutScheduleThreshold;

class FormElementKey {
public:
    FormElementKey(AtomicStringImpl* = 0, AtomicStringImpl* = 0);
    ~FormElementKey();
    FormElementKey(const FormElementKey&);
    FormElementKey& operator=(const FormElementKey&);

    AtomicStringImpl* name() const { return m_name; }
    AtomicStringImpl* type() const { return m_type; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    FormElementKey(WTF::HashTableDeletedValueType) : m_name(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_name == hashTableDeletedValue(); }

private:
    void ref() const;
    void deref() const;

    static AtomicStringImpl* hashTableDeletedValue() { return reinterpret_cast<AtomicStringImpl*>(-1); }

    AtomicStringImpl* m_name;
    AtomicStringImpl* m_type;
};

inline bool operator==(const FormElementKey& a, const FormElementKey& b)
{
    return a.name() == b.name() && a.type() == b.type();
}

struct FormElementKeyHash {
    static unsigned hash(const FormElementKey&);
    static bool equal(const FormElementKey& a, const FormElementKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
    static void constructDeletedValue(FormElementKey& slot) { new (&slot) FormElementKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const FormElementKey& value) { return value.isHashTableDeletedValue(); }
};

class TextAutoSizingKey {
public:
    TextAutoSizingKey();
    TextAutoSizingKey(RenderStyle*, Document*);
    ~TextAutoSizingKey();
    TextAutoSizingKey(const TextAutoSizingKey&);
    TextAutoSizingKey& operator=(const TextAutoSizingKey&);
    Document* doc() const { return m_doc; }
    RenderStyle* style() const { return m_style; }
    inline bool isValidDoc() const { return m_doc && m_doc != deletedKeyDoc(); }
    inline bool isValidStyle() const { return m_style && m_style != deletedKeyStyle(); }
    static Document* deletedKeyDoc() { return (Document*) -1; }
    static RenderStyle* deletedKeyStyle() { return (RenderStyle*) -1; }
private:
    void ref() const;
    void deref() const;
    RenderStyle *m_style;
    Document *m_doc;
};

inline bool operator==(const TextAutoSizingKey& a, const TextAutoSizingKey& b)
{
    if (a.isValidStyle() && b.isValidStyle())
        return a.style()->equalForTextAutosizing(b.style());
    return a.style() == b.style();
}

struct TextAutoSizingHash {
    static unsigned hash(const TextAutoSizingKey&k) { return k.style()->hashForTextAutosizing(); }
    static bool equal(const TextAutoSizingKey& a, const TextAutoSizingKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct TextAutoSizingTraits : WTF::GenericHashTraits<TextAutoSizingKey> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(TextAutoSizingKey& slot);
    static bool isDeletedValue(const TextAutoSizingKey& value);
};

class TextAutoSizingValue : public RefCounted<TextAutoSizingValue>{
public:
    static PassRefPtr<TextAutoSizingValue> create()
    {
        return adoptRef(new TextAutoSizingValue());
    }

    void addNode(Node *node, float size);
    bool adjustNodeSizes ();
    int numNodes () const;
    void reset();

private:
    TextAutoSizingValue() { }

    HashSet<RefPtr<Node> > m_autoSizedNodes;
};

struct ViewportArguments
{
    ViewportArguments() :initialScale(-1), minimumScale(-1), maximumScale(-1), width(-1), height(-1), userScalable(-1) { }
    
    float initialScale;
    float minimumScale;
    float maximumScale;
    float width;
    float height;
    
    float userScalable;
    
    bool hasCustomArgument() const { return initialScale != -1 || minimumScale != -1 || maximumScale != -1 || width != -1 || height != -1 || userScalable != -1; }
};

class Document : public ContainerNode, public ScriptExecutionContext {
public:
    static PassRefPtr<Document> create(Frame* frame)
    {
        return new Document(frame, false);
    }
    static PassRefPtr<Document> createXHTML(Frame* frame)
    {
        return new Document(frame, true);
    }
    virtual ~Document();

    virtual bool isDocument() const { return true; }

    using ContainerNode::ref;
    using ContainerNode::deref;
    virtual void removedLastRef();

    // Nodes belonging to this document hold "self-only" references -
    // these are enough to keep the document from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its document to still have a valid document
    // pointer without introducing reference cycles

    void selfOnlyRef()
    {
        ASSERT(!m_deletionHasBegun);
        ++m_selfOnlyRefCount;
    }
    void selfOnlyDeref()
    {
        ASSERT(!m_deletionHasBegun);
        --m_selfOnlyRefCount;
        if (!m_selfOnlyRefCount && !refCount()) {
#ifndef NDEBUG
            m_deletionHasBegun = true;
#endif
            delete this;
        }
    }

    // DOM methods & attributes for Document

    DocumentType* doctype() const { return m_docType.get(); }

    DOMImplementation* implementation() const;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);
    
    Element* documentElement() const
    {
        if (!m_documentElement)
            cacheDocumentElement();
        return m_documentElement.get();
    }
    
    virtual PassRefPtr<Element> createElement(const AtomicString& tagName, ExceptionCode&);
    PassRefPtr<DocumentFragment> createDocumentFragment ();
    PassRefPtr<Text> createTextNode(const String& data);
    PassRefPtr<Comment> createComment(const String& data);
    PassRefPtr<CDATASection> createCDATASection(const String& data, ExceptionCode&);
    PassRefPtr<ProcessingInstruction> createProcessingInstruction(const String& target, const String& data, ExceptionCode&);
    PassRefPtr<Attr> createAttribute(const String& name, ExceptionCode& ec) { return createAttributeNS(String(), name, ec, true); }
    PassRefPtr<Attr> createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&, bool shouldIgnoreNamespaceChecks = false);
    PassRefPtr<EntityReference> createEntityReference(const String& name, ExceptionCode&);
    PassRefPtr<Node> importNode(Node* importedNode, bool deep, ExceptionCode&);
    virtual PassRefPtr<Element> createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    PassRefPtr<Element> createElement(const QualifiedName&, bool createdByParser, ExceptionCode& ec);
    Element* getElementById(const AtomicString&) const;
    bool hasElementWithId(AtomicStringImpl* id) const;
    bool containsMultipleElementsWithId(const AtomicString& elementId) { return m_duplicateIds.contains(elementId.impl()); }

    Element* elementFromPoint(int x, int y) const;
    String readyState() const;
    String inputEncoding() const;
    String defaultCharset() const;

    String charset() const { return inputEncoding(); }
    String characterSet() const { return inputEncoding(); }

    void setCharset(const String&);

    String contentLanguage() const { return m_contentLanguage; }
    void setContentLanguage(const String& lang) { m_contentLanguage = lang; }

    String xmlEncoding() const { return m_xmlEncoding; }
    String xmlVersion() const { return m_xmlVersion; }
    bool xmlStandalone() const { return m_xmlStandalone; }

    void setXMLEncoding(const String& encoding) { m_xmlEncoding = encoding; } // read-only property, only to be set from XMLTokenizer
    void setXMLVersion(const String&, ExceptionCode&);
    void setXMLStandalone(bool, ExceptionCode&);

    String documentURI() const { return m_documentURI; }
    void setDocumentURI(const String&);

    virtual KURL baseURI() const;

    PassRefPtr<Node> adoptNode(PassRefPtr<Node> source, ExceptionCode&);

    PassRefPtr<HTMLCollection> images();
    PassRefPtr<HTMLCollection> embeds();
    PassRefPtr<HTMLCollection> plugins(); // an alias for embeds() required for the JS DOM bindings.
    PassRefPtr<HTMLCollection> applets();
    PassRefPtr<HTMLCollection> links();
    PassRefPtr<HTMLCollection> forms();
    PassRefPtr<HTMLCollection> anchors();
    PassRefPtr<HTMLCollection> all();
    PassRefPtr<HTMLCollection> objects();
    PassRefPtr<HTMLCollection> scripts();
    PassRefPtr<HTMLCollection> windowNamedItems(const String& name);
    PassRefPtr<HTMLCollection> documentNamedItems(const String& name);

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    HTMLCollection::CollectionInfo* collectionInfo(HTMLCollection::Type type)
    {
        ASSERT(type >= HTMLCollection::FirstUnnamedDocumentCachedType);
        unsigned index = type - HTMLCollection::FirstUnnamedDocumentCachedType;
        ASSERT(index < HTMLCollection::NumUnnamedDocumentCachedTypes);
        return &m_collectionInfo[index]; 
    }

    HTMLCollection::CollectionInfo* nameCollectionInfo(HTMLCollection::Type, const AtomicString& name);

    // DOM methods overridden from  parent classes

    virtual String nodeName() const;
    virtual NodeType nodeType() const;

    // Other methods (not part of DOM)
    virtual bool isHTMLDocument() const { return false; }
    virtual bool isImageDocument() const { return false; }
#if ENABLE(SVG)
    virtual bool isSVGDocument() const { return false; }
#endif
    virtual bool isPluginDocument() const { return false; }
    virtual bool isMediaDocument() const { return false; }
#if ENABLE(WML)
    virtual bool isWMLDocument() const { return false; }
#endif
    
    CSSStyleSelector* styleSelector() const { return m_styleSelector; }

    Element* getElementByAccessKey(const String& key) const;
    
    /**
     * Updates the pending sheet count and then calls updateStyleSelector.
     */
    void removePendingSheet();

    /**
     * This method returns true if all top-level stylesheets have loaded (including
     * any @imports that they may be loading).
     */
    bool haveStylesheetsLoaded() const
    {
        return m_pendingStylesheets <= 0 || m_ignorePendingStylesheets
#if USE(LOW_BANDWIDTH_DISPLAY)
            || m_inLowBandwidthDisplay
#endif
            ;
    }

    /**
     * Increments the number of pending sheets.  The <link> elements
     * invoke this to add themselves to the loading list.
     */
    void addPendingSheet() { m_pendingStylesheets++; }

    void addStyleSheetCandidateNode(Node*, bool createdByParser);
    void removeStyleSheetCandidateNode(Node*);

    bool gotoAnchorNeededAfterStylesheetsLoad() { return m_gotoAnchorNeededAfterStylesheetsLoad; }
    void setGotoAnchorNeededAfterStylesheetsLoad(bool b) { m_gotoAnchorNeededAfterStylesheetsLoad = b; }

    /**
     * Called when one or more stylesheets in the document may have been added, removed or changed.
     *
     * Creates a new style selector and assign it to this document. This is done by iterating through all nodes in
     * document (or those before <BODY> in a HTML document), searching for stylesheets. Stylesheets can be contained in
     * <LINK>, <STYLE> or <BODY> elements, as well as processing instructions (XML documents only). A list is
     * constructed from these which is used to create the a new style selector which collates all of the stylesheets
     * found and is used to calculate the derived styles for all rendering objects.
     */
    void updateStyleSelector();

    void recalcStyleSelector();

    bool usesDescendantRules() const { return m_usesDescendantRules; }
    void setUsesDescendantRules(bool b) { m_usesDescendantRules = b; }
    bool usesSiblingRules() const { return m_usesSiblingRules; }
    void setUsesSiblingRules(bool b) { m_usesSiblingRules = b; }
    bool usesFirstLineRules() const { return m_usesFirstLineRules; }
    void setUsesFirstLineRules(bool b) { m_usesFirstLineRules = b; }
    bool usesFirstLetterRules() const { return m_usesFirstLetterRules; }
    void setUsesFirstLetterRules(bool b) { m_usesFirstLetterRules = b; }
    bool usesBeforeAfterRules() const { return m_usesBeforeAfterRules; }
    void setUsesBeforeAfterRules(bool b) { m_usesBeforeAfterRules = b; }

    // Machinery for saving and restoring state when you leave and then go back to a page.
    void registerFormElementWithState(FormControlElementWithState* e) { m_formElementsWithState.add(e); }
    void unregisterFormElementWithState(FormControlElementWithState* e) { m_formElementsWithState.remove(e); }
    Vector<String> formElementsState() const;
    unsigned formElementsCharacterCount() const;
    void addAutoCorrectMarker(const Range *range, const String& word, const String& correction);
    void setStateForNewFormElements(const Vector<String>&);
    bool hasStateForNewFormElements() const;
    bool takeStateForFormElement(AtomicStringImpl* name, AtomicStringImpl* type, String& state);

    FrameView* view() const; // can be NULL
    Frame* frame() const { return m_frame; } // can be NULL
    Page* page() const; // can be NULL
    Settings* settings() const; // can be NULL

    PassRefPtr<Range> createRange();

    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow,
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, 
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    // Special support for editing
    PassRefPtr<CSSStyleDeclaration> createCSSStyleDeclaration();
    PassRefPtr<EditingText> createEditingTextNode(const String&);

    virtual void recalcStyle( StyleChange = NoChange );
    virtual void updateRendering();
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    static void updateDocumentsRendering();
    DocLoader* docLoader() { return m_docLoader; }

    virtual void attach();
    virtual void detach();
    // Override ScriptExecutionContext methods to do additional work
    void suspendActiveDOMObjects();
    void resumeActiveDOMObjects();
    void stopActiveDOMObjects();

    RenderArena* renderArena() { return m_renderArena; }

    RenderView* renderView() const;

    void clearAXObjectCache();
    AXObjectCache* axObjectCache() const;
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void open(Document* ownerDocument = 0);
    void implicitOpen();
    void close();
    void implicitClose();
    void cancelParsing();

    void write(const String& text, Document* ownerDocument = 0);
    void writeln(const String& text, Document* ownerDocument = 0);
    void finishParsing();
    void clear();

    bool wellFormed() const { return m_wellFormed; }

    const KURL& url() const { return m_url; }
    void setURL(const KURL&);

    const KURL& baseURL() const { return m_baseURL; }
    // Setting the BaseElementURL will change the baseURL.
    void setBaseElementURL(const KURL&);

    const String& baseTarget() const { return m_baseTarget; }
    // Setting the BaseElementTarget will change the baseTarget.
    void setBaseElementTarget(const String& baseTarget) { m_baseTarget = baseTarget; }

    KURL completeURL(const String&) const;

    // from cachedObjectClient
    virtual void setCSSStyleSheet(const String& url, const String& charset, const CachedCSSStyleSheet*);

#if FRAME_LOADS_USER_STYLESHEET
    void setUserStyleSheet(const String& sheet);
#endif

    String userStyleSheet() const;

    CSSStyleSheet* elementSheet();
    CSSStyleSheet* mappedElementSheet();
    virtual Tokenizer* createTokenizer();
    Tokenizer* tokenizer() { return m_tokenizer; }
    
    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

    enum ParseMode { Compat, AlmostStrict, Strict };

private:
    virtual void determineParseMode() {}
    
public:
    void setParseMode(ParseMode m) { m_parseMode = m; }
    ParseMode parseMode() const { return m_parseMode; }

    bool inCompatMode() const { return m_parseMode == Compat; }
    bool inAlmostStrictMode() const { return m_parseMode == AlmostStrict; }
    bool inStrictMode() const { return m_parseMode == Strict; }
    
    void setParsing(bool);
    bool parsing() const { return m_bParsing; }
    int minimumLayoutDelay();
    bool shouldScheduleLayout();
    int elapsedTime() const;
    
    void setTextColor(const Color& color) { m_textColor = color; }
    Color textColor() const { return m_textColor; }

    const Color& linkColor() const { return m_linkColor; }
    const Color& visitedLinkColor() const { return m_visitedLinkColor; }
    const Color& activeLinkColor() const { return m_activeLinkColor; }
    void setLinkColor(const Color& c) { m_linkColor = c; }
    void setVisitedLinkColor(const Color& c) { m_visitedLinkColor = c; }
    void setActiveLinkColor(const Color& c) { m_activeLinkColor = c; }
    void resetLinkColor();
    void resetVisitedLinkColor();
    void resetActiveLinkColor();
    
    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const IntPoint&, const PlatformMouseEvent&);

    virtual bool childTypeAllowed(NodeType);
    virtual PassRefPtr<Node> cloneNode(bool deep);

    virtual bool canReplaceChild(Node* newChild, Node* oldChild);
    
    StyleSheetList* styleSheets();

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet() const;
    String selectedStylesheetSet() const;
    void setSelectedStylesheetSet(const String&);

    bool setFocusedNode(PassRefPtr<Node>);
    Node* focusedNode() const { return m_focusedNode.get(); }

    // The m_ignoreAutofocus flag specifies whether or not the document has been changed by the user enough 
    // for WebCore to ignore the autofocus attribute on any form controls
    bool ignoreAutofocus() const { return m_ignoreAutofocus; };
    void setIgnoreAutofocus(bool shouldIgnore = true) { m_ignoreAutofocus = shouldIgnore; };

    void setHoverNode(PassRefPtr<Node>);
    Node* hoverNode() const { return m_hoverNode.get(); }

    void setActiveNode(PassRefPtr<Node>);
    Node* activeNode() const { return m_activeNode.get(); }

    void focusedNodeRemoved();
    void removeFocusedNodeOfSubtree(Node*, bool amongChildrenOnly = false);
    void hoveredNodeDetached(Node*);
    void activeChainNodeDetached(Node*);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Node*);
    Node* getCSSTarget() const;
    
    void setDocumentChanged(bool);

    void attachNodeIterator(NodeIterator*);
    void detachNodeIterator(NodeIterator*);

    void attachRange(Range*);
    void detachRange(Range*);

    void nodeChildrenChanged(ContainerNode*);
    void nodeWillBeRemoved(Node*);

    void textInserted(Node*, unsigned offset, unsigned length);
    void textRemoved(Node*, unsigned offset, unsigned length);
    void textNodesMerged(Text* oldNode, unsigned offset);
    void textNodeSplit(Text* oldNode);

    DOMWindow* defaultView() const { return domWindow(); } 
    DOMWindow* domWindow() const;

    PassRefPtr<Event> createEvent(const String& eventType, ExceptionCode&);

    // keep track of what types of event listeners are registered, so we don't
    // dispatch events unnecessarily
    enum ListenerType {
        DOMSUBTREEMODIFIED_LISTENER          = 0x01,
        DOMNODEINSERTED_LISTENER             = 0x02,
        DOMNODEREMOVED_LISTENER              = 0x04,
        DOMNODEREMOVEDFROMDOCUMENT_LISTENER  = 0x08,
        DOMNODEINSERTEDINTODOCUMENT_LISTENER = 0x10,
        DOMATTRMODIFIED_LISTENER             = 0x20,
        DOMCHARACTERDATAMODIFIED_LISTENER    = 0x40,
        OVERFLOWCHANGED_LISTENER             = 0x80,
        ANIMATIONEND_LISTENER                = 0x100,
        ANIMATIONSTART_LISTENER              = 0x200,
        ANIMATIONITERATION_LISTENER          = 0x400,
        TRANSITIONEND_LISTENER               = 0x800
    };

    bool hasListenerType(ListenerType listenerType) const { return (m_listenerTypes & listenerType); }
    void addListenerType(ListenerType listenerType) { m_listenerTypes = m_listenerTypes | listenerType; }
    void addListenerTypeIfNeeded(const AtomicString& eventType);

    CSSStyleDeclaration* getOverrideStyle(Element*, const String& pseudoElt);

    void handleWindowEvent(Event*, bool useCapture);
    void setWindowInlineEventListenerForType(const AtomicString& eventType, PassRefPtr<EventListener>);
    EventListener* windowInlineEventListenerForType(const AtomicString& eventType);
    void removeWindowInlineEventListenerForType(const AtomicString& eventType);

    void setWindowInlineEventListenerForTypeAndAttribute(const AtomicString& eventType, Attribute*);

    void addWindowEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeWindowEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    bool hasWindowEventListener(const AtomicString& eventType);

    void markWindowEventListeners();

    void addPendingFrameUnloadEventCount();
    void removePendingFrameUnloadEventCount();
    void addPendingFrameBeforeUnloadEventCount();
    void removePendingFrameBeforeUnloadEventCount();

    PassRefPtr<EventListener> createEventListener(const String& functionName, const String& code, Node*);

    /**
     * Searches through the document, starting from fromNode, for the next selectable element that comes after fromNode.
     * The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab indexes
     * first (from lowest to highest), and then elements without tab indexes (in document order).
     *
     * @param fromNode The node from which to start searching. The node after this will be focused. May be null.
     *
     * @return The focus node that comes after fromNode
     *
     * See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
     */
    Node* nextFocusableNode(Node* start, KeyboardEvent*);

    /**
     * Searches through the document, starting from fromNode, for the previous selectable element (that comes _before_)
     * fromNode. The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab
     * indexes first (from lowest to highest), and then elements without tab indexes (in document order).
     *
     * @param fromNode The node from which to start searching. The node before this will be focused. May be null.
     *
     * @return The focus node that comes before fromNode
     *
     * See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
     */
    Node* previousFocusableNode(Node* start, KeyboardEvent*);

    int nodeAbsIndex(Node*);
    Node* nodeWithAbsIndex(int absIndex);

    /**
     * Handles a HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
     * when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
     * tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
     * specified in a HTML file.
     *
     * @param equiv The http header name (value of the meta tag's "equiv" attribute)
     * @param content The header value (value of the meta tag's "content" attribute)
     */
    void processHttpEquiv(const String& equiv, const String& content);
    void processArguments(const String & features, void *userData, void (*argumentsCallback)(const String& keyString, const String& valueString, Document * aDocument, void* userData));
    void processViewport(const String & features);
    void processFormatDetection(const String & features);
    
    void dispatchImageLoadEventSoon(ImageLoader*);
    void dispatchImageLoadEventsNow();
    void removeImage(ImageLoader*);
    
    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    Element* ownerElement() const;

    String title() const { return m_title; }
    void setTitle(const String&, Element* titleElement = 0);
    void removeTitle(Element* titleElement);

    String cookie() const;
    void setCookie(const String&);

    String referrer() const;

    String domain() const;
    void setDomain(const String& newDomain);

    String lastModified() const;

    const KURL& cookieURL() const { return m_cookieURL; }

    const KURL& policyBaseURL() const { return m_policyBaseURL; }
    void setPolicyBaseURL(const KURL& url) { m_policyBaseURL = url; }
    
    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const String&);

    // The following breaks a qualified name into a prefix and a local name.
    // It also does a validity check, and returns false if the qualified name
    // is invalid.  It also sets ExceptionCode when name is invalid.
    static bool parseQualifiedName(const String& qualifiedName, String& prefix, String& localName, ExceptionCode&);
    
    // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
    static bool hasPrefixNamespaceMismatch(const QualifiedName&);
    
    void addElementById(const AtomicString& elementId, Element *element);
    void removeElementById(const AtomicString& elementId, Element *element);

    void addImageMap(HTMLMapElement*);
    void removeImageMap(HTMLMapElement*);
    HTMLMapElement* getImageMap(const String& url) const;

    HTMLElement* body();
    void setBody(PassRefPtr<HTMLElement>, ExceptionCode&);

    HTMLHeadElement* head();

    bool execCommand(const String& command, bool userInterface = false, const String& value = String());
    bool queryCommandEnabled(const String& command);
    bool queryCommandIndeterm(const String& command);
    bool queryCommandState(const String& command);
    bool queryCommandSupported(const String& command);
    String queryCommandValue(const String& command);
    
    void addMarker(Range*, DocumentMarker::MarkerType, String description = String());
    void addMarker(Node*, DocumentMarker);
    void copyMarkers(Node *srcNode, unsigned startOffset, int length, Node *dstNode, int delta, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(Range*, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(Node*, unsigned startOffset, int length, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void removeMarkers(Node*);
    void repaintMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    void setRenderedRectForMarker(Node*, DocumentMarker, const IntRect&);
    void invalidateRenderedRectsForMarkersInRect(const IntRect&);
    void shiftMarkers(Node*, unsigned startOffset, int delta, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);

    DocumentMarker* markerContainingPoint(const IntPoint&, DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    Vector<DocumentMarker> markersForNode(Node*);
    Vector<IntRect> renderedRectsForMarkers(DocumentMarker::MarkerType = DocumentMarker::AllMarkers);
    
    // designMode support
    enum InheritedBool { off = false, on = true, inherit };    
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;

    Document* parentDocument() const;
    Document* topDocument() const;

    int docID() const { return m_docID; }

#if ENABLE(XSLT)
    void applyXSLTransform(ProcessingInstruction* pi);
    void setTransformSource(void* doc);
    const void* transformSource() { return m_transformSource; }
    PassRefPtr<Document> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(Document* doc) { m_transformSourceDocument = doc; }
#endif

#if ENABLE(XBL)
    // XBL methods
    XBLBindingManager* bindingManager() const { return m_bindingManager; }
#endif

    void incDOMTreeVersion() { ++m_domtree_version; }
    unsigned domTreeVersion() const { return m_domtree_version; }

    void setDocType(PassRefPtr<DocumentType>);

    virtual void finishedParsing();

#if ENABLE(XPATH)
    // XPathEvaluator methods
    PassRefPtr<XPathExpression> createExpression(const String& expression,
                                                 XPathNSResolver* resolver,
                                                 ExceptionCode& ec);
    PassRefPtr<XPathNSResolver> createNSResolver(Node *nodeResolver);
    PassRefPtr<XPathResult> evaluate(const String& expression,
                                     Node* contextNode,
                                     XPathNSResolver* resolver,
                                     unsigned short type,
                                     XPathResult* result,
                                     ExceptionCode& ec);
#endif // ENABLE(XPATH)
    
    enum PendingSheetLayout { NoLayoutWithPendingSheets, DidLayoutWithPendingSheets, IgnoreLayoutWithPendingSheets };

    bool didLayoutWithPendingStylesheets() const { return m_pendingSheetLayout == DidLayoutWithPendingSheets; }
    
    void setHasNodesWithPlaceholderStyle() { m_hasNodesWithPlaceholderStyle = true; }

    const String& iconURL() const { return m_iconURL; }
    void setIconURL(const String& iconURL, const String& type);

    void setUseSecureKeyboardEntryWhenActive(bool);
    bool useSecureKeyboardEntryWhenActive() const;
    
#if USE(LOW_BANDWIDTH_DISPLAY)
    void setDocLoader(DocLoader* loader) { m_docLoader = loader; }
    bool inLowBandwidthDisplay() const { return m_inLowBandwidthDisplay; }
    void setLowBandwidthDisplay(bool lowBandWidth) { m_inLowBandwidthDisplay = lowBandWidth; }
#endif
    
    void addNodeListCache() { ++m_numNodeListCaches; }
    void removeNodeListCache() { ASSERT(m_numNodeListCaches > 0); --m_numNodeListCaches; }
    bool hasNodeListCaches() const { return m_numNodeListCaches; }

    void updateFocusAppearanceSoon();
    void cancelFocusAppearanceUpdate();
        
    // FF method for accessing the selection added for compatability.
    DOMSelection* getSelection() const;
    
    // Extension for manipulating canvas drawing contexts for use in CSS
    CanvasRenderingContext2D* getCSSCanvasContext(const String& type, const String& name, int width, int height);
    HTMLCanvasElement* getCSSCanvasElement(const String& name);

    bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
    void initDNSPrefetch();
    void parseDNSPrefetchControlHeader(const String&);

    virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);
    virtual void addMessage(MessageDestination, MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);
    virtual void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
    virtual void postTask(PassRefPtr<Task>); // Executes the task on context's thread asynchronously.

    void addTimeout(int timeoutId, DOMTimer*);
    void removeTimeout(int timeoutId);
    DOMTimer* findTimeout(int timeoutId);
    
#if ENABLE(TOUCH_EVENTS)
#include "DocumentIPhone.h"
#endif

protected:
    Document(Frame*, bool isXHTML);

private:
    virtual void refScriptExecutionContext() { ref(); }
    virtual void derefScriptExecutionContext() { deref(); }

    virtual const KURL& virtualURL() const; // Same as url(), but needed for ScriptExecutionContext to implement it without a performance loss for direct calls.
    virtual KURL virtualCompleteURL(const String&) const; // Same as completeURL() for the same reason as above.

    CSSStyleSelector* m_styleSelector;
    bool m_didCalculateStyleSelector;

    Frame* m_frame;
    DocLoader* m_docLoader;
    Tokenizer* m_tokenizer;
    bool m_wellFormed;

    // Document URLs.
    KURL m_url;  // Document.URL: The URL from which this document was retrieved.
    KURL m_baseURL;  // Node.baseURI: The URL to use when resolving relative URLs.
    KURL m_baseElementURL;  // The URL set by the <base> element.
    KURL m_cookieURL;  // The URL to use for cookie access.
    KURL m_policyBaseURL;  // The policy URL for third-party cookie blocking.

    // Document.documentURI:
    // Although URL-like, Document.documentURI can actually be set to any
    // string by content.  Document.documentURI affects m_baseURL unless the
    // document contains a <base> element, in which case the <base> element
    // takes precedence.
    String m_documentURI;

    String m_baseTarget;

    RefPtr<DocumentType> m_docType;
    mutable RefPtr<DOMImplementation> m_implementation;

    RefPtr<StyleSheet> m_sheet;
#if FRAME_LOADS_USER_STYLESHEET
    String m_usersheet;
#endif

    // Track the number of currently loading top-level stylesheets.  Sheets
    // loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements.
    int m_pendingStylesheets;

    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets;

    // If we do ignore the pending stylesheet count, then we need to add a boolean
    // to track that this happened so that we can do a full repaint when the stylesheets
    // do eventually load.
    PendingSheetLayout m_pendingSheetLayout;
    
    bool m_hasNodesWithPlaceholderStyle;

    RefPtr<CSSStyleSheet> m_elemSheet;
    RefPtr<CSSStyleSheet> m_mappedElementSheet;

    bool m_printing;
    
    bool m_ignoreAutofocus;

    ParseMode m_parseMode;

    Color m_textColor;

    RefPtr<Node> m_focusedNode;
    RefPtr<Node> m_hoverNode;
    RefPtr<Node> m_activeNode;
    mutable RefPtr<Element> m_documentElement;

    unsigned m_domtree_version;
    
    HashSet<NodeIterator*> m_nodeIterators;
    HashSet<Range*> m_ranges;

    unsigned short m_listenerTypes;

    RefPtr<StyleSheetList> m_styleSheets; // All of the stylesheets that are currently in effect for our media type and stylesheet set.
    ListHashSet<Node*> m_styleSheetCandidateNodes; // All of the nodes that could potentially provide stylesheets to the document (<link>, <style>, <?xml-stylesheet>)

    RegisteredEventListenerVector m_windowEventListeners;

    typedef HashMap<FormElementKey, Vector<String>, FormElementKeyHash, FormElementKeyHashTraits> FormElementStateMap;
    ListHashSet<FormControlElementWithState*> m_formElementsWithState;
    FormElementStateMap m_stateForNewFormElements;
    
    Color m_linkColor;
    Color m_visitedLinkColor;
    Color m_activeLinkColor;

    String m_preferredStylesheetSet;
    String m_selectedStylesheetSet;

    bool m_loadingSheet;
    bool visuallyOrdered;
    bool m_bParsing;
    bool m_docChanged;
    bool m_inStyleRecalc;
    bool m_closeAfterStyleRecalc;
    bool m_usesDescendantRules;
    bool m_usesSiblingRules;
    bool m_usesFirstLineRules;
    bool m_usesFirstLetterRules;
    bool m_usesBeforeAfterRules;
    bool m_gotoAnchorNeededAfterStylesheetsLoad;
    bool m_isDNSPrefetchEnabled;
    bool m_haveExplicitlyDisabledDNSPrefetch;
    bool m_frameElementsShouldIgnoreScrolling;

    String m_title;
    bool m_titleSetExplicitly;
    RefPtr<Element> m_titleElement;
    
    RenderArena* m_renderArena;

    typedef std::pair<Vector<DocumentMarker>, Vector<IntRect> > MarkerMapVectorPair;
    typedef HashMap<RefPtr<Node>, MarkerMapVectorPair*> MarkerMap;
    MarkerMap m_markers;

    mutable AXObjectCache* m_axObjectCache;
    
    Vector<ImageLoader*> m_imageLoadEventDispatchSoonList;
    Vector<ImageLoader*> m_imageLoadEventDispatchingList;
    Timer<Document> m_imageLoadEventTimer;

    Timer<Document> m_updateFocusAppearanceTimer;

    Node* m_cssTarget;
    
    bool m_processingLoadEvent;
    double m_startTime;
    bool m_overMinimumLayoutThreshold;
    
#if ENABLE(XSLT)
    void* m_transformSource;
    RefPtr<Document> m_transformSourceDocument;
#endif

#if ENABLE(XBL)
    XBLBindingManager* m_bindingManager; // The access point through which documents and elements communicate with XBL.
#endif
    
    typedef HashMap<AtomicStringImpl*, HTMLMapElement*> ImageMapsByName;
    ImageMapsByName m_imageMapsByName;

    HashSet<Node*> m_disconnectedNodesWithEventListeners;

    int m_docID; // A unique document identifier used for things like document-specific mapped attributes.

    String m_xmlEncoding;
    String m_xmlVersion;
    bool m_xmlStandalone;

    String m_contentLanguage;

public:
    bool inPageCache() const { return m_inPageCache; }
    void setInPageCache(bool flag);
    
    // Elements can register themselves for the "documentWillBecomeInactive()" and  
    // "documentDidBecomeActive()" callbacks
    void registerForDocumentActivationCallbacks(Element*);
    void unregisterForDocumentActivationCallbacks(Element*);
    void documentWillBecomeInactive();
    void documentDidBecomeActive();

    void registerForMediaVolumeCallbacks(Element*);
    void unregisterForMediaVolumeCallbacks(Element*);
    void mediaVolumeDidChange();

    void setShouldCreateRenderers(bool);
    bool shouldCreateRenderers();
    
    void setDecoder(PassRefPtr<TextResourceDecoder>);
    TextResourceDecoder* decoder() const { return m_decoder.get(); }

    String displayStringModifiedByEncoding(const String& str) const {
        if (m_decoder)
            return m_decoder->encoding().displayString(str.impl());
        return str;
    }
    PassRefPtr<StringImpl> displayStringModifiedByEncoding(PassRefPtr<StringImpl> str) const {
        if (m_decoder)
            return m_decoder->encoding().displayString(str);
        return str;
    }
    void displayBufferModifiedByEncoding(UChar* buffer, unsigned len) const {
        if (m_decoder)
            m_decoder->encoding().displayBuffer(buffer, len);
    }

    // Quirk for the benefit of Apple's Dictionary application.
    void setFrameElementsShouldIgnoreScrolling(bool ignore) { m_frameElementsShouldIgnoreScrolling = ignore; }
    bool frameElementsShouldIgnoreScrolling() const { return m_frameElementsShouldIgnoreScrolling; }

#if ENABLE(DASHBOARD_SUPPORT)
    void setDashboardRegionsDirty(bool f) { m_dashboardRegionsDirty = f; }
    bool dashboardRegionsDirty() const { return m_dashboardRegionsDirty; }
    bool hasDashboardRegions () const { return m_hasDashboardRegions; }
    void setHasDashboardRegions (bool f) { m_hasDashboardRegions = f; }
    const Vector<DashboardRegionValue>& dashboardRegions() const;
    void setDashboardRegions(const Vector<DashboardRegionValue>&);
#endif

    void removeAllEventListenersFromAllNodes();

    void registerDisconnectedNodeWithEventListeners(Node*);
    void unregisterDisconnectedNodeWithEventListeners(Node*);
    
    HTMLFormElement::CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
#if ENABLE(SVG)
    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions* accessSVGExtensions();
#endif

    void initSecurityContext();

    // Explicitly override the security origin for this document.
    // Note: It is dangerous to change the security origin of a document
    //       that already contains content.
    void setSecurityOrigin(SecurityOrigin*);

    bool processingLoadEvent() const { return m_processingLoadEvent; }

#if ENABLE(DATABASE)
    void addOpenDatabase(Database*);
    void removeOpenDatabase(Database*);
    DatabaseThread* databaseThread();   // Creates the thread as needed, but not if it has been already terminated.
    void setHasOpenDatabases() { m_hasOpenDatabases = true; }
    bool hasOpenDatabases() { return m_hasOpenDatabases; }
    void stopDatabases();
#endif
    
    void setUsingGeolocation(bool f) { m_usingGeolocation = f; }
    bool usingGeolocation() const { return m_usingGeolocation; };

#if ENABLE(WML)
    void resetWMLPageState();
#endif

protected:
    void clearXMLVersion() { m_xmlVersion = String(); }

private:
    void updateTitle();
    void removeAllDisconnectedNodeEventListeners();
    void imageLoadEventTimerFired(Timer<Document>*);
    void updateFocusAppearanceTimerFired(Timer<Document>*);
    void updateBaseURL();

    void cacheDocumentElement() const;

    RenderObject* m_savedRenderer;
    int m_secureForms;
    
    RefPtr<TextResourceDecoder> m_decoder;

    // We maintain the invariant that m_duplicateIds is the count of all elements with a given ID
    // excluding the one referenced in m_elementsById, if any. This means it one less than the total count
    // when the first node with a given ID is cached, otherwise the same as the total count.
    mutable HashMap<AtomicStringImpl*, Element*> m_elementsById;
    mutable HashCountedSet<AtomicStringImpl*> m_duplicateIds;
    
    mutable HashMap<StringImpl*, Element*, CaseFoldingHash> m_elementsByAccessKey;
    
    InheritedBool m_designMode;
    
    int m_selfOnlyRefCount;

    HTMLFormElement::CheckedRadioButtons m_checkedRadioButtons;

    typedef HashMap<AtomicStringImpl*, HTMLCollection::CollectionInfo*> NamedCollectionMap;
    HTMLCollection::CollectionInfo m_collectionInfo[HTMLCollection::NumUnnamedDocumentCachedTypes];
    NamedCollectionMap m_nameCollectionInfo[HTMLCollection::NumNamedDocumentCachedTypes];

#if ENABLE(XPATH)
    RefPtr<XPathEvaluator> m_xpathEvaluator;
#endif
    
#if ENABLE(SVG)
    OwnPtr<SVGDocumentExtensions> m_svgExtensions;
#endif
    
#if ENABLE(DASHBOARD_SUPPORT)
    Vector<DashboardRegionValue> m_dashboardRegions;
    bool m_hasDashboardRegions;
    bool m_dashboardRegionsDirty;
#endif

    HashMap<String, RefPtr<HTMLCanvasElement> > m_cssCanvasElements;

    mutable bool m_accessKeyMapValid;
    bool m_createRenderers;
    bool m_inPageCache;
    String m_iconURL;
    
    HashSet<Element*> m_documentActivationCallbackElements;
    HashSet<Element*> m_mediaVolumeCallbackElements;

    bool m_useSecureKeyboardEntryWhenActive;

    bool m_isXHTML;

    unsigned m_numNodeListCaches;

public:
    typedef HashMap<WebCore::Node*, JSNode*> JSWrapperCache;
    JSWrapperCache& wrapperCache() { return m_wrapperCache; }
private:
    JSWrapperCache m_wrapperCache;

#if ENABLE(DATABASE)
    RefPtr<DatabaseThread> m_databaseThread;
    bool m_hasOpenDatabases;    // This never changes back to false, even as the database thread is closed.
    typedef HashSet<Database*> DatabaseSet;
    OwnPtr<DatabaseSet> m_openDatabaseSet;
#endif
    
    bool m_usingGeolocation;

#if USE(LOW_BANDWIDTH_DISPLAY)
    bool m_inLowBandwidthDisplay;
#endif

    typedef HashMap<int, DOMTimer*> TimeoutsMap;
    TimeoutsMap m_timeouts;

public:
    void addAutoSizingNode (Node *node, float size);
    void validateAutoSizingNodes ();
    void resetAutoSizingNodes();

    void setIsTelephoneNumberParsingEnabled(bool);
    bool isTelephoneNumberParsingEnabled() const;

    void incrementScrollEventListenersCount();
    void decrementScrollEventListenersCount();

    void incrementTotalImageDataSize(CachedImage* image);
    void decrementTotalImageDataSize(CachedImage* image);
    unsigned long totalImageDataSize();

    void incrementAnimatedImageDataCount(unsigned count);
    unsigned long animatedImageDataCount();
private:
    typedef HashMap<TextAutoSizingKey, RefPtr<TextAutoSizingValue>, TextAutoSizingHash, TextAutoSizingTraits> TextAutoSizingMap;
    TextAutoSizingMap m_textAutoSizedNodes;

    bool m_isTelephoneNumberParsingEnabled;
    unsigned m_scrollEventListenerCount;
    
    HashCountedSet<CachedImage*> m_documentImages;
    
    unsigned long m_totalImageDataSize;
    unsigned long m_animatedImageDataCount;
};

inline bool Document::hasElementWithId(AtomicStringImpl* id) const
{
    ASSERT(id);
    return m_elementsById.contains(id) || m_duplicateIds.contains(id);
}
    
inline bool Node::isDocumentNode() const
{
    return this == m_document.get();
}

} // namespace WebCore

#endif // Document_h
