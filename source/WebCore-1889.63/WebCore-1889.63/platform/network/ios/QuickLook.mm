//
//  QuickLook.m
//  WebCore
//
//  Copyright 2009 Apple Inc. All rights reserved.
//

#import "config.h"
#import "QuickLook.h"

#if USE(QUICK_LOOK)

#import "FileSystemIOS.h"
#import "KURL.h"
#import "Logging.h"
#import "ResourceHandle.h"
#import "RuntimeApplicationChecksIOS.h"
#import "SoftLinking.h"
#import "SubstituteData.h"
#import "WebCoreURLResponseIOS.h"
#import <Foundation/Foundation.h>
#import <Foundation/NSFileManager_NSURLExtras.h>
#import <QuickLook/QLPreviewConverter.h>
#import <QuickLook/QuickLookPrivate.h>
#import <wtf/PassOwnPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#if USE(CFNETWORK)

#import <CFNetwork/CFURLConnection.h>

@interface NSURLResponse (QuickLookDetails)
+(NSURLResponse *)_responseWithCFURLResponse:(CFURLResponseRef)response;
-(CFURLResponseRef)_CFURLResponse;
@end
#endif

SOFT_LINK_FRAMEWORK_OPTIONAL(QuickLook)

SOFT_LINK_CLASS(QuickLook, QLPreviewConverter)

SOFT_LINK_MAY_FAIL(QuickLook, QLPreviewGetSupportedMIMETypes, NSSet *, (), ())
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyBestMimeTypeForFileNameAndMimeType, NSString *, (NSString *fileName, NSString *mimeType), (fileName, mimeType))
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyBestMimeTypeForURLAndMimeType, NSString *, (NSURL *url, NSString *mimeType), (url, mimeType))
SOFT_LINK_MAY_FAIL(QuickLook, QLTypeCopyUTIForURLAndMimeType, NSString *, (NSURL *url, NSString *mimeType), (url, mimeType))

SOFT_LINK_CONSTANT_MAY_FAIL(QuickLook, QLPreviewScheme, NSString *)

namespace WebCore {
    NSString *QLTypeCopyUTIForURLAndMimeType(NSURL *url, NSString *mimeType);
}

using namespace WebCore;

Class WebCore::QLPreviewConverterClass()
{
#define QLPreviewConverter getQLPreviewConverterClass()
    return QLPreviewConverter;
#undef QLPreviewConverter
}

NSString *WebCore::QLTypeCopyBestMimeTypeForFileNameAndMimeType(NSString *fileName, NSString *mimeType)
{
    if (!canLoadQLTypeCopyBestMimeTypeForFileNameAndMimeType())
        return nil;

    return ::QLTypeCopyBestMimeTypeForFileNameAndMimeType(fileName, mimeType);
}

NSString *WebCore::QLTypeCopyBestMimeTypeForURLAndMimeType(NSURL *url, NSString *mimeType)
{
    if (!canLoadQLTypeCopyBestMimeTypeForURLAndMimeType())
        return nil;

    return ::QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType);
}

NSSet *WebCore::QLPreviewGetSupportedMIMETypesSet()
{
    if (!canLoadQLPreviewGetSupportedMIMETypes())
        return nil;

    DEFINE_STATIC_LOCAL(RetainPtr<NSSet>, set, (::QLPreviewGetSupportedMIMETypes()));
    return set.get();
}

NSString *WebCore::QLTypeCopyUTIForURLAndMimeType(NSURL *url, NSString *mimeType)
{
    if (!canLoadQLTypeCopyUTIForURLAndMimeType())
        return nil;

    return ::QLTypeCopyUTIForURLAndMimeType(url, mimeType);
}

NSDictionary *WebCore::QLFileAttributes()
{    
    // set file perms to owner read/write only
    NSNumber *filePOSIXPermissions = [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)];
    
    DEFINE_STATIC_LOCAL(RetainPtr<NSDictionary>, dictionary, ([[NSDictionary alloc] initWithObjectsAndKeys:
                                                    NSUserName(), NSFileOwnerAccountName,
                                                    filePOSIXPermissions, NSFilePosixPermissions,
                                                    NULL]));
    return dictionary.get();
}

NSDictionary *WebCore::QLDirectoryAttributes()
{
    // set file perms to owner read/write/execute only
    NSNumber *directoryPOSIXPermissions = [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)];

    DEFINE_STATIC_LOCAL(RetainPtr<NSDictionary>, dictionary, ([[NSDictionary alloc] initWithObjectsAndKeys:
                                                    NSUserName(), NSFileOwnerAccountName,
                                                    directoryPOSIXPermissions, NSFilePosixPermissions,
                                                    NULL
                                                    ]));
    return dictionary.get();
}

static Mutex& qlPreviewConverterDictionaryMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static NSMutableDictionary *QLPreviewConverterDictionary()
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSMutableDictionary>, dictionary, ([NSMutableDictionary dictionary]));
    return dictionary.get();
}

static NSMutableDictionary *QLContentDictionary()
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSMutableDictionary>, contentDictionary, ([NSMutableDictionary dictionary]));
    return contentDictionary.get();
}

void WebCore::addQLPreviewConverterWithFileForURL(NSURL *url, id converter, NSString *fileName)
{
    ASSERT(url);
    ASSERT(converter);
    MutexLocker lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() setObject:converter forKey:url];
    [QLContentDictionary() setObject:(fileName ? fileName : @"") forKey:url];
}

NSString *WebCore::qlPreviewConverterFileNameForURL(NSURL *url)
{
    return [QLContentDictionary() objectForKey:url];
}

NSString *WebCore::qlPreviewConverterUTIForURL(NSURL *url)
{
    id converter = nil;
    {
        MutexLocker lock(qlPreviewConverterDictionaryMutex());
        converter = [QLPreviewConverterDictionary() objectForKey:url];
    }
    if (!converter)
        return nil;
    return [converter previewUTI];
}

void WebCore::removeQLPreviewConverterForURL(NSURL *url)
{
    MutexLocker lock(qlPreviewConverterDictionaryMutex());
    [QLPreviewConverterDictionary() removeObjectForKey:url];
    
    // Delete the file when we remove the preview converter
    NSString *filename = qlPreviewConverterFileNameForURL(url);
    if ([filename length])
        [[NSFileManager defaultManager] _web_removeFileOnlyAtPath:filename];
    [QLContentDictionary() removeObjectForKey:url];
}

PassOwnPtr<ResourceRequest> WebCore::registerQLPreviewConverterIfNeeded(NSURL *url, NSString *mimeType, NSData *data)
{
    RetainPtr<NSString> updatedMIMEType = adoptNS(WebCore::QLTypeCopyBestMimeTypeForURLAndMimeType(url, mimeType));

    if ([WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:updatedMIMEType.get()]) {
        RetainPtr<NSString> uti = adoptNS(WebCore::QLTypeCopyUTIForURLAndMimeType(url, updatedMIMEType.get()));

        RetainPtr<id> converter = adoptNS([[QLPreviewConverterClass() alloc] initWithData:data name:nil uti:uti.get() options:nil]);
        NSURLRequest *request = [converter.get() previewRequest];

        // We use [request URL] here instead of url since it will be
        // the URL that the WebDataSource will see during -dealloc.
        addQLPreviewConverterWithFileForURL([request URL], converter.get(), nil);

        return adoptPtr(new ResourceRequest(request));
    }

    return nullptr;
}

const KURL WebCore::safeQLURLForDocumentURLAndResourceURL(const KURL& documentURL, const String& resourceURL)
{
    id converter = nil;
    NSURL *nsDocumentURL = documentURL;
    {
        MutexLocker lock(qlPreviewConverterDictionaryMutex());
        converter = [QLPreviewConverterDictionary() objectForKey:nsDocumentURL];
    }
    
    if (!converter)
        return KURL(ParsedURLString, resourceURL);
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL URLWithString:resourceURL]];
    NSURLRequest *safeRequest = [converter safeRequestForRequest:request];
    [request release];
    
    return [safeRequest URL];
}

static Vector<char> createQLPreviewProtocol()
{
    Vector<char> previewProtocol;
#define QLPreviewScheme getQLPreviewScheme()
    const char* qlPreviewScheme = [QLPreviewScheme UTF8String];
#undef QLPreviewScheme
    previewProtocol.append(qlPreviewScheme, strlen(qlPreviewScheme) + 1);
    return previewProtocol;
}

const char* WebCore::QLPreviewProtocol()
{
    if (!canLoadQLPreviewScheme())
        return "";

    DEFINE_STATIC_LOCAL(Vector<char>, previewProtocol, (createQLPreviewProtocol()));
    return previewProtocol.data();
}

#if USE(CFNETWORK)
/*
 * The way QuickLook works is we pass it an NSURLConnectionDelegate callback object at creation
 * time. Then we pass it all the data as we receive it. Once we've downloaded the full URL,
 * QuickLook turns around and send us, through this delegate, the HTML version of the file which we
 * pass on to WebCore. The flag m_finishedLoadingDataIntoConverter in QuickLookHandle decides
 * whether to pass the data to QuickLook or WebCore.
 *
 * This works fine when using NS APIs, but when using CFNetwork, we don't have a
 * NSURLConnectionDelegate. So we create WebQuickLookHandleAsDelegate as an intermediate delegate
 * object and pass it to QLPreviewConverter. The proxy delegate then forwards the messages on to the
 * CFNetwork code.
 */
@interface WebQuickLookHandleAsDelegate : NSObject <NSURLConnectionDelegate> {
    CFURLConnectionRef m_connection;
    ResourceHandle* m_handle;
}

- (id)initWithConnection:(CFURLConnectionRef)connection handle:(ResourceHandle*)handle;
- (void)clearHandle;
@end

@implementation WebQuickLookHandleAsDelegate
- (id)initWithConnection:(CFURLConnectionRef)connection handle:(ResourceHandle*)handle
{
    self = [super init];
    if (!self)
        return nil;
    m_connection = connection;
    m_handle = handle;
    return self;
}

- (void)connection:(NSURLConnection *)connection didReceiveDataArray:(NSArray *)dataArray
{
    UNUSED_PARAM(connection);
    if (!m_handle)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didReceiveDataArray()");
    ResourceHandle::connectionClientCallbacks()->didReceiveDataArray(m_connection, reinterpret_cast<CFArrayRef>(dataArray), m_handle);
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    UNUSED_PARAM(connection);
    if (!m_handle)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didReceiveData() - data length = %ld", (long)[data length]);

    // QuickLook code sends us a nil data at times. The check below is the same as the one in
    // ResourceHandleMac.cpp added for a different bug.
    if (![data length])
        return;
    ResourceHandle::connectionClientCallbacks()->didReceiveData(m_connection, reinterpret_cast<CFDataRef>(data), static_cast<int>(lengthReceived), m_handle);
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    UNUSED_PARAM(connection);
    if (!m_handle)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didFinishLoading()");
    ResourceHandle::connectionClientCallbacks()->didFinishLoading(m_connection, m_handle);
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    UNUSED_PARAM(connection);
    if (!m_handle)
        return;
    LOG(Network, "WebQuickLookHandleAsDelegate::didFail()");
    ResourceHandle::connectionClientCallbacks()->didFail(m_connection, reinterpret_cast<CFErrorRef>(error), m_handle);
}

- (void)clearHandle
{
    m_handle = 0;
}
@end
#endif

namespace WebCore {

static NSString *createTemporaryFileForQuickLook(NSString *fileName)
{
    NSString *downloadDirectory = createTemporaryDirectory(@"QuickLookContent");
    if (!downloadDirectory)
        return nil;

    NSString *contentPath = [downloadDirectory stringByAppendingPathComponent:fileName];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *uniqueContentPath = [fileManager _web_pathWithUniqueFilenameForPath:contentPath];

    BOOL success = [fileManager _web_createFileAtPathWithIntermediateDirectories:uniqueContentPath
                                                                        contents:nil
                                                                      attributes:QLFileAttributes()
                                                             directoryAttributes:QLDirectoryAttributes()];

    return success ? uniqueContentPath : nil;
}


QuickLookHandle::QuickLookHandle(ResourceHandle* handle, NSURLConnection *connection, NSURLResponse *nsResponse, id delegate)
    : m_handle(handle)
    , m_converter(AdoptNS, [[QLPreviewConverterClass() alloc] initWithConnection:connection delegate:delegate response:nsResponse options:nil])
    , m_delegate(AdoptNS, [delegate retain])
    , m_finishedLoadingDataIntoConverter(false)
    , m_quicklookFileHandle(0)
    , m_nsResponse([m_converter.get() previewResponse])
{
    NSURL *previewRequestURL = [[m_converter.get() previewRequest] URL];
    if (!applicationIsMobileSafari()) {
        // This keeps the QLPreviewConverter alive to serve any subresource requests.
        // It is removed by -[WebDataSource dealloc].
        addQLPreviewConverterWithFileForURL(previewRequestURL, m_converter.get(), nil);
        return;
    }

    // QL consumes the incoming data, we need to store it so that it
    // can be opened in the handling application.
    NSString *quicklookContentPath = createTemporaryFileForQuickLook([m_converter.get() previewFileName]);
    LOG(Network, "QuickLookHandle::QuickLookHandle() - quicklookContentPath: %s", [quicklookContentPath UTF8String]);

    if (quicklookContentPath) {
        m_quicklookFileHandle = adoptNS([[NSFileHandle fileHandleForWritingAtPath:quicklookContentPath] retain]);
        // We must use the generated URL from m_converter's NSURLRequest object
        // so that it matches the URL removed from -[WebDataSource dealloc].
        addQLPreviewConverterWithFileForURL(previewRequestURL, m_converter.get(), quicklookContentPath);
    }
}

PassOwnPtr<QuickLookHandle> QuickLookHandle::create(ResourceHandle* handle, NSURLConnection *connection, NSURLResponse *nsResponse, id delegate)
{
    if (handle->firstRequest().isMainResourceRequest() && [WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:[nsResponse MIMEType]])
        return adoptPtr(new QuickLookHandle(handle, connection, nsResponse, delegate));

    return nullptr;
}

#if USE(CFNETWORK)
PassOwnPtr<QuickLookHandle> QuickLookHandle::create(ResourceHandle* handle, CFURLConnectionRef connection, CFURLResponseRef cfResponse)
{
    if (handle->firstRequest().isMainResourceRequest() && [WebCore::QLPreviewGetSupportedMIMETypesSet() containsObject:(NSString *)CFURLResponseGetMIMEType(cfResponse)]) {
        NSURLResponse *nsResponse = [NSURLResponse _responseWithCFURLResponse:cfResponse];
        WebQuickLookHandleAsDelegate *delegate = [[[WebQuickLookHandleAsDelegate alloc] initWithConnection:connection handle:handle] autorelease];
        return adoptPtr(new QuickLookHandle(handle, nil, nsResponse, delegate));
    }

    return nullptr;
}

CFURLResponseRef QuickLookHandle::cfResponse()
{
    return [m_nsResponse _CFURLResponse];
}
#endif

NSURLResponse *QuickLookHandle::nsResponse()
{
    return m_nsResponse;
}

bool QuickLookHandle::didReceiveDataArray(CFArrayRef cfDataArray)
{
    NSArray * const dataArray = (NSArray *)cfDataArray;

    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didReceiveDataArray()");
    [m_converter.get() appendDataArray:dataArray];
    if (m_quicklookFileHandle) {
        for (NSData *data in dataArray)
            [m_quicklookFileHandle.get() writeData:data];
    }
    return true;
}

bool QuickLookHandle::didReceiveData(CFDataRef cfData)
{
    NSData * const data = (NSData *)cfData;

    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didReceiveData()");
    [m_converter.get() appendData:data];
    if (m_quicklookFileHandle)
        [m_quicklookFileHandle.get() writeData:data];
    return true;
}

bool QuickLookHandle::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "QuickLookHandle::didFinishLoading()");
    m_finishedLoadingDataIntoConverter = YES;
    [m_converter.get() finishedAppendingData];
    if (m_quicklookFileHandle)
        [m_quicklookFileHandle.get() closeFile];
    return true;
}

void QuickLookHandle::didFail()
{
    LOG(Network, "QuickLookHandle::didFail()");
    m_quicklookFileHandle = nullptr;
    // removeQLPreviewConverterForURL deletes the temporary file created.
    removeQLPreviewConverterForURL([m_handle->firstRequest().nsURLRequest(DoNotUpdateHTTPBody) URL]);
    [m_converter.get() finishConverting];
    m_converter = nullptr;
}

QuickLookHandle::~QuickLookHandle()
{
    LOG(Network, "QuickLookHandle::~QuickLookHandle()");
    if (m_quicklookFileHandle) {
        m_quicklookFileHandle = nullptr;
        removeQLPreviewConverterForURL([m_handle->firstRequest().nsURLRequest(DoNotUpdateHTTPBody) URL]);
    }
    m_converter = nullptr;

    [m_delegate.get() clearHandle];
}

}

#endif // USE(QUICK_LOOK)
