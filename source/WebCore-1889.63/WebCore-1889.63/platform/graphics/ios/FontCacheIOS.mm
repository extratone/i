/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FontCache.h"

#import "Font.h"
#import "RenderThemeIOS.h"
#import <CoreText/CTFontDescriptorPriv.h>
#import <CoreText/CTFontPriv.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>

namespace WebCore {

void FontCache::platformInit()
{
    wkSetUpFontCache();
}

static inline bool isGSFontWeightBold(NSInteger fontWeight)
{
    return fontWeight >= FontWeight600;
}

static inline bool requiresCustomFallbackFont(const UInt32 character)
{
    return character == AppleLogo || character == BigDot;
}

static CFCharacterSetRef copyFontCharacterSet(CFStringRef fontName)
{
    // The size, 10, is arbitrary.
    RetainPtr<CTFontDescriptorRef> fontDescriptor(AdoptCF, CTFontDescriptorCreateWithNameAndSize(fontName, 10));
    RetainPtr<GSFontRef> font(AdoptCF, CTFontCreateWithFontDescriptor(fontDescriptor.get(), 10, NULL));
    return (CFCharacterSetRef)CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontCharacterSetAttribute);
}

static CFCharacterSetRef appleColorEmojiCharacterSet()
{
    static CFCharacterSetRef characterSet = copyFontCharacterSet(CFSTR("AppleColorEmoji"));
    return characterSet;
}

static CFCharacterSetRef phoneFallbackCharacterSet()
{
    static CFCharacterSetRef characterSet = copyFontCharacterSet(CFSTR(".PhoneFallback"));
    return characterSet;
}

PassRefPtr<SimpleFontData> FontCache::getSystemFontFallbackForCharacters(const FontDescription& description, const SimpleFontData* originalFontData, const UChar* characters, int length)
{
    const FontPlatformData& platformData = originalFontData->platformData();
    GSFontRef gsFont = platformData.font();

    CFIndex coveredLength = 0;
    RetainPtr<CTFontRef> substituteFont(AdoptCF, CTFontCreatePhysicalFontForCharactersWithLanguage(gsFont, (const UTF16Char*)characters, (CFIndex)length, 0, &coveredLength));
    if (!substituteFont)
        return 0;

    GSFontTraitMask originalTraits = GSFontGetTraits(gsFont);
    GSFontTraitMask actualTraits = 0;
    if (isGSFontWeightBold(description.weight()) || description.italic())
        actualTraits = GSFontGetTraits(substituteFont.get());

    bool syntheticBold = (originalTraits & GSBoldFontMask) && !(actualTraits & GSBoldFontMask);
    bool syntheticOblique = (originalTraits & GSItalicFontMask) && !(actualTraits & GSItalicFontMask);

    FontPlatformData alternateFont(substituteFont.get(), platformData.size(), platformData.isPrinterFont(), syntheticBold, syntheticOblique, platformData.m_orientation);
    alternateFont.m_isEmoji = CTFontIsAppleColorEmoji(substituteFont.get());
    
    return getCachedFontData(&alternateFont, DoNotRetain);
}

PassRefPtr<SimpleFontData> FontCache::systemFallbackForCharacters(const FontDescription& description, const SimpleFontData* originalFontData, bool, const UChar* characters, int length)
{
    static bool isGB18030ComplianceRequired = wkIsGB18030ComplianceRequired();

    // Unlike OS X, our fallback font on iPhone is Arial Unicode, which doesn't have some apple-specific glyphs like F8FF.
    // Fall back to the Apple Fallback font in this case.
    if (length > 0 && requiresCustomFallbackFont(*characters))
        return getCachedFontData(getCustomFallbackFont(*characters, description), DoNotRetain);

    UChar32 c = *characters;
    if (length > 1 && U16_IS_LEAD(c) && U16_IS_TRAIL(characters[1]))
        c = U16_GET_SUPPLEMENTARY(c, characters[1]);

    // For system fonts we use CoreText fallback mechanism.
    if (length) {
        RetainPtr<CTFontDescriptorRef> fontDescriptor(AdoptCF, CTFontCopyFontDescriptor(originalFontData->getGSFont()));
        if (CTFontDescriptorIsSystemUIFont(fontDescriptor.get()))
            return getSystemFontFallbackForCharacters(description, originalFontData, characters, length);
    }

    bool useCJFont = false;
    bool useKoreanFont = false;
    bool useCyrillicFont = false;
    bool useArabicFont = false;
    bool useHebrewFont = false;
    bool useIndicFont = false;
    bool useThaiFont = false;
    bool useTibetanFont = false;
    bool useCanadianAboriginalSyllabicsFont = false;
    bool useEmojiFont = false;
    if (length > 0) {
        do {
            // This isn't a loop but a way to efficiently check for ranges of characters.

            // The following ranges are Korean Hangul and should be rendered by AppleSDGothicNeo
            // U+1100 - U+11FF
            // U+3130 - U+318F
            // U+AC00 - U+D7A3

            // These are Cyrillic and should be rendered by Helvetica Neue
            // U+0400 - U+052F
            
            if (c < 0x400)
                break;
            if (c <= 0x52F) {
                useCyrillicFont = true;
                break;
            }
            if (c < 0x590)
                break;
            if (c < 0x600) {
                useHebrewFont = true;
                break;
            }
            if (c <= 0x6FF) {
                useArabicFont = true;
                break;
            }
            if (c < 0x900)
                break;
            if (c < 0xE00) {
                useIndicFont = true;
                break;
            }
            if (c <= 0xE7F) {
                useThaiFont = true;
                break;
            }
            if (c < 0x0F00)
                break;
            if (c <= 0x0FFF) {
                useTibetanFont = true;
                break;
            }
            if (c < 0x1100)
                break;
            if (c <= 0x11FF) {
                useKoreanFont = true;
                break;
            }
            if (c > 0x1400 && c < 0x1780) {
                useCanadianAboriginalSyllabicsFont = true;
                break;
            }
            if (c < 0x2E80)
                break;
            if (c < 0x3130) {
                useCJFont = true;
                break;
            }
            if (c <= 0x318F) {
                useKoreanFont = true;
                break;
            }
            if (c < 0xAC00) {
                useCJFont = true;
                break;
            }
            if (c <= 0xD7A3) {
                useKoreanFont = true;
                break;
            }
            if ( c <= 0xDFFF) {
                useCJFont = true;
                break;
            }
            if ( c < 0xE000)
                break;
            if ( c < 0xE600) {
                if (isGB18030ComplianceRequired)
                    useCJFont = true;
                else
                    useEmojiFont = true;
                break;
            }
            if ( c <= 0xE864 && isGB18030ComplianceRequired) {
                useCJFont = true;
                break;
            }
            if (c <= 0xF8FF)
                break;
            if (c < 0xFB00) {
                useCJFont = true;
                break;
            }
            if (c < 0xFB50)
                break;
            if (c <= 0xFDFF) {
                useArabicFont = true;
                break;
            }
            if (c < 0xFE20)
                break;
            if (c < 0xFE70) {
                useCJFont = true;
                break;
            }
            if (c < 0xFF00) {
                useArabicFont = true;
                break;
            }
            if (c < 0xFFF0) {
                useCJFont = true;
                break;
            }
            if (c >=0x20000 && c <= 0x2FFFF)
                useCJFont = true;
        } while (0);
    }
        
    RefPtr<SimpleFontData> simpleFontData;
    
    if (useCJFont) {
        // by default, Chinese font is preferred,  fall back on Japanese

        enum CJKFontVariant {
            kCJKFontUseHiragino = 0,
            kCJKFontUseSTHeitiSC,
            kCJKFontUseSTHeitiTC,
            kCJKFontUseSTHeitiJ,
            kCJKFontUseSTHeitiK,
            kCJKFontsUseHKGPW3UI
        };

        DEFINE_STATIC_LOCAL(AtomicString, plainHiragino, ("HiraKakuProN-W3"));
        DEFINE_STATIC_LOCAL(AtomicString, plainSTHeitiSC, ("STHeitiSC-Light"));
        DEFINE_STATIC_LOCAL(AtomicString, plainSTHeitiTC, ("STHeitiTC-Light"));
        DEFINE_STATIC_LOCAL(AtomicString, plainSTHeitiJ, ("STHeitiJ-Light"));
        DEFINE_STATIC_LOCAL(AtomicString, plainSTHeitiK, ("STHeitiK-Light"));
        DEFINE_STATIC_LOCAL(AtomicString, plainHKGPW3UI, ("HKGPW3UI"));
        static AtomicString* cjkPlain[] = {     
            &plainHiragino,
            &plainSTHeitiSC,
            &plainSTHeitiTC,
            &plainSTHeitiJ,
            &plainSTHeitiK,
            &plainHKGPW3UI,
        };

        DEFINE_STATIC_LOCAL(AtomicString, boldHiragino, ("HiraKakuProN-W6"));
        DEFINE_STATIC_LOCAL(AtomicString, boldSTHeitiSC, ("STHeitiSC-Medium"));
        DEFINE_STATIC_LOCAL(AtomicString, boldSTHeitiTC, ("STHeitiTC-Medium"));
        DEFINE_STATIC_LOCAL(AtomicString, boldSTHeitiJ, ("STHeitiJ-Medium"));
        DEFINE_STATIC_LOCAL(AtomicString, boldSTHeitiK, ("STHeitiK-Medium"));
        DEFINE_STATIC_LOCAL(AtomicString, boldHKGPW3UI, ("HKGPW3UI"));
        static AtomicString* cjkBold[] = {  
            &boldHiragino,
            &boldSTHeitiSC,
            &boldSTHeitiTC,
            &boldSTHeitiJ,
            &boldSTHeitiK,
            &boldHKGPW3UI,
        };

        //default below is for Simplified Chinese user: zh-Hans - note that Hiragino is the
        //the secondary font as we want its for Hiragana and Katakana. The other CJK fonts
        //do not, and should not, contain Hiragana or Katakana glyphs.
        static CJKFontVariant preferredCJKFont = kCJKFontUseSTHeitiSC;
        static CJKFontVariant secondaryCJKFont = kCJKFontsUseHKGPW3UI;
        
        static bool CJKFontInitialized = false;
        if (!CJKFontInitialized) {
            CJKFontInitialized = true;
            // Testing: languageName = (CFStringRef)@"ja";
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            NSArray *languages = [defaults stringArrayForKey:@"AppleLanguages"];
            
            if( languages ) {
                CFStringRef languageName = NULL;
                for( NSString *language in languages) {
                    languageName = CFLocaleCreateCanonicalLanguageIdentifierFromString(NULL, (CFStringRef)language);
                    if ( CFEqual(languageName, CFSTR("zh-Hans")) )
                        break;                                  //Simplified Chinese - default settings
                    else if ( CFEqual(languageName, CFSTR("ja")) ) {    
                        preferredCJKFont=kCJKFontUseHiragino;   //japanese - prefer Hiragino and STHeiti Japanse Variant
                        secondaryCJKFont=kCJKFontUseSTHeitiJ;
                        break;
                    } else if ( CFEqual(languageName, CFSTR("ko")) ) {  
                        preferredCJKFont=kCJKFontUseSTHeitiK;   //korean - prefer STHeiti Korean Variant 
                        break;
                    } else if ( CFEqual(languageName, CFSTR("zh-Hant")) ) {
                        preferredCJKFont=kCJKFontUseSTHeitiTC;  //Traditional Chinese - prefer STHeiti Traditional Variant
                        break;
                    }
                    CFRelease(languageName);
                    languageName = NULL;
                }
                if( languageName )
                    CFRelease(languageName);
            }
        }
        
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? *cjkBold[preferredCJKFont] : *cjkPlain[preferredCJKFont], false, DoNotRetain);
        bool useSecondaryFont = true;
        if (simpleFontData) {
            CGGlyph glyphs[2];
            // CGFontGetGlyphsForUnichars takes UTF-16 buffer. Should only be 1 codepoint but since we may pass in two UTF-16 characters,
            // make room for 2 glyphs just to be safe.
            CGFontGetGlyphsForUnichars(simpleFontData->platformData().cgFont(), characters, glyphs, length);

            useSecondaryFont = (glyphs[0] == 0);
        }

        if (useSecondaryFont)
            simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? *cjkBold[secondaryCJKFont] : *cjkPlain[secondaryCJKFont], false, DoNotRetain);
    } else if (useKoreanFont) {
        DEFINE_STATIC_LOCAL(AtomicString, koreanPlain, ("AppleSDGothicNeo-Medium"));
        DEFINE_STATIC_LOCAL(AtomicString, koreanBold, ("AppleSDGothicNeo-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? koreanBold : koreanPlain, false, DoNotRetain);
    } else if (useCyrillicFont) {
        DEFINE_STATIC_LOCAL(AtomicString, cyrillicPlain, ("HelveticaNeue"));
        DEFINE_STATIC_LOCAL(AtomicString, cyrillicBold, ("HelveticaNeue-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? cyrillicBold : cyrillicPlain, false, DoNotRetain);
    } else if (useArabicFont) {
        DEFINE_STATIC_LOCAL(AtomicString, arabicPlain, ("GeezaPro"));
        DEFINE_STATIC_LOCAL(AtomicString, arabicBold, ("GeezaPro-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? arabicBold : arabicPlain, false, DoNotRetain);
    } else if (useHebrewFont) {
        DEFINE_STATIC_LOCAL(AtomicString, hebrewPlain, ("ArialHebrew"));
        DEFINE_STATIC_LOCAL(AtomicString, hebrewBold, ("ArialHebrew-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? hebrewBold : hebrewPlain, false, DoNotRetain);
    } else if (useIndicFont) {
        DEFINE_STATIC_LOCAL(AtomicString, devanagariFont, ("DevanagariSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, bengaliFont, ("BanglaSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, gurmukhiFont, ("GurmukhiMN")); // Might be replaced in a future release with a Sangam version.
        DEFINE_STATIC_LOCAL(AtomicString, gujaratiFont, ("GujaratiSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, oriyaFont, ("OriyaSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, tamilFont, ("TamilSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, teluguFont, ("TeluguSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, kannadaFont, ("KannadaSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, malayalamFont, ("MalayalamSangamMN"));
        DEFINE_STATIC_LOCAL(AtomicString, sinhalaFont, ("SinhalaSangamMN"));

        DEFINE_STATIC_LOCAL(AtomicString, devanagariFontBold, ("DevanagariSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, bengaliFontBold, ("BanglaSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, gurmukhiFontBold, ("GurmukhiMN-Bold")); // Might be replaced in a future release with a Sangam version.
        DEFINE_STATIC_LOCAL(AtomicString, gujaratiFontBold, ("GujaratiSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, oriyaFontBold, ("OriyaSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, tamilFontBold, ("TamilSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, teluguFontBold, ("TeluguSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, kannadaFontBold, ("KannadaSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, malayalamFontBold, ("MalayalamSangamMN-Bold"));
        DEFINE_STATIC_LOCAL(AtomicString, sinhalaFontBold, ("SinhalaSangamMN-Bold"));

        static AtomicString* indicUnicodePageFonts[] = {
            &devanagariFont,
            &bengaliFont,
            &gurmukhiFont,
            &gujaratiFont,
            &oriyaFont,
            &tamilFont,
            &teluguFont,
            &kannadaFont,
            &malayalamFont,
            &sinhalaFont
        };

        static AtomicString* indicUnicodePageFontsBold[] = {
            &devanagariFontBold,
            &bengaliFontBold,
            &gurmukhiFontBold,
            &gujaratiFontBold,
            &oriyaFontBold,
            &tamilFontBold,
            &teluguFontBold,
            &kannadaFontBold,
            &malayalamFontBold,
            &sinhalaFontBold
        };

        uint32_t indicPageOrderIndex = (c - 0x0900) / 0x0080;    // Indic scripts start at 0x0900 in Unicode. Each script is allocalted a block of 0x80 characters.
        if (indicPageOrderIndex < (sizeof(indicUnicodePageFonts) / sizeof(AtomicString*))) {
            AtomicString* indicFontString = isGSFontWeightBold(description.weight()) ? indicUnicodePageFontsBold[indicPageOrderIndex] : indicUnicodePageFonts[indicPageOrderIndex];
            if (indicFontString)
                simpleFontData = getCachedFontData(description, *indicFontString, false, DoNotRetain);
        }
    } else if (useThaiFont) {
        DEFINE_STATIC_LOCAL(AtomicString, thaiPlain, ("Thonburi"));
        DEFINE_STATIC_LOCAL(AtomicString, thaiBold, ("Thonburi-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? thaiBold : thaiPlain, false, DoNotRetain);
    } else if (useTibetanFont) {
        DEFINE_STATIC_LOCAL(AtomicString, tibetanPlain, ("Kailasa"));
        DEFINE_STATIC_LOCAL(AtomicString, tibetanBold, ("Kailasa-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? tibetanBold : tibetanPlain, false, DoNotRetain);
    } else if (useCanadianAboriginalSyllabicsFont) {
        DEFINE_STATIC_LOCAL(AtomicString, casPlain, ("EuphemiaUCAS"));
        DEFINE_STATIC_LOCAL(AtomicString, casBold, ("EuphemiaUCAS-Bold"));
        simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? casBold : casPlain, false, DoNotRetain);
    } else {
        DEFINE_STATIC_LOCAL(AtomicString, appleColorEmoji, ("AppleColorEmoji", AtomicString::ConstructFromLiteral));
        if (!useEmojiFont) {
            if (!CFCharacterSetIsLongCharacterMember(phoneFallbackCharacterSet(), c))
                useEmojiFont = CFCharacterSetIsLongCharacterMember(appleColorEmojiCharacterSet(), c);
        }
        if (useEmojiFont)
            simpleFontData = getCachedFontData(description, appleColorEmoji, false, DoNotRetain);
    }

    if (simpleFontData)
        return simpleFontData.release();

    return getNonRetainedLastResortFallbackFont(description);
}

PassRefPtr<SimpleFontData> FontCache::similarFontPlatformData(const FontDescription& description)
{
    // Attempt to find an appropriate font using a match based on 
    // the presence of keywords in the the requested names.  For example, we'll
    // match any name that contains "Arabic" to Geeza Pro.
    RefPtr<SimpleFontData> simpleFontData;
    for (unsigned i = 0; i < description.familyCount(); ++i) {
        const AtomicString& family = description.familyAt(i);
        if (family.isEmpty())
            continue;

        // Substitute the default monospace font for well-known monospace fonts.
        DEFINE_STATIC_LOCAL(AtomicString, monaco, ("monaco"));
        DEFINE_STATIC_LOCAL(AtomicString, menlo, ("menlo"));
        DEFINE_STATIC_LOCAL(AtomicString, courier, ("courier"));
        if (equalIgnoringCase(family, monaco) || equalIgnoringCase(family, menlo)) {
            simpleFontData = getCachedFontData(description, courier);
            continue;
        }

        // Substitute Verdana for Lucida Grande.
        DEFINE_STATIC_LOCAL(AtomicString, lucidaGrande, ("lucida grande"));
        DEFINE_STATIC_LOCAL(AtomicString, verdana, ("verdana"));
        if (equalIgnoringCase(family, lucidaGrande)) {
            simpleFontData = getCachedFontData(description, verdana);
            continue;
        }
        static String* matchWords[3] = { new String("Arabic"), new String("Pashto"), new String("Urdu") };
        DEFINE_STATIC_LOCAL(AtomicString, geezaPlain, ("GeezaPro"));
        DEFINE_STATIC_LOCAL(AtomicString, geezaBold, ("GeezaPro-Bold"));
        for (int j = 0; j < 3 && !simpleFontData; ++j)
            if (family.contains(*matchWords[j], false))
                simpleFontData = getCachedFontData(description, isGSFontWeightBold(description.weight()) ? geezaBold : geezaPlain);
    }

    return simpleFontData.release();
}

PassRefPtr<SimpleFontData> FontCache::getLastResortFallbackFont(const FontDescription& fontDescription, ShouldRetain shouldRetain)
{
    DEFINE_STATIC_LOCAL(AtomicString, fallbackFontStr, (".PhoneFallback"));
    return getCachedFontData(fontDescription, fallbackFontStr, false, shouldRetain);
}

FontPlatformData* FontCache::getCustomFallbackFont(const UInt32 c, const FontDescription& description)
{
    ASSERT(requiresCustomFallbackFont(c));
    
    if (c == AppleLogo) {
        DEFINE_STATIC_LOCAL(AtomicString, helveticaStr, ("Helvetica Neue"));
        FontPlatformData* platformFont = getCachedFontPlatformData(description, helveticaStr);
        return platformFont;
    } else if (c == BigDot) {
        DEFINE_STATIC_LOCAL(AtomicString, lockClockStr, ("LockClock-Light"));
        FontPlatformData* platformFont = getCachedFontPlatformData(description, lockClockStr);
        return platformFont;
    }
    return NULL;
}

static inline FontTraitsMask toTraitsMask(GSFontTraitMask gsFontTraits)
{
    return static_cast<FontTraitsMask>(((gsFontTraits & GSItalicFontMask) ? FontStyleItalicMask : FontStyleNormalMask)
        | FontVariantNormalMask
        // FontWeight600 or higher is bold for GSFonts, so choose middle values for
        // bold (600-900) and non-bold (100-500)
        | ((gsFontTraits & GSBoldFontMask) ? FontWeight700Mask : FontWeight300Mask));
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    // FIXME: This should be implemented using CoreText too. <rdar://problem/13522787>
    RetainPtr<CFMutableArrayRef> fontNames(AdoptCF, GSFontCopyFontNamesForFamilyName(familyName.string().utf8().data()));
    if (!fontNames)
        return;

    CFIndex count = CFArrayGetCount(fontNames.get());
    if (!count)
        return;

    for (CFIndex i = 0; i < count; ++i) {
        CFStringRef fontName = static_cast<CFStringRef>(CFArrayGetValueAtIndex(fontNames.get(), i));
        const CFIndex bufferSize = CFStringGetLength(fontName) + 1;
        Vector<char, 256> fontNameBuffer(bufferSize);
        if (CFStringGetCString(fontName, fontNameBuffer.data(), bufferSize, kCFStringEncodingUTF8))
            traitsMasks.append(toTraitsMask(GSFontGetTraitsForName(fontNameBuffer.data())));
    }
}

float FontCache::weightOfCTFont(CTFontRef font)
{
    float result = 0.0;
    RetainPtr<CFDictionaryRef> traits(AdoptCF, CTFontCopyTraits(font));
    if (!traits)
        return result;
    
    CFNumberRef resultRef = (CFNumberRef)CFDictionaryGetValue(traits.get(), kCTFontWeightTrait);
    if (resultRef)
        CFNumberGetValue(resultRef, kCFNumberFloatType, &result);

    return result;
}

// FIXME: Remove this function as soon as we deprecate the -webkit-system-... values.
// <rdar://problem/13969378>
static CTFontRef createCTFontForUsage(const String& familyName, GSFontTraitMask traits)
{
    if (familyName.isNull())
        return 0;

    CTFontSymbolicTraits symbolicTraits = 0;
    if (traits & GSBoldFontMask)
        symbolicTraits |= kCTFontBoldTrait;
    if (traits & GSItalicFontMask)
        symbolicTraits |= kCTFontTraitItalic;
    RetainPtr<CFStringRef> familyNameStr = familyName.createCFString();
    RetainPtr<CTFontDescriptorRef> fontDescriptor(AdoptCF, CTFontDescriptorCreateForUsage(familyNameStr.get(), RenderThemeIOS::contentSizeCategory(), 0));
    if (symbolicTraits)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), symbolicTraits, symbolicTraits));

    return CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0.0, NULL);
}

static CTFontRef createCTFontWithTextStyle(const String& familyName, GSFontTraitMask traits, CGFloat size)
{
    if (familyName.isNull())
        return 0;

    CTFontSymbolicTraits symbolicTraits = 0;
    if (traits & GSBoldFontMask)
        symbolicTraits |= kCTFontBoldTrait;
    if (traits & GSItalicFontMask)
        symbolicTraits |= kCTFontTraitItalic;
    RetainPtr<CFStringRef> familyNameStr = familyName.createCFString();
    RetainPtr<CTFontDescriptorRef> fontDescriptor(AdoptCF, CTFontDescriptorCreateWithTextStyle(familyNameStr.get(), RenderThemeIOS::contentSizeCategory(), 0));
    if (symbolicTraits)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), symbolicTraits, symbolicTraits));

    return CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, NULL);
}

static CTFontRef createCTFontWithFamilyNameAndWeight(const String& familyName, GSFontTraitMask traits, float size, uint16_t weight)
{
    if (familyName.isNull())
        return 0;

    DEFINE_STATIC_LOCAL(AtomicString, systemUIFontWithWebKitPrefix, ("-webkit-system-font", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, systemUIFontWithApplePrefix, ("-apple-system-font", AtomicString::ConstructFromLiteral));
    if (equalIgnoringCase(familyName, systemUIFontWithWebKitPrefix) || equalIgnoringCase(familyName, systemUIFontWithApplePrefix)) {
        CTFontUIFontType fontType = kCTFontUIFontSystem;
        if (weight > 300) {
            // The comment below has been copied from CoreText/UIFoundation. However, in WebKit we synthesize the oblique,
            // so we should investigate the result <rdar://problem/14449340>:
            // We don't do bold-italic for system fonts. If you ask for it, we'll assume that you're just kidding and that you really want bold. This is a feature.
            if (traits & GSBoldFontMask)
                fontType = kCTFontUIFontEmphasizedSystem;
            else if (traits & GSItalicFontMask)
                fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemItalic);
        } else if (weight > 250)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemLight);
        else if (weight > 150)
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemThin);
        else
            fontType = static_cast<CTFontUIFontType>(kCTFontUIFontSystemUltraLight);
        RetainPtr<CTFontDescriptorRef> fontDescriptor(AdoptCF, CTFontDescriptorCreateForUIType(fontType, size, NULL));
        return CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, NULL);
    }

    RetainPtr<CFStringRef> familyNameStr = familyName.createCFString();
    CTFontSymbolicTraits requestedTraits = (CTFontSymbolicTraits)(traits & GSDefinedStandardTraitsFontMask);
    return CTFontCreateForCSS(familyNameStr.get(), weight, requestedTraits, size);
}

static uint16_t toCTFontWeight(FontWeight fontWeight)
{
    switch (fontWeight) {
    case FontWeight100:
        return 100;
    case FontWeight200:
        return 200;
    case FontWeight300:
        return 300;
    case FontWeight400:
        return 400;
    case FontWeight500:
        return 500;
    case FontWeight600:
        return 600;
    case FontWeight700:
        return 700;
    case FontWeight800:
        return 800;
    case FontWeight900:
        return 900;
    default:
        return 400;
    }
}

PassOwnPtr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    // Special case for "Courier" font. We used to have only an oblique variant on iOS, so prior to
    // iOS 6.0, we disallowed its use here. We'll fall back on "Courier New". <rdar://problem/5116477&10850227>
    DEFINE_STATIC_LOCAL(AtomicString, courier, ("Courier"));
    static bool shouldDisallowCourier = !iosExecutableWasLinkedOnOrAfterVersion(wkIOSSystemVersion_6_0);
    if (shouldDisallowCourier && equalIgnoringCase(family, courier))
        return nullptr;

    GSFontTraitMask traits = 0;
    if (fontDescription.italic())
        traits |= GSItalicFontMask;
    if (isGSFontWeightBold(fontDescription.weight()))
        traits |= GSBoldFontMask;
    float size = fontDescription.computedPixelSize();

    RetainPtr<GSFontRef> gsFont;
    if (family.startsWith("UICTFontUsage")) {
        gsFont = adoptCF(createCTFontForUsage(family, traits));
        if (!gsFont)
            return nullptr;
        return adoptPtr(new FontPlatformData(gsFont.get(), size, fontDescription.usePrinterFont(), false, false, fontDescription.orientation(), fontDescription.widthVariant()));
    } else if (family.startsWith("UICTFontTextStyle")) {
        gsFont = adoptCF(createCTFontWithTextStyle(family, traits, size));
        if (!gsFont)
            return nullptr;
        return adoptPtr(new FontPlatformData(gsFont.get(), size, fontDescription.usePrinterFont(), false, false, fontDescription.orientation(), fontDescription.widthVariant()));
    }
    
    gsFont = adoptCF(createCTFontWithFamilyNameAndWeight(family, traits, size, toCTFontWeight(fontDescription.weight())));
    if (!gsFont)
        return nullptr;

    GSFontTraitMask actualTraits = 0;
    if (isGSFontWeightBold(fontDescription.weight()) || fontDescription.italic())
        actualTraits = GSFontGetTraits(gsFont.get());

    bool isAppleColorEmoji = CTFontIsAppleColorEmoji(gsFont.get());

    bool syntheticBold = (traits & GSBoldFontMask) && !(actualTraits & GSBoldFontMask) && !isAppleColorEmoji;
    bool syntheticOblique = (traits & GSItalicFontMask) && !(actualTraits & GSItalicFontMask) && !isAppleColorEmoji;

    FontPlatformData* result = new FontPlatformData(gsFont.get(), size, fontDescription.usePrinterFont(), syntheticBold, syntheticOblique, fontDescription.orientation(), fontDescription.widthVariant());
    if (isAppleColorEmoji)
        result->m_isEmoji = true;
    return adoptPtr(result);
}

} // namespace WebCore
