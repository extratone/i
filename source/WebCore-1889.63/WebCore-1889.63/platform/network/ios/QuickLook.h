//
//  QuickLook.h
//  WebCore
//
//  Copyright 2009 Apple Inc. All rights reserved.
//

#ifndef QuickLook_h
#define QuickLook_h

#if USE(QUICK_LOOK)

#import "ResourceRequest.h"
#import <objc/objc-runtime.h>
#import <wtf/PassOwnPtr.h>

#ifdef __OBJC__
@class NSArray;
@class NSData;
@class NSDictionary;
@class NSFileHandle;
@class NSSet;
@class NSString;
@class NSURL;
@class NSURLConnection;
@class NSURLResponse;
#else
class NSArray;
class NSData;
class NSDictionary;
class NSFileHandle;
class NSSet;
class NSString;
class NSURL;
class NSURLConnection;
class NSURLResponse;
#endif

#if USE(CFNETWORK)
typedef struct _CFURLResponse* CFURLResponseRef;
typedef struct _CFURLConnection* CFURLConnectionRef;
#endif

namespace WebCore {

    class KURL;
    class ResourceHandle;
    class SubstituteData;

    Class QLPreviewConverterClass();
    NSString *QLTypeCopyBestMimeTypeForFileNameAndMimeType(NSString *fileName, NSString *mimeType);
    NSString *QLTypeCopyBestMimeTypeForURLAndMimeType(NSURL *, NSString *mimeType);

    NSSet *QLPreviewGetSupportedMIMETypesSet();
    
    // Used for setting the permissions on the saved QL content
    NSDictionary *QLFileAttributes();
    NSDictionary *QLDirectoryAttributes();

    void addQLPreviewConverterWithFileForURL(NSURL *, id converter, NSString* fileName);
    NSString *qlPreviewConverterFileNameForURL(NSURL *);
    NSString *qlPreviewConverterUTIForURL(NSURL *);
    void removeQLPreviewConverterForURL(NSURL *);

    PassOwnPtr<ResourceRequest> registerQLPreviewConverterIfNeeded(NSURL *, NSString *mimeType, NSData *);

    const KURL safeQLURLForDocumentURLAndResourceURL(const KURL& documentURL, const String& resourceURL);

    const char* QLPreviewProtocol();


    class QuickLookHandle {
        WTF_MAKE_NONCOPYABLE(QuickLookHandle);
    public:
        static PassOwnPtr<QuickLookHandle> create(ResourceHandle*, NSURLConnection *, NSURLResponse *, id delegate);
#if USE(CFNETWORK)
        static PassOwnPtr<QuickLookHandle> create(ResourceHandle*, CFURLConnectionRef, CFURLResponseRef);
#endif
        ~QuickLookHandle();

        bool didReceiveDataArray(CFArrayRef);
        bool didReceiveData(CFDataRef);
        bool didFinishLoading();
        void didFail();

        NSURLResponse *nsResponse();
#if USE(CFNETWORK)
        CFURLResponseRef cfResponse();
#endif

    private:
        QuickLookHandle(ResourceHandle*, NSURLConnection *, NSURLResponse *, id delegate);
#if USE(CFNETWORK)
        QuickLookHandle(ResourceHandle*, CFURLResponseRef);
#endif

        ResourceHandle* m_handle;
        RetainPtr<id> m_converter;
        RetainPtr<id> m_delegate;
        bool m_finishedLoadingDataIntoConverter;
        RetainPtr<NSFileHandle *> m_quicklookFileHandle;
        NSURLResponse *m_nsResponse;
    };


} // namespace WebCore

#endif // USE(QUICK_LOOK)

#endif // QuickLook_h
