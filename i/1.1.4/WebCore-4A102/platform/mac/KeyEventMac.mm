/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "PlatformKeyboardEvent.h"

#import "Logging.h"

#import "GraphicsServices/GraphicsServices.h"
#import "WKWindow.h"


namespace WebCore {

static String keyIdentifierForKeyEvent(GSEventRef event)
{
    CFStringRef s = GSEventCopyCharactersIgnoringModifiers (event);
    if (CFStringGetLength(s) != 1) {
        WKError("received an unexpected number of characters in key event: %u", CFStringGetLength(s));
        return "Unidentified";
    }
    unichar c = CFStringGetCharacterAtIndex (s, 0);
    CFRelease(s);
    switch (c) {
        // Each identifier listed in the DOM spec is listed here.
        // Many are simply commented out since they do not appear on standard Macintosh keyboards
        // or are on a key that doesn't have a corresponding character.

        // "Accept"
        // "AllCandidates"

        // "Alt"
        case NSMenuFunctionKey:
            return "Alt";

        // "Apps"
        // "BrowserBack"
        // "BrowserForward"
        // "BrowserHome"
        // "BrowserRefresh"
        // "BrowserSearch"
        // "BrowserStop"
        // "CapsLock"

        // "Clear"
        case NSClearLineFunctionKey:
            return "Clear";

        // "CodeInput"
        // "Compose"
        // "Control"
        // "Crsel"
        // "Convert"
        // "Copy"
        // "Cut"

        // "Down"
        case NSDownArrowFunctionKey:
            return "Down";
        // "End"
        case NSEndFunctionKey:
            return "End";
        // "Enter"
        case 0x3: case 0xA: case 0xD: // Macintosh calls the one on the main keyboard Return, but Windows calls it Enter, so we'll do the same for the DOM
            return "Enter";

        // "EraseEof"

        // "Execute"
        case NSExecuteFunctionKey:
            return "Execute";

        // "Exsel"

        // "F1"
        case NSF1FunctionKey:
            return "F1";
        // "F2"
        case NSF2FunctionKey:
            return "F2";
        // "F3"
        case NSF3FunctionKey:
            return "F3";
        // "F4"
        case NSF4FunctionKey:
            return "F4";
        // "F5"
        case NSF5FunctionKey:
            return "F5";
        // "F6"
        case NSF6FunctionKey:
            return "F6";
        // "F7"
        case NSF7FunctionKey:
            return "F7";
        // "F8"
        case NSF8FunctionKey:
            return "F8";
        // "F9"
        case NSF9FunctionKey:
            return "F9";
        // "F10"
        case NSF10FunctionKey:
            return "F10";
        // "F11"
        case NSF11FunctionKey:
            return "F11";
        // "F12"
        case NSF12FunctionKey:
            return "F12";
        // "F13"
        case NSF13FunctionKey:
            return "F13";
        // "F14"
        case NSF14FunctionKey:
            return "F14";
        // "F15"
        case NSF15FunctionKey:
            return "F15";
        // "F16"
        case NSF16FunctionKey:
            return "F16";
        // "F17"
        case NSF17FunctionKey:
            return "F17";
        // "F18"
        case NSF18FunctionKey:
            return "F18";
        // "F19"
        case NSF19FunctionKey:
            return "F19";
        // "F20"
        case NSF20FunctionKey:
            return "F20";
        // "F21"
        case NSF21FunctionKey:
            return "F21";
        // "F22"
        case NSF22FunctionKey:
            return "F22";
        // "F23"
        case NSF23FunctionKey:
            return "F23";
        // "F24"
        case NSF24FunctionKey:
            return "F24";

        // "FinalMode"

        // "Find"
        case NSFindFunctionKey:
            return "Find";

        // "FullWidth"
        // "HalfWidth"
        // "HangulMode"
        // "HanjaMode"

        // "Help"
        case NSHelpFunctionKey:
            return "Help";

        // "Hiragana"

        // "Home"
        case NSHomeFunctionKey:
            return "Home";
        // "Insert"
        case NSInsertFunctionKey:
            return "Insert";

        // "JapaneseHiragana"
        // "JapaneseKatakana"
        // "JapaneseRomaji"
        // "JunjaMode"
        // "KanaMode"
        // "KanjiMode"
        // "Katakana"
        // "LaunchApplication1"
        // "LaunchApplication2"
        // "LaunchMail"

        // "Left"
        case NSLeftArrowFunctionKey:
            return "Left";

        // "Meta"
        // "MediaNextTrack"
        // "MediaPlayPause"
        // "MediaPreviousTrack"
        // "MediaStop"

        // "ModeChange"
        case NSModeSwitchFunctionKey:
            return "ModeChange";

        // "Nonconvert"
        // "NumLock"

        // "PageDown"
        case NSPageDownFunctionKey:
            return "PageDown";
        // "PageUp"
        case NSPageUpFunctionKey:
            return "PageUp";

        // "Paste"

        // "Pause"
        case NSPauseFunctionKey:
            return "Pause";

        // "Play"
        // "PreviousCandidate"

        // "PrintScreen"
        case NSPrintScreenFunctionKey:
            return "PrintScreen";

        // "Process"
        // "Props"

        // "Right"
        case NSRightArrowFunctionKey:
            return "Right";

        // "RomanCharacters"

        // "Scroll"
        case NSScrollLockFunctionKey:
            return "Scroll";
        // "Select"
        case NSSelectFunctionKey:
            return "Select";

        // "SelectMedia"
        // "Shift"

        // "Stop"
        case NSStopFunctionKey:
            return "Stop";
        // "Up"
        case NSUpArrowFunctionKey:
            return "Up";
        // "Undo"
        case NSUndoFunctionKey:
            return "Undo";

        // "VolumeDown"
        // "VolumeMute"
        // "VolumeUp"
        // "Win"
        // "Zoom"

        // More function keys, not in the key identifier specification.
        case NSF25FunctionKey:
            return "F25";
        case NSF26FunctionKey:
            return "F26";
        case NSF27FunctionKey:
            return "F27";
        case NSF28FunctionKey:
            return "F28";
        case NSF29FunctionKey:
            return "F29";
        case NSF30FunctionKey:
            return "F30";
        case NSF31FunctionKey:
            return "F31";
        case NSF32FunctionKey:
            return "F32";
        case NSF33FunctionKey:
            return "F33";
        case NSF34FunctionKey:
            return "F34";
        case NSF35FunctionKey:
            return "F35";

        // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
        case 0x7F:
            return "U+000008";
        // Standard says that DEL becomes U+00007F.
        case NSDeleteFunctionKey:
            return "U+00007F";

        case NSBeginFunctionKey:
        case NSBreakFunctionKey:
        case NSClearDisplayFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSDeleteLineFunctionKey:
        case NSInsertCharFunctionKey:
        case NSInsertLineFunctionKey:
        case NSNextFunctionKey:
        case NSPrevFunctionKey:
        case NSPrintFunctionKey:
        case NSRedoFunctionKey:
        case NSResetFunctionKey:
        case NSSysReqFunctionKey:
        case NSSystemFunctionKey:
        case NSUserFunctionKey:
            // FIXME: We should use something other than the vendor-area Unicode values for the above keys.
            // For now, just fall through to the default.
        default:
            return String::sprintf("U+%06X", toupper(c));
    }
}

static bool isKeypadEvent(GSEventRef event)
{
    // Check that this is the type of event that has a keyCode.
    switch (GSEventGetType (event)) {
	case kGSEventKeyDown:
	case kGSEventKeyUp:
	case kGSEventFlagsChanged:
            break;
        default:
            return false;
    }

     return false;
}



static inline NSString *eventCopyCharacters(GSEventRef event)
{
    return [(NSString *)GSEventCopyCharacters(event) autorelease]; 
}

static inline NSString *eventCopyCharactersIgnoringModifiers(GSEventRef event)
{
    return [(NSString *)GSEventCopyCharactersIgnoringModifiers(event) autorelease]; 
}

PlatformKeyboardEvent::PlatformKeyboardEvent(GSEventRef event, bool forceAutoRepeat)
    : m_text(eventCopyCharacters(event)),
      m_unmodifiedText(eventCopyCharactersIgnoringModifiers(event)),
      m_keyIdentifier(keyIdentifierForKeyEvent(event)),
      m_isKeyUp(GSEventGetType(event) == kGSEventKeyUp),
      m_autoRepeat (forceAutoRepeat || GSEventIsKeyRepeating (event)),
      m_WindowsKeyCode(GSEventGetKeyCode(event)),
      m_isKeypad(isKeypadEvent(event)),
      m_shiftKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskShift),
      m_ctrlKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskControl),
      m_altKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskAlternate),
      m_metaKey(GSEventGetModifierFlags(event) & kGSEventFlagMaskCommand)
{
    // Turn 0x7F into 8, because backspace needs to always be 8.
    if (m_text == "\x7F")
        m_text = "\x8";
    if (m_unmodifiedText == "\x7F")
        m_unmodifiedText = "\x8";
    // Always use 9 for tab -- we don't want to use AppKit's different character for shift-tab.
    if (m_WindowsKeyCode == 9) {
        m_text = "\x9";
        m_unmodifiedText = "\x9";
    }
}

}
