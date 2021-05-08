# Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com> 
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebCore) \
    $(WebCore)/bindings/js \
    $(WebCore)/bindings/objc \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/html \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/page \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/xml \
    $(WebCore)/wml \
    $(WebCore)/svg \
#

DOM_CLASSES = \
    AbstractView \
    Attr \
    BarInfo \
    CDATASection \
    CSSCharsetRule \
    CSSFontFaceRule \
    CSSImportRule \
    CSSMediaRule \
    CSSPageRule \
    CSSPrimitiveValue \
    CSSRule \
    CSSRuleList \
    CSSStyleDeclaration \
    CSSStyleRule \
    CSSStyleSheet \
    CSSUnknownRule \
    CSSValue \
    CSSValueList \
    CSSVariablesRule \
    CSSVariablesDeclaration \
    CanvasGradient \
    CanvasPattern \
    CanvasRenderingContext2D \
    CharacterData \
    Clipboard \
    Comment \
    Console \
    Coordinates \
    Counter \
    DOMApplicationCache \
    DOMCoreException \
    DOMImplementation \
    DOMParser \
    DOMSelection \
    DOMStringList \
    DOMWindow \
    Database \
    Document \
    DocumentFragment \
    DocumentType \
    Element \
    ElementTimeControl \
    Entity \
    EntityReference \
    Event \
    EventException \
    EventListener \
    EventTarget \
    EventTargetNode \
    File \
    FileList \
    Geolocation \
    Geoposition \
    HTMLAnchorElement \
    HTMLAppletElement \
    HTMLAreaElement \
    HTMLAudioElement \
    HTMLBRElement \
    HTMLBaseElement \
    HTMLBaseFontElement \
    HTMLBlockquoteElement \
    HTMLBodyElement \
    HTMLButtonElement \
    HTMLCanvasElement \
    HTMLCollection \
    HTMLDListElement \
    HTMLDirectoryElement \
    HTMLDivElement \
    HTMLDocument \
    HTMLElement \
    HTMLEmbedElement \
    HTMLFieldSetElement \
    HTMLFontElement \
    HTMLFormElement \
    HTMLFrameElement \
    HTMLFrameSetElement \
    HTMLHRElement \
    HTMLHeadElement \
    HTMLHeadingElement \
    HTMLHtmlElement \
    HTMLIFrameElement \
    HTMLImageElement \
    HTMLInputElement \
    HTMLIsIndexElement \
    HTMLLIElement \
    HTMLLabelElement \
    HTMLLegendElement \
    HTMLLinkElement \
    HTMLMapElement \
    HTMLMarqueeElement \
    HTMLMediaElement \
    HTMLMenuElement \
    HTMLMetaElement \
    HTMLModElement \
    HTMLOListElement \
    HTMLObjectElement \
    HTMLOptGroupElement \
    HTMLOptionElement \
    HTMLOptionsCollection \
    HTMLParagraphElement \
    HTMLParamElement \
    HTMLPreElement \
    HTMLQuoteElement \
    HTMLScriptElement \
    HTMLSelectElement \
    HTMLSourceElement \
    HTMLStyleElement \
    HTMLTableCaptionElement \
    HTMLTableCellElement \
    HTMLTableColElement \
    HTMLTableElement \
    HTMLTableRowElement \
    HTMLTableSectionElement \
    HTMLTextAreaElement \
    HTMLTitleElement \
    HTMLUListElement \
    HTMLVideoElement \
    History \
    ImageData \
    KeyboardEvent \
    Location \
    MediaError \
    MediaList \
    MessageChannel \
    MessageEvent \
    MessagePort \
    MimeType \
    MimeTypeArray \
    MouseEvent \
    MutationEvent \
    NamedNodeMap \
    Navigator \
    Node \
    NodeFilter \
    NodeIterator \
    NodeList \
    Notation \
    OverflowEvent \
    Plugin \
    PluginArray \
    PositionCallback \
    PositionError \
    PositionErrorCallback \
    ProcessingInstruction \
    ProgressEvent \
    RGBColor \
    Range \
    RangeException \
    Rect \
    SQLError \
    SQLResultSet \
    SQLResultSetRowList \
    SQLTransaction \
    Storage \
    StorageEvent \
    SVGAElement \
    SVGAltGlyphElement \
    SVGAngle \
    SVGAnimateColorElement \
    SVGAnimateElement \
    SVGAnimateTransformElement \
    SVGAnimatedAngle \
    SVGAnimatedBoolean \
    SVGAnimatedEnumeration \
    SVGAnimatedInteger \
    SVGAnimatedLength \
    SVGAnimatedLengthList \
    SVGAnimatedNumber \
    SVGAnimatedNumberList \
    SVGAnimatedPathData \
    SVGAnimatedPoints \
    SVGAnimatedPreserveAspectRatio \
    SVGAnimatedRect \
    SVGAnimatedString \
    SVGAnimatedTransformList \
    SVGAnimationElement \
    SVGCircleElement \
    SVGClipPathElement \
    SVGColor \
    SVGComponentTransferFunctionElement \
    SVGCursorElement \
    SVGDefinitionSrcElement \
    SVGDefsElement \
    SVGDescElement \
    SVGDocument \
    SVGElement \
    SVGElementInstance \
    SVGElementInstanceList \
    SVGEllipseElement \
    SVGException \
    SVGExternalResourcesRequired \
    SVGFEBlendElement \
    SVGFEColorMatrixElement \
    SVGFEComponentTransferElement \
    SVGFECompositeElement \
    SVGFEDiffuseLightingElement \
    SVGFEDisplacementMapElement \
    SVGFEDistantLightElement \
    SVGFEFloodElement \
    SVGFEFuncAElement \
    SVGFEFuncBElement \
    SVGFEFuncGElement \
    SVGFEFuncRElement \
    SVGFEGaussianBlurElement \
    SVGFEImageElement \
    SVGFEMergeElement \
    SVGFEMergeNodeElement \
    SVGFEOffsetElement \
    SVGFEPointLightElement \
    SVGFESpecularLightingElement \
    SVGFESpotLightElement \
    SVGFETileElement \
    SVGFETurbulenceElement \
    SVGFilterElement \
    SVGFilterPrimitiveStandardAttributes \
    SVGFitToViewBox \
    SVGFontElement \
    SVGFontFaceElement \
    SVGFontFaceFormatElement \
    SVGFontFaceNameElement \
    SVGFontFaceSrcElement \
    SVGFontFaceUriElement \
    SVGForeignObjectElement \
    SVGGElement \
    SVGGlyphElement \
    SVGGradientElement \
    SVGHKernElement \
    SVGImageElement \
    SVGLangSpace \
    SVGLength \
    SVGLengthList \
    SVGLineElement \
    SVGLinearGradientElement \
    SVGLocatable \
    SVGMarkerElement \
    SVGMaskElement \
    SVGMatrix \
    SVGMetadataElement \
    SVGMissingGlyphElement \
    SVGNumber \
    SVGNumberList \
    SVGPaint \
    SVGPathElement \
    SVGPathSeg \
    SVGPathSegArcAbs \
    SVGPathSegArcRel \
    SVGPathSegClosePath \
    SVGPathSegCurvetoCubicAbs \
    SVGPathSegCurvetoCubicRel \
    SVGPathSegCurvetoCubicSmoothAbs \
    SVGPathSegCurvetoCubicSmoothRel \
    SVGPathSegCurvetoQuadraticAbs \
    SVGPathSegCurvetoQuadraticRel \
    SVGPathSegCurvetoQuadraticSmoothAbs \
    SVGPathSegCurvetoQuadraticSmoothRel \
    SVGPathSegLinetoAbs \
    SVGPathSegLinetoHorizontalAbs \
    SVGPathSegLinetoHorizontalRel \
    SVGPathSegLinetoRel \
    SVGPathSegLinetoVerticalAbs \
    SVGPathSegLinetoVerticalRel \
    SVGPathSegList \
    SVGPathSegMovetoAbs \
    SVGPathSegMovetoRel \
    SVGPatternElement \
    SVGPoint \
    SVGPointList \
    SVGPolygonElement \
    SVGPolylineElement \
    SVGPreserveAspectRatio \
    SVGRadialGradientElement \
    SVGRect \
    SVGRectElement \
    SVGRenderingIntent \
    SVGSVGElement \
    SVGScriptElement \
    SVGSetElement \
    SVGStopElement \
    SVGStringList \
    SVGStylable \
    SVGStyleElement \
    SVGSwitchElement \
    SVGSymbolElement \
    SVGTRefElement \
    SVGTSpanElement \
    SVGTests \
    SVGTextContentElement \
    SVGTextElement \
    SVGTextPathElement \
    SVGTextPositioningElement \
    SVGTitleElement \
    SVGTransform \
    SVGTransformList \
    SVGTransformable \
    SVGURIReference \
    SVGUnitTypes \
    SVGUseElement \
    SVGViewElement \
    SVGZoomAndPan \
    SVGZoomEvent \
    Screen \
    StyleSheet \
    StyleSheetList \
    Text \
    TextEvent \
    TextMetrics \
    TimeRanges \
    TreeWalker \
    UIEvent \
    VoidCallback \
    WebKitAnimationEvent \
    WebKitCSSKeyframeRule \
    WebKitCSSKeyframesRule \
    WebKitCSSMatrix \
    WebKitCSSTransformValue \
    WebKitPoint \
    WebKitTransitionEvent \
    WheelEvent \
    Worker \
    WorkerContext \
    WorkerLocation \
    WorkerNavigator \
    XMLHttpRequest \
    XMLHttpRequestException \
    XMLHttpRequestProgressEvent \
    XMLHttpRequestUpload \
    XMLSerializer \
    XPathEvaluator \
    XPathException \
    XPathExpression \
    XPathNSResolver \
    XPathResult \
    XSLTProcessor \
    \
    GestureEvent \
    Touch \
    TouchEvent \
    TouchList \
#

.PHONY : all

all : \
    $(filter-out JSEventListener.h JSRGBColor.h,$(DOM_CLASSES:%=JS%.h)) \
    \
    JSDOMWindowBase.lut.h \
    JSRGBColor.lut.h \
    JSWorkerContextBase.lut.h \
    \
    JSJavaScriptCallFrame.h \
    \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLEntityNames.c \
    HTMLNames.cpp \
    WMLElementFactory.cpp \
    WMLNames.cpp \
    JSSVGElementWrapperFactory.cpp \
    SVGElementFactory.cpp \
    SVGNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNames.cpp \
    XPathGrammar.cpp \
    tokenizer.cpp \
#

# --------

ifeq ($(OS),MACOS)

FRAMEWORK_FLAGS = $(shell echo $(FRAMEWORK_SEARCH_PATHS) | perl -e 'print "-F " . join(" -F ", split(" ", <>));')

ifeq ($(shell /usr/bin/gcc -isysroot $(SDKROOT) -E -P -dM -F $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_DASHBOARD_SUPPORT | cut -d' ' -f3), 1)
    ENABLE_DASHBOARD_SUPPORT = 1
else
    ENABLE_DASHBOARD_SUPPORT = 0
endif

else

ENABLE_DASHBOARD_SUPPORT = 0

endif

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSPropertyNames.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/SVGCSSPropertyNames.in
    WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in
endif

ifeq ($(ENABLE_DASHBOARD_SUPPORT), 1)
    WEBCORE_CSS_PROPERTY_NAMES := $(WEBCORE_CSS_PROPERTY_NAMES) $(WebCore)/css/DashboardSupportCSSPropertyNames.in
endif

CSSPropertyNames.h : $(WEBCORE_CSS_PROPERTY_NAMES) css/makeprop.pl
	if sort $(WEBCORE_CSS_PROPERTY_NAMES) | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $(WEBCORE_CSS_PROPERTY_NAMES) > CSSPropertyNames.in
	perl "$(WebCore)/css/makeprop.pl"

CSSValueKeywords.h : $(WEBCORE_CSS_VALUE_KEYWORDS) css/makevalues.pl
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	if sort CSSValueKeywords.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	perl "$(WebCore)/css/makevalues.pl"

# --------

# DOCTYPE strings

DocTypeStrings.cpp : html/DocTypeStrings.gperf
	gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

# --------

# HTML entity names

HTMLEntityNames.c : html/HTMLEntityNames.gperf
	gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@

# --------

# color names

ColorData.c : platform/ColorData.gperf
	gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@

# --------

# CSS tokenizer

tokenizer.cpp : css/tokenizer.flex css/maketokenizer
	flex -t $< | perl $(WebCore)/css/maketokenizer > $@

# --------

# CSS grammar
# NOTE: Older versions of bison do not inject an inclusion guard, so we add one.

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	echo '#ifndef CSSGrammar_h' > CSSGrammar.h
	echo '#define CSSGrammar_h' >> CSSGrammar.h
	cat CSSGrammar.cpp.h CSSGrammar.hpp >> CSSGrammar.h
	echo '#endif' >> CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# --------

# XPath grammar
# NOTE: Older versions of bison do not inject an inclusion guard, so we add one.

XPathGrammar.cpp : xml/XPathGrammar.y $(PROJECT_FILE)
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	echo '#ifndef XPathGrammar_h' > XPathGrammar.h
	echo '#define XPathGrammar_h' >> XPathGrammar.h
	cat XPathGrammar.cpp.h XPathGrammar.hpp >> XPathGrammar.h
	echo '#endif' >> XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# --------

# user agent style sheets

USER_AGENT_STYLE_SHEETS = html4.css.out quirks.css.out view-source.css.out themeWin.css.out themeWinQuirks.css.out 

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) svg.css.out
endif

ifeq ($(findstring ENABLE_WML,$(FEATURE_DEFINES)), ENABLE_WML)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) wml.css.out
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) mediaControls.css.out
ifeq ($(OS),MACOS)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) mediaControlsQT.css.out
endif
endif

UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS)
	perl $< $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

%.css.out : %.css
	/usr/bin/g++ -E -x c -P -C -DIPHONE=1 $^ > $@

# --------

# lookup tables for old-style JavaScript bindings

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< -n WebCore > $@
%Table.cpp: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< -n WebCore > $@

# --------

# HTML tag and attribute names

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_VIDEO=1
endif

ifdef HTML_FLAGS

HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --wrapperFactory --extraDefines "$(HTML_FLAGS)"

else

HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --wrapperFactory

endif

XMLNames.cpp : dom/make_names.pl xml/xmlattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

# SVG tag and attribute names, and element factory

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.exp
endif

ifeq ($(findstring ENABLE_SVG_USE,$(FEATURE_DEFINES)), ENABLE_SVG_USE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_USE=1
endif

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

ifeq ($(findstring ENABLE_SVG_FILTERS,$(FEATURE_DEFINES)), ENABLE_SVG_FILTERS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FILTERS=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Filters.exp
endif
endif

ifeq ($(findstring ENABLE_SVG_AS_IMAGE,$(FEATURE_DEFINES)), ENABLE_SVG_AS_IMAGE)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_AS_IMAGE=1
endif

ifeq ($(findstring ENABLE_SVG_ANIMATION,$(FEATURE_DEFINES)), ENABLE_SVG_ANIMATION)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_ANIMATION=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.Animation.exp
endif
endif

ifeq ($(findstring ENABLE_SVG_FOREIGN_OBJECT,$(FEATURE_DEFINES)), ENABLE_SVG_FOREIGN_OBJECT)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FOREIGN_OBJECT=1
ifeq ($(findstring ENABLE_SVG_DOM_OBJC_BINDINGS,$(FEATURE_DEFINES)), ENABLE_SVG_DOM_OBJC_BINDINGS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.SVG.ForeignObject.exp
endif
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)

ifdef SVG_FLAGS

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory
else

SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory

endif

JSSVGElementWrapperFactory.cpp : SVGNames.cpp

XLinkNames.cpp : dom/make_names.pl svg/xlinkattrs.in
	perl -I $(WebCore)/bindings/scripts $< --attrs $(WebCore)/svg/xlinkattrs.in

else

SVGElementFactory.cpp :
	echo > $@

SVGNames.cpp :
	echo > $@

XLinkNames.cpp :
	echo > $@

# This file is autogenerated by make_names.pl when SVG is enabled.

JSSVGElementWrapperFactory.cpp :
	echo > $@

endif

# --------

# WML tag and attribute names, and element factory

ifeq ($(findstring ENABLE_WML,$(FEATURE_DEFINES)), ENABLE_WML)

WMLElementFactory.cpp WMLNames.cpp : dom/make_names.pl wml/WMLTagNames.in wml/WMLAttributeNames.in
	perl -I $(WebCore)/bindings/scripts $< --tags $(WebCore)/wml/WMLTagNames.in --attrs $(WebCore)/wml/WMLAttributeNames.in --factory --wrapperFactory

else

WMLElementFactory.cpp :
	echo > $@

WMLNames.cpp :
	echo > $@

endif


# --------

# JavaScript bindings

GENERATE_BINDINGS = perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl \
    --include dom --include html --include css --include page --include xml --include svg --outputDir .

GENERATE_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#

JS%.h : %.idl $(GENERATE_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorJS.pm
	$(GENERATE_BINDINGS) --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS $<

# ------------------------

# Mac-specific rules

ifeq ($(OS),MACOS)

all : $(filter-out DOMDOMWindow.h DOMMimeType.h DOMPlugin.h,$(DOM_CLASSES:%=DOM%.h))

all : CharsetData.cpp WebCore.exp

# --------

# character set name table

CharsetData.cpp : platform/text/mac/make-charset-table.pl platform/text/mac/character-sets.txt platform/text/mac/mac-encodings.txt
	perl $^ kTextEncoding > $@

# --------

# export file

ifeq ($(findstring ENABLE_TOUCH_EVENTS,$(FEATURE_DEFINES)), ENABLE_TOUCH_EVENTS)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.Touch.exp
endif

ifeq ($(shell /usr/bin/gcc -isysroot $(SDKROOT) -E -P -dM -F $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ' WTF_PLATFORM_IPHONE ' | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.iPhone.exp
endif

ifeq ($(findstring ENABLE_IPHONE_PPT,$(FEATURE_DEFINES)), ENABLE_IPHONE_PPT)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.iPhonePPT.exp
endif

ifeq ($(shell /usr/bin/gcc -isysroot $(SDKROOT) -E -P -dM -F $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_MAC_JAVA_BRIDGE | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.JNI.exp
endif

# See also "Generate 64-bit Export File" build phase script in WebCore.xcodeproj/project.pbxproj
ifeq ($(shell /usr/bin/gcc -isysroot $(SDKROOT) -E -P -dM -F $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_FLAGS) WebCore/ForwardingHeaders/wtf/Platform.h | grep ENABLE_NETSCAPE_PLUGIN_API | cut -d' ' -f3), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.NPAPI.exp
endif

ifeq ($(ENABLE_DASHBOARD_SUPPORT), 1)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.DashboardSupport.exp
endif

ifeq ($(findstring 10.4,$(MACOSX_DEPLOYMENT_TARGET)), 10.4)
    WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.Tiger.exp
endif

ifeq ($(findstring ENABLE_PLUGIN_PROXY_FOR_VIDEO,$(FEATURE_DEFINES)), ENABLE_PLUGIN_PROXY_FOR_VIDEO)
     WEBCORE_EXPORT_DEPENDENCIES := $(WEBCORE_EXPORT_DEPENDENCIES) WebCore.VideoProxy.exp
endif

WebCore.exp : WebCore.base.exp $(WEBCORE_EXPORT_DEPENDENCIES)
	cat $^ > $@

# --------

# Objective-C bindings

DOM%.h : %.idl $(GENERATE_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorObjC.pm bindings/objc/PublicDOMInterfaces.h
	$(GENERATE_BINDINGS) --defines "$(FEATURE_DEFINES) LANGUAGE_OBJECTIVE_C" --generator ObjC $<

# --------

endif

# ------------------------
