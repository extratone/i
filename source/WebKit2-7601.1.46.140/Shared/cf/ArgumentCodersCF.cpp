/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ArgumentCodersCF.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "DataReference.h"
#include <WebCore/CFURLExtras.h>
#include <wtf/Vector.h>

#if USE(FOUNDATION)
#import <Foundation/Foundation.h>
#endif

#if defined(__has_include) && __has_include(<Security/SecIdentityPriv.h>)
#include <Security/SecIdentityPriv.h>
#endif

extern "C" SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator, SecCertificateRef certificate, SecKeyRef privateKey);

#if PLATFORM(IOS)
#if defined(__has_include) && __has_include(<Security/SecKeyPriv.h>)
#include <Security/SecKeyPriv.h>
#endif

extern "C" OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData);
#endif

#if HAVE(SEC_ACCESS_CONTROL)
#if defined(__has_include) && __has_include(<Security/SecAccessControlPriv.h>)
#include <Security/SecAccessControlPriv.h>
#endif

extern "C" SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error);
extern "C" CFDataRef SecAccessControlCopyData(SecAccessControlRef access_control);
#endif

using namespace WebCore;

namespace IPC {

CFTypeRef tokenNullTypeRef()
{
    static CFStringRef tokenNullType = CFSTR("WKNull");
    return tokenNullType;
}

enum CFType {
    CFArray,
    CFBoolean,
    CFData,
    CFDate,
    CFDictionary,
    CFNull,
    CFNumber,
    CFString,
    CFURL,
    SecCertificate,
    SecIdentity,
#if HAVE(SEC_KEYCHAIN)
    SecKeychainItem,
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    SecAccessControl,
#endif
    Null,
    Unknown,
};

static CFType typeFromCFTypeRef(CFTypeRef type)
{
    ASSERT(type);

    if (type == tokenNullTypeRef())
        return Null;

    CFTypeID typeID = CFGetTypeID(type);
    if (typeID == CFArrayGetTypeID())
        return CFArray;
    if (typeID == CFBooleanGetTypeID())
        return CFBoolean;
    if (typeID == CFDataGetTypeID())
        return CFData;
    if (typeID == CFDateGetTypeID())
        return CFDate;
    if (typeID == CFDictionaryGetTypeID())
        return CFDictionary;
    if (typeID == CFNullGetTypeID())
        return CFNull;
    if (typeID == CFNumberGetTypeID())
        return CFNumber;
    if (typeID == CFStringGetTypeID())
        return CFString;
    if (typeID == CFURLGetTypeID())
        return CFURL;
    if (typeID == SecCertificateGetTypeID())
        return SecCertificate;
    if (typeID == SecIdentityGetTypeID())
        return SecIdentity;
#if HAVE(SEC_KEYCHAIN)
    if (typeID == SecKeychainItemGetTypeID())
        return SecKeychainItem;
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    if (typeID == SecAccessControlGetTypeID())
        return SecAccessControl;
#endif

    ASSERT_NOT_REACHED();
    return Unknown;
}

void encode(ArgumentEncoder& encoder, CFTypeRef typeRef)
{
    CFType type = typeFromCFTypeRef(typeRef);
    encoder.encodeEnum(type);

    switch (type) {
    case CFArray:
        encode(encoder, static_cast<CFArrayRef>(typeRef));
        return;
    case CFBoolean:
        encode(encoder, static_cast<CFBooleanRef>(typeRef));
        return;
    case CFData:
        encode(encoder, static_cast<CFDataRef>(typeRef));
        return;
    case CFDate:
        encode(encoder, static_cast<CFDateRef>(typeRef));
        return;
    case CFDictionary:
        encode(encoder, static_cast<CFDictionaryRef>(typeRef));
        return;
    case CFNull:
        return;
    case CFNumber:
        encode(encoder, static_cast<CFNumberRef>(typeRef));
        return;
    case CFString:
        encode(encoder, static_cast<CFStringRef>(typeRef));
        return;
    case CFURL:
        encode(encoder, static_cast<CFURLRef>(typeRef));
        return;
    case SecCertificate:
        encode(encoder, (SecCertificateRef)typeRef);
        return;
    case SecIdentity:
        encode(encoder, (SecIdentityRef)(typeRef));
        return;
#if HAVE(SEC_KEYCHAIN)
    case SecKeychainItem:
        encode(encoder, (SecKeychainItemRef)typeRef);
        return;
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case SecAccessControl:
        encode(encoder, (SecAccessControlRef)typeRef);
        return;
#endif
    case Null:
        return;
    case Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFTypeRef>& result)
{
    CFType type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case CFArray: {
        RetainPtr<CFArrayRef> array;
        if (!decode(decoder, array))
            return false;
        result = adoptCF(array.leakRef());
        return true;
    }
    case CFBoolean: {
        RetainPtr<CFBooleanRef> boolean;
        if (!decode(decoder, boolean))
            return false;
        result = adoptCF(boolean.leakRef());
        return true;
    }
    case CFData: {
        RetainPtr<CFDataRef> data;
        if (!decode(decoder, data))
            return false;
        result = adoptCF(data.leakRef());
        return true;
    }
    case CFDate: {
        RetainPtr<CFDateRef> date;
        if (!decode(decoder, date))
            return false;
        result = adoptCF(date.leakRef());
        return true;
    }
    case CFDictionary: {
        RetainPtr<CFDictionaryRef> dictionary;
        if (!decode(decoder, dictionary))
            return false;
        result = adoptCF(dictionary.leakRef());
        return true;
    }
    case CFNull:
        result = adoptCF(kCFNull);
        return true;
    case CFNumber: {
        RetainPtr<CFNumberRef> number;
        if (!decode(decoder, number))
            return false;
        result = adoptCF(number.leakRef());
        return true;
    }
    case CFString: {
        RetainPtr<CFStringRef> string;
        if (!decode(decoder, string))
            return false;
        result = adoptCF(string.leakRef());
        return true;
    }
    case CFURL: {
        RetainPtr<CFURLRef> url;
        if (!decode(decoder, url))
            return false;
        result = adoptCF(url.leakRef());
        return true;
    }
    case SecCertificate: {
        RetainPtr<SecCertificateRef> certificate;
        if (!decode(decoder, certificate))
            return false;
        result = adoptCF(certificate.leakRef());
        return true;
    }
    case SecIdentity: {
        RetainPtr<SecIdentityRef> identity;
        if (!decode(decoder, identity))
            return false;
        result = adoptCF(identity.leakRef());
        return true;
    }
#if HAVE(SEC_KEYCHAIN)
    case SecKeychainItem: {
        RetainPtr<SecKeychainItemRef> keychainItem;
        if (!decode(decoder, keychainItem))
            return false;
        result = adoptCF(keychainItem.leakRef());
        return true;
    }
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case SecAccessControl: {
        RetainPtr<SecAccessControlRef> accessControl;
        if (!decode(decoder, accessControl))
            return false;
        result = adoptCF(accessControl.leakRef());
        return true;
    }
#endif
    case Null:
        result = tokenNullTypeRef();
        return true;
    case Unknown:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

void encode(ArgumentEncoder& encoder, CFArrayRef array)
{
    CFIndex size = CFArrayGetCount(array);
    Vector<CFTypeRef, 32> values(size);

    CFArrayGetValues(array, CFRangeMake(0, size), values.data());

    encoder << static_cast<uint64_t>(size);

    for (CFIndex i = 0; i < size; ++i) {
        ASSERT(values[i]);

        encode(encoder, values[i]);
    }
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFArrayRef>& result)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    RetainPtr<CFMutableArrayRef> array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < size; ++i) {
        RetainPtr<CFTypeRef> element;
        if (!decode(decoder, element))
            return false;

        CFArrayAppendValue(array.get(), element.get());
    }

    result = adoptCF(array.leakRef());
    return true;
}

void encode(ArgumentEncoder& encoder, CFBooleanRef boolean)
{
    encoder << static_cast<bool>(CFBooleanGetValue(boolean));
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFBooleanRef>& result)
{
    bool boolean;
    if (!decoder.decode(boolean))
        return false;

    result = adoptCF(boolean ? kCFBooleanTrue : kCFBooleanFalse);
    return true;
}

void encode(ArgumentEncoder& encoder, CFDataRef data)
{
    CFIndex length = CFDataGetLength(data);
    const UInt8* bytePtr = CFDataGetBytePtr(data);

    encoder << IPC::DataReference(bytePtr, length);
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFDataRef>& result)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    result = adoptCF(CFDataCreate(0, dataReference.data(), dataReference.size()));
    return true;
}

void encode(ArgumentEncoder& encoder, CFDateRef date)
{
    encoder << static_cast<double>(CFDateGetAbsoluteTime(date));
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFDateRef>& result)
{
    double absoluteTime;
    if (!decoder.decode(absoluteTime))
        return false;

    result = adoptCF(CFDateCreate(0, absoluteTime));
    return true;
}

void encode(ArgumentEncoder& encoder, CFDictionaryRef dictionary)
{
    CFIndex size = CFDictionaryGetCount(dictionary);
    Vector<CFTypeRef, 32> keys(size);
    Vector<CFTypeRef, 32> values(size);
    
    CFDictionaryGetKeysAndValues(dictionary, keys.data(), values.data());

    encoder << static_cast<uint64_t>(size);

    for (CFIndex i = 0; i < size; ++i) {
        ASSERT(keys[i]);
        ASSERT(CFGetTypeID(keys[i]) == CFStringGetTypeID());
        ASSERT(values[i]);

        // Ignore values we don't recognize.
        if (typeFromCFTypeRef(values[i]) == Unknown)
            continue;

        encode(encoder, static_cast<CFStringRef>(keys[i]));
        encode(encoder, values[i]);
    }
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFDictionaryRef>& result)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (uint64_t i = 0; i < size; ++i) {
        // Try to decode the key name.
        RetainPtr<CFStringRef> key;
        if (!decode(decoder, key))
            return false;

        RetainPtr<CFTypeRef> value;
        if (!decode(decoder, value))
            return false;

        CFDictionarySetValue(dictionary.get(), key.get(), value.get());
    }

    result = adoptCF(dictionary.leakRef());
    return true;
}

void encode(ArgumentEncoder& encoder, CFNumberRef number)
{
    CFNumberType numberType = CFNumberGetType(number);

    Vector<uint8_t> buffer(CFNumberGetByteSize(number));
    bool result = CFNumberGetValue(number, numberType, buffer.data());
    ASSERT_UNUSED(result, result);

    encoder.encodeEnum(numberType);
    encoder << IPC::DataReference(buffer);
}

static size_t sizeForNumberType(CFNumberType numberType)
{
    switch (numberType) {
    case kCFNumberSInt8Type:
        return sizeof(SInt8);
    case kCFNumberSInt16Type:
        return sizeof(SInt16);
    case kCFNumberSInt32Type:
        return sizeof(SInt32);
    case kCFNumberSInt64Type:
        return sizeof(SInt64);
    case kCFNumberFloat32Type:
        return sizeof(Float32);
    case kCFNumberFloat64Type:
        return sizeof(Float64);
    case kCFNumberCharType:
        return sizeof(char);
    case kCFNumberShortType:
        return sizeof(short);
    case kCFNumberIntType:
        return sizeof(int);
    case kCFNumberLongType:
        return sizeof(long);
    case kCFNumberLongLongType:
        return sizeof(long long);
    case kCFNumberFloatType:
        return sizeof(float);
    case kCFNumberDoubleType:
        return sizeof(double);
    case kCFNumberCFIndexType:
        return sizeof(CFIndex);
    case kCFNumberNSIntegerType:
#ifdef __LP64__
        return sizeof(long);
#else
        return sizeof(int);
#endif
    case kCFNumberCGFloatType:
#ifdef __LP64__
        return sizeof(double);
#else
        return sizeof(float);
#endif
    }

    return 0;
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFNumberRef>& result)
{
    CFNumberType numberType;
    if (!decoder.decodeEnum(numberType))
        return false;

    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    size_t neededBufferSize = sizeForNumberType(numberType);
    if (!neededBufferSize || dataReference.size() != neededBufferSize)
        return false;

    ASSERT(dataReference.data());
    CFNumberRef number = CFNumberCreate(0, numberType, dataReference.data());
    result = adoptCF(number);

    return true;
}

void encode(ArgumentEncoder& encoder, CFStringRef string)
{
    CFIndex length = CFStringGetLength(string);
    CFStringEncoding encoding = CFStringGetFastestEncoding(string);

    CFRange range = CFRangeMake(0, length);
    CFIndex bufferLength = 0;

    CFIndex numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, 0, 0, &bufferLength);
    ASSERT(numConvertedBytes == length);

    Vector<UInt8, 128> buffer(bufferLength);
    numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, buffer.data(), buffer.size(), &bufferLength);
    ASSERT(numConvertedBytes == length);

    encoder.encodeEnum(encoding);
    encoder << IPC::DataReference(buffer);
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFStringRef>& result)
{
    CFStringEncoding encoding;
    if (!decoder.decodeEnum(encoding))
        return false;

    if (!CFStringIsEncodingAvailable(encoding))
        return false;
    
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    CFStringRef string = CFStringCreateWithBytes(0, dataReference.data(), dataReference.size(), encoding, false);
    if (!string)
        return false;

    result = adoptCF(string);
    return true;
}

void encode(ArgumentEncoder& encoder, CFURLRef url)
{
    CFURLRef baseURL = CFURLGetBaseURL(url);
    encoder << static_cast<bool>(baseURL);
    if (baseURL)
        encode(encoder, baseURL);

    URLCharBuffer urlBytes;
    getURLBytes(url, urlBytes);
    IPC::DataReference dataReference(reinterpret_cast<const uint8_t*>(urlBytes.data()), urlBytes.size());
    encoder << dataReference;
}

bool decode(ArgumentDecoder& decoder, RetainPtr<CFURLRef>& result)
{
    RetainPtr<CFURLRef> baseURL;
    bool hasBaseURL;
    if (!decoder.decode(hasBaseURL))
        return false;
    if (hasBaseURL) {
        if (!decode(decoder, baseURL))
            return false;
    }

    IPC::DataReference urlBytes;
    if (!decoder.decode(urlBytes))
        return false;

#if USE(FOUNDATION)
    // FIXME: Move this to ArgumentCodersCFMac.mm and change this file back to be C++
    // instead of Objective-C++.
    if (urlBytes.isEmpty()) {
        // CFURL can't hold an empty URL, unlike NSURL.
        // FIXME: This discards base URL, which seems incorrect.
        result = reinterpret_cast<CFURLRef>([NSURL URLWithString:@""]);
        return true;
    }
#endif

    result = createCFURLFromBuffer(reinterpret_cast<const char*>(urlBytes.data()), urlBytes.size(), baseURL.get());
    return result;
}

void encode(ArgumentEncoder& encoder, SecCertificateRef certificate)
{
    RetainPtr<CFDataRef> data = adoptCF(SecCertificateCopyData(certificate));
    encode(encoder, data.get());
}

bool decode(ArgumentDecoder& decoder, RetainPtr<SecCertificateRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return false;

    result = adoptCF(SecCertificateCreateWithData(0, data.get()));
    return true;
}

#if PLATFORM(IOS)
static bool secKeyRefDecodingAllowed;

void setAllowsDecodingSecKeyRef(bool allowsDecodingSecKeyRef)
{
    secKeyRefDecodingAllowed = allowsDecodingSecKeyRef;
}

static CFDataRef copyPersistentRef(SecKeyRef key)
{
    // This function differs from SecItemCopyPersistentRef in that it specifies an access group.
    // This is necessary in case there are multiple copies of the key in the keychain, because we
    // need a reference to the one that the Networking process will be able to access.
    CFDataRef persistentRef = nullptr;
    SecItemCopyMatching((CFDictionaryRef)@{
        (id)kSecReturnPersistentRef: @YES,
        (id)kSecValueRef: (id)key,
        (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
        (id)kSecAttrAccessGroup: @"com.apple.identities",
    }, (CFTypeRef*)&persistentRef);

    return persistentRef;
}
#endif

void encode(ArgumentEncoder& encoder, SecIdentityRef identity)
{
    SecCertificateRef certificate = nullptr;
    SecIdentityCopyCertificate(identity, &certificate);
    encode(encoder, certificate);
    CFRelease(certificate);

    SecKeyRef key = nullptr;
    SecIdentityCopyPrivateKey(identity, &key);

    CFDataRef keyData = nullptr;
#if PLATFORM(IOS)
    keyData = copyPersistentRef(key);
#endif
#if PLATFORM(MAC)
    SecKeychainItemCreatePersistentReference((SecKeychainItemRef)key, &keyData);
#endif
    CFRelease(key);

    encoder << !!keyData;
    if (keyData) {
        encode(encoder, keyData);
        CFRelease(keyData);
    }
}

bool decode(ArgumentDecoder& decoder, RetainPtr<SecIdentityRef>& result)
{
    RetainPtr<SecCertificateRef> certificate;
    if (!decode(decoder, certificate))
        return false;

    bool hasKey;
    if (!decoder.decode(hasKey))
        return false;

    if (!hasKey)
        return true;

    RetainPtr<CFDataRef> keyData;
    if (!decode(decoder, keyData))
        return false;

    SecKeyRef key = nullptr;
#if PLATFORM(IOS)
    if (secKeyRefDecodingAllowed)
        SecKeyFindWithPersistentRef(keyData.get(), &key);
#endif
#if PLATFORM(MAC)
    SecKeychainItemCopyFromPersistentReference(keyData.get(), (SecKeychainItemRef*)&key);
#endif
    if (key) {
        result = adoptCF(SecIdentityCreate(kCFAllocatorDefault, certificate.get(), key));
        CFRelease(key);
    }

    return true;
}

#if HAVE(SEC_KEYCHAIN)
void encode(ArgumentEncoder& encoder, SecKeychainItemRef keychainItem)
{
    CFDataRef data;
    if (SecKeychainItemCreatePersistentReference(keychainItem, &data) == errSecSuccess) {
        encode(encoder, data);
        CFRelease(data);
    }
}

bool decode(ArgumentDecoder& decoder, RetainPtr<SecKeychainItemRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    SecKeychainItemRef item;
    if (SecKeychainItemCopyFromPersistentReference(data.get(), &item) != errSecSuccess || !item)
        return false;
    
    result = adoptCF(item);
    return true;
}
#endif

#if HAVE(SEC_ACCESS_CONTROL)
void encode(ArgumentEncoder& encoder, SecAccessControlRef accessControl)
{
    RetainPtr<CFDataRef> data = adoptCF(SecAccessControlCopyData(accessControl));
    if (data)
        encode(encoder, data.get());
}

bool decode(ArgumentDecoder& decoder, RetainPtr<SecAccessControlRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return false;

    result = adoptCF(SecAccessControlCreateFromData(kCFAllocatorDefault, data.get(), nullptr));
    if (!result)
        return false;

    return true;
}

#endif

} // namespace IPC
