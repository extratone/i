/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKFileUploadPanel.h"

#if PLATFORM(IOS)

#import "APIArray.h"
#import "APIData.h"
#import "APIString.h"
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WKData.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WebOpenPanelParameters.h"
#import "WebOpenPanelResultListenerProxy.h"
#import "WebPageProxy.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <Photos/Photos.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <WebKit/WebNSFileManagerExtras.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

SOFT_LINK_FRAMEWORK(AVFoundation);
SOFT_LINK_CLASS(AVFoundation, AVAssetImageGenerator);
SOFT_LINK_CLASS(AVFoundation, AVURLAsset);

SOFT_LINK_FRAMEWORK(CoreMedia);
SOFT_LINK_CONSTANT(CoreMedia, kCMTimeZero, CMTime);

SOFT_LINK_FRAMEWORK(Photos);
SOFT_LINK_CLASS(Photos, PHAsset);
SOFT_LINK_CLASS(Photos, PHImageManager);
SOFT_LINK_CLASS(Photos, PHImageRequestOptions);
SOFT_LINK_CONSTANT(Photos, PHImageRequestOptionsResizeModeNone, NSString *);
SOFT_LINK_CONSTANT(Photos, PHImageRequestOptionsVersionCurrent, NSString *);

#define kCMTimeZero getkCMTimeZero()

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#pragma mark - Document picker icons

static inline UIImage *photoLibraryIcon()
{
    // FIXME: Remove when a new SDK is available. <rdar://problem/20150072>
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000 && defined(HAVE_WEBKIT_DOC_PICKER_ICONS)
    return _UIImageGetWebKitPhotoLibraryIcon();
#else
    return nil;
#endif
}

static inline UIImage *cameraIcon()
{
    // FIXME: Remove when a new SDK is available. <rdar://problem/20150072>
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000 && defined(HAVE_WEBKIT_DOC_PICKER_ICONS)
    return _UIImageGetWebKitTakePhotoOrVideoIcon();
#else
    return nil;
#endif
}

#pragma mark - Icon generation

static const CGFloat iconSideLength = 100;

static CGRect squareCropRectForSize(CGSize size)
{
    CGFloat smallerSide = MIN(size.width, size.height);
    CGRect cropRect = CGRectMake(0, 0, smallerSide, smallerSide);

    if (size.width < size.height)
        cropRect.origin.y = std::round((size.height - smallerSide) / 2);
    else
        cropRect.origin.x = std::round((size.width - smallerSide) / 2);

    return cropRect;
}

static UIImage *squareImage(CGImageRef image)
{
    if (!image)
        return nil;

    CGSize imageSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    if (imageSize.width == imageSize.height)
        return [UIImage imageWithCGImage:image];

    CGRect squareCropRect = squareCropRectForSize(imageSize);
    RetainPtr<CGImageRef> squareImage = adoptCF(CGImageCreateWithImageInRect(image, squareCropRect));
    return [UIImage imageWithCGImage:squareImage.get()];
}

static UIImage *thumbnailSizedImageForImage(CGImageRef image)
{
    UIImage *squaredImage = squareImage(image);
    if (!squaredImage)
        return nil;

    CGRect destRect = CGRectMake(0, 0, iconSideLength, iconSideLength);
    UIGraphicsBeginImageContext(destRect.size);
    CGContextSetInterpolationQuality(UIGraphicsGetCurrentContext(), kCGInterpolationHigh);
    [squaredImage drawInRect:destRect];
    UIImage *resultImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return resultImage;
}

static UIImage* fallbackIconForFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    UIDocumentInteractionController *interactionController = [UIDocumentInteractionController interactionControllerWithURL:file];
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 90000
    return thumbnailSizedImageForImage(((UIImage *)interactionController.icons[0]).CGImage);
#else
    return thumbnailSizedImageForImage(interactionController.icons[0].CGImage);
#endif
}

static UIImage* iconForImageFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    NSDictionary *options = @{
        (id)kCGImageSourceCreateThumbnailFromImageIfAbsent: @YES,
        (id)kCGImageSourceThumbnailMaxPixelSize: @(iconSideLength),
        (id)kCGImageSourceCreateThumbnailWithTransform: @YES,
    };
    RetainPtr<CGImageSource> imageSource = adoptCF(CGImageSourceCreateWithURL((CFURLRef)file, 0));
    RetainPtr<CGImageRef> thumbnail = adoptCF(CGImageSourceCreateThumbnailAtIndex(imageSource.get(), 0, (CFDictionaryRef)options));
    if (!thumbnail) {
        LOG_ERROR("WKFileUploadPanel: Error creating thumbnail image for image: %@", file);
        return fallbackIconForFile(file);
    }

    return thumbnailSizedImageForImage(thumbnail.get());
}

static UIImage* iconForVideoFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    RetainPtr<AVURLAsset> asset = adoptNS([allocAVURLAssetInstance() initWithURL:file options:nil]);
    RetainPtr<AVAssetImageGenerator> generator = adoptNS([allocAVAssetImageGeneratorInstance() initWithAsset:asset.get()]);
    [generator setAppliesPreferredTrackTransform:YES];

    NSError *error = nil;
    RetainPtr<CGImageRef> imageRef = adoptCF([generator copyCGImageAtTime:kCMTimeZero actualTime:nil error:&error]);
    if (!imageRef) {
        LOG_ERROR("WKFileUploadPanel: Error creating image for video '%@': %@", file, error);
        return fallbackIconForFile(file);
    }

    return thumbnailSizedImageForImage(imageRef.get());
}

static UIImage* iconForFile(NSURL *file)
{
    ASSERT_ARG(file, [file isFileURL]);

    NSString *fileExtension = file.pathExtension;
    if (!fileExtension.length)
        return nil;

    RetainPtr<CFStringRef> fileUTI = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)fileExtension, 0));

    if (UTTypeConformsTo(fileUTI.get(), kUTTypeImage))
        return iconForImageFile(file);

    if (UTTypeConformsTo(fileUTI.get(), kUTTypeMovie))
        return iconForVideoFile(file);

    return fallbackIconForFile(file);
}


#pragma mark - _WKFileUploadItem

@interface _WKFileUploadItem : NSObject
- (instancetype)initWithFileURL:(NSURL *)fileURL;
@property (nonatomic, readonly, getter=isVideo) BOOL video;
@property (nonatomic, readonly) NSURL *fileURL;
@property (nonatomic, readonly) UIImage *displayImage;
@end

@implementation _WKFileUploadItem {
    RetainPtr<NSURL> _fileURL;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL
{
    if (!(self = [super init]))
        return nil;

    ASSERT([fileURL isFileURL]);
    ASSERT([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]);
    _fileURL = fileURL;
    return self;
}

- (BOOL)isVideo
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (NSURL *)fileURL
{
    return _fileURL.get();
}

- (UIImage *)displayImage
{
    ASSERT_NOT_REACHED();
    return nil;
}

@end


@interface _WKImageFileUploadItem : _WKFileUploadItem
@end

@implementation _WKImageFileUploadItem

- (BOOL)isVideo
{
    return NO;
}

- (UIImage *)displayImage
{
    return iconForImageFile(self.fileURL);
}

@end


@interface _WKVideoFileUploadItem : _WKFileUploadItem
@end

@implementation _WKVideoFileUploadItem

- (BOOL)isVideo
{
    return YES;
}

- (UIImage *)displayImage
{
    return iconForVideoFile(self.fileURL);
}

@end


#pragma mark - WKFileUploadPanel

@interface WKFileUploadPanel () <UIPopoverControllerDelegate, UINavigationControllerDelegate, UIImagePickerControllerDelegate, UIDocumentPickerDelegate, UIDocumentMenuDelegate>
@end

@implementation WKFileUploadPanel {
    WKContentView *_view;
    RefPtr<WebKit::WebOpenPanelResultListenerProxy> _listener;
    RetainPtr<NSArray> _mimeTypes;
    CGPoint _interactionPoint;
    BOOL _allowMultipleFiles;
    BOOL _usingCamera;
    RetainPtr<UIImagePickerController> _imagePicker;
    RetainPtr<UIViewController> _presentationViewController; // iPhone always. iPad for Fullscreen Camera.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    RetainPtr<UIPopoverController> _presentationPopover; // iPad for action sheet and Photo Library.
#pragma clang diagnostic pop
    RetainPtr<UIDocumentMenuViewController> _documentMenuController;
    RetainPtr<UIAlertController> _actionSheetController;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;
    _view = view;
    return self;
}

- (void)dealloc
{
    [_imagePicker setDelegate:nil];
    [_presentationPopover setDelegate:nil];
    [_documentMenuController setDelegate:nil];

    [super dealloc];
}

- (void)_dispatchDidDismiss
{
    if ([_delegate respondsToSelector:@selector(fileUploadPanelDidDismiss:)])
        [_delegate fileUploadPanelDidDismiss:self];
}

#pragma mark - Panel Completion (one of these must be called)

- (void)_cancel
{
    _listener->cancel();
    [self _dispatchDidDismiss];
}

- (void)_chooseFiles:(NSArray *)fileURLs displayString:(NSString *)displayString iconImage:(UIImage *)iconImage
{
    NSUInteger count = [fileURLs count];
    if (!count) {
        [self _cancel];
        return;
    }

    Vector<RefPtr<API::Object>> urls;
    urls.reserveInitialCapacity(count);
    for (NSURL *fileURL in fileURLs)
        urls.uncheckedAppend(adoptRef(toImpl(WKURLCreateWithCFURL((CFURLRef)fileURL))));
    Ref<API::Array> fileURLsRef = API::Array::create(WTF::move(urls));

    NSData *jpeg = UIImageJPEGRepresentation(iconImage, 1.0);
    RefPtr<API::Data> iconImageDataRef = adoptRef(toImpl(WKDataCreate(reinterpret_cast<const unsigned char*>([jpeg bytes]), [jpeg length])));

    RefPtr<API::String> displayStringRef = adoptRef(toImpl(WKStringCreateWithCFString((CFStringRef)displayString)));

    _listener->chooseFiles(fileURLsRef.ptr(), displayStringRef.get(), iconImageDataRef.get());
    [self _dispatchDidDismiss];
}

#pragma mark - Present / Dismiss API

- (void)presentWithParameters:(WebKit::WebOpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener
{
    ASSERT(!_listener);

    _listener = listener;
    _allowMultipleFiles = parameters->allowMultipleFiles();
    _interactionPoint = [_view lastInteractionLocation];

    Ref<API::Array> acceptMimeTypes = parameters->acceptMIMETypes();
    NSMutableArray *mimeTypes = [NSMutableArray arrayWithCapacity:acceptMimeTypes->size()];
    for (const auto& mimeType : acceptMimeTypes->elementsOfType<API::String>())
        [mimeTypes addObject:mimeType->string()];
    _mimeTypes = adoptNS([mimeTypes copy]);

    // FIXME: Remove this check and the fallback code when a new SDK is available. <rdar://problem/20150072>
    if ([UIDocumentMenuViewController instancesRespondToSelector:@selector(_setIgnoreApplicationEntitlementForImport:)]) {
        [self _showDocumentPickerMenu];
        return;
    }

    // Fall back to showing the old-style source selection sheet.
    // If there is no camera or this is type=multiple, just show the image picker for the photo library.
    // Otherwise, show an action sheet for the user to choose between camera or library.
    if (_allowMultipleFiles || ![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    else
        [self _showMediaSourceSelectionSheet];
}

- (void)dismiss
{
    [self _dismissDisplayAnimated:NO];
    [self _cancel];
}

- (void)_dismissDisplayAnimated:(BOOL)animated
{
    if (_presentationPopover) {
        [_presentationPopover dismissPopoverAnimated:animated];
        [_presentationPopover setDelegate:nil];
        _presentationPopover = nil;
    }

    if (_presentationViewController) {
        [_presentationViewController dismissViewControllerAnimated:animated completion:^{
            _presentationViewController = nil;
        }];
    }
}

#pragma mark - Media Types

static bool stringHasPrefixCaseInsensitive(NSString *str, NSString *prefix)
{
    NSRange range = [str rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)];
    return range.location != NSNotFound;
}

static NSArray *UTIsForMIMETypes(NSArray *mimeTypes)
{
    // The HTML5 spec mentions the literal "image/*" and "video/*" strings.
    // We support these and go a step further, if the MIME type starts with
    // "image/" or "video/" we adjust the picker's image or video filters.
    // So, "image/jpeg" would make the picker display all images types.
    NSMutableSet *mediaTypes = [NSMutableSet set];
    for (NSString *mimeType in mimeTypes) {
        // FIXME: We should support more MIME type -> UTI mappings. <http://webkit.org/b/142614>
        if (stringHasPrefixCaseInsensitive(mimeType, @"image/"))
            [mediaTypes addObject:(NSString *)kUTTypeImage];
        else if (stringHasPrefixCaseInsensitive(mimeType, @"video/"))
            [mediaTypes addObject:(NSString *)kUTTypeMovie];
    }

    return mediaTypes.allObjects;
}

- (NSArray *)_mediaTypesForPickerSourceType:(UIImagePickerControllerSourceType)sourceType
{
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get());
    if (mediaTypes.count)
        return mediaTypes;

    // Fallback to every supported media type if there is no filter.
    return [UIImagePickerController availableMediaTypesForSourceType:sourceType];
}

- (NSArray *)_documentPickerMenuMediaTypes
{
    NSArray *mediaTypes = UTIsForMIMETypes(_mimeTypes.get());
    if (mediaTypes.count)
        return mediaTypes;

    // Fallback to every supported media type if there is no filter.
    return @[@"public.item"];
}

#pragma mark - Source selection menu

- (NSString *)_photoLibraryButtonLabel
{
    return WEB_UI_STRING_KEY("Photo Library", "Photo Library (file upload action sheet)", "File Upload alert sheet button string for choosing an existing media item from the Photo Library");
}

- (NSString *)_cameraButtonLabel
{
    if (![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera])
        return nil;

    // Choose the appropriate string for the camera button.
    NSArray *filteredMediaTypes = [self _mediaTypesForPickerSourceType:UIImagePickerControllerSourceTypeCamera];
    BOOL containsImageMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeImage];
    BOOL containsVideoMediaType = [filteredMediaTypes containsObject:(NSString *)kUTTypeMovie];
    ASSERT(containsImageMediaType || containsVideoMediaType);
    if (containsImageMediaType && containsVideoMediaType)
        return WEB_UI_STRING_KEY("Take Photo or Video", "Take Photo or Video (file upload action sheet)", "File Upload alert sheet camera button string for taking photos or videos");

    if (containsVideoMediaType)
        return WEB_UI_STRING_KEY("Take Video", "Take Video (file upload action sheet)", "File Upload alert sheet camera button string for taking only videos");

    return WEB_UI_STRING_KEY("Take Photo", "Take Photo (file upload action sheet)", "File Upload alert sheet camera button string for taking only photos");
}

- (void)_showMediaSourceSelectionSheet
{
    NSString *existingString = [self _photoLibraryButtonLabel];
    NSString *cameraString = [self _cameraButtonLabel];
    NSString *cancelString = WEB_UI_STRING_KEY("Cancel", "Cancel (file upload action sheet)", "File Upload alert sheet button string to cancel");

    _actionSheetController = [UIAlertController alertControllerWithTitle:nil message:nil preferredStyle:UIAlertControllerStyleActionSheet];

    UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:cancelString style:UIAlertActionStyleCancel handler:^(UIAlertAction *){
        [self _cancel];
        // We handled cancel ourselves. Prevent the popover controller delegate from cancelling when the popover dismissed.
        [_presentationPopover setDelegate:nil];
    }];

    UIAlertAction *cameraAction = [UIAlertAction actionWithTitle:cameraString style:UIAlertActionStyleDefault handler:^(UIAlertAction *){
        _usingCamera = YES;
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];
    }];

    UIAlertAction *photoLibraryAction = [UIAlertAction actionWithTitle:existingString style:UIAlertActionStyleDefault handler:^(UIAlertAction *){
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    }];

    [_actionSheetController addAction:cancelAction];
    [_actionSheetController addAction:cameraAction];
    [_actionSheetController addAction:photoLibraryAction];

    [self _presentForCurrentInterfaceIdiom:_actionSheetController.get()];
}

- (void)_showDocumentPickerMenu
{
    // FIXME: Support multiple file selection when implemented. <rdar://17177981>
    // FIXME: We call -_setIgnoreApplicationEntitlementForImport: before initialization, because the assertion we're trying
    // to suppress is in the initializer. <rdar://problem/20137692> tracks doing this with a private initializer.
    _documentMenuController = adoptNS([UIDocumentMenuViewController alloc]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [_documentMenuController _setIgnoreApplicationEntitlementForImport:YES];
#pragma clang diagnostic pop
    [_documentMenuController initWithDocumentTypes:[self _documentPickerMenuMediaTypes] inMode:UIDocumentPickerModeImport];
    [_documentMenuController setDelegate:self];

    [_documentMenuController addOptionWithTitle:[self _photoLibraryButtonLabel] image:photoLibraryIcon() order:UIDocumentMenuOrderFirst handler:^{
        [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypePhotoLibrary];
    }];

    if (NSString *cameraString = [self _cameraButtonLabel]) {
        [_documentMenuController addOptionWithTitle:cameraString image:cameraIcon() order:UIDocumentMenuOrderFirst handler:^{
            _usingCamera = YES;
            [self _showPhotoPickerWithSourceType:UIImagePickerControllerSourceTypeCamera];
        }];
    }

    [self _presentForCurrentInterfaceIdiom:_documentMenuController.get()];
}

#pragma mark - Image Picker

- (void)_showPhotoPickerWithSourceType:(UIImagePickerControllerSourceType)sourceType
{
    _imagePicker = adoptNS([[UIImagePickerController alloc] init]);
    [_imagePicker setDelegate:self];
    [_imagePicker setSourceType:sourceType];
    [_imagePicker setAllowsEditing:NO];
    [_imagePicker setModalPresentationStyle:UIModalPresentationFullScreen];
    [_imagePicker _setAllowsMultipleSelection:_allowMultipleFiles];
    [_imagePicker setMediaTypes:[self _mediaTypesForPickerSourceType:sourceType]];

    // Use a popover on the iPad if the source type is not the camera.
    // The camera will use a fullscreen, modal view controller.
    BOOL usePopover = UICurrentUserInterfaceIdiomIsPad() && sourceType != UIImagePickerControllerSourceTypeCamera;
    if (usePopover)
        [self _presentPopoverWithContentViewController:_imagePicker.get() animated:YES];
    else
        [self _presentFullscreenViewController:_imagePicker.get() animated:YES];
}

#pragma mark - Presenting View Controllers

- (void)_presentForCurrentInterfaceIdiom:(UIViewController *)viewController
{
    if (UICurrentUserInterfaceIdiomIsPad())
        [self _presentPopoverWithContentViewController:viewController animated:YES];
    else
        [self _presentFullscreenViewController:viewController animated:YES];
}

- (void)_presentPopoverWithContentViewController:(UIViewController *)contentViewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    _presentationPopover = adoptNS([[UIPopoverController alloc] initWithContentViewController:contentViewController]);
#pragma clang diagnostic pop
    [_presentationPopover setDelegate:self];
    [_presentationPopover presentPopoverFromRect:CGRectIntegral(CGRectMake(_interactionPoint.x, _interactionPoint.y, 1, 1)) inView:_view permittedArrowDirections:UIPopoverArrowDirectionAny animated:animated];
}

- (void)_presentFullscreenViewController:(UIViewController *)viewController animated:(BOOL)animated
{
    [self _dismissDisplayAnimated:animated];

    _presentationViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:_view];
    [_presentationViewController presentViewController:viewController animated:animated completion:nil];
}

#pragma mark - UIPopoverControllerDelegate

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
    [self _cancel];
}

#pragma mark - UIDocumentMenuDelegate implementation

- (void)documentMenu:(UIDocumentMenuViewController *)documentMenu didPickDocumentPicker:(UIDocumentPickerViewController *)documentPicker
{
    documentPicker.delegate = self;
    [self _presentFullscreenViewController:documentPicker animated:YES];
}

- (void)documentMenuWasCancelled:(UIDocumentMenuViewController *)documentMenu
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - UIDocumentPickerControllerDelegate implementation

- (void)documentPicker:(UIDocumentPickerViewController *)documentPicker didPickDocumentAtURL:(NSURL *)url
{
    [self _dismissDisplayAnimated:YES];
    [self _chooseFiles:@[url] displayString:url.lastPathComponent iconImage:iconForFile(url)];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)documentPicker
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - UIImagePickerControllerDelegate implementation

- (BOOL)_willMultipleSelectionDelegateBeCalled
{
    // The multiple selection delegate will not be called when the UIImagePicker was not multiple selection.
    if (!_allowMultipleFiles)
        return NO;

    // The multiple selection delegate will not be called when we used the camera in the UIImagePicker.
    if (_usingCamera)
        return NO;

    return YES;
}

- (void)imagePickerController:(UIImagePickerController *)imagePicker didFinishPickingMediaWithInfo:(NSDictionary *)info
{
    // Sometimes both delegates get called, sometimes just one. Always let the
    // multiple selection delegate handle everything if it will get called.
    if ([self _willMultipleSelectionDelegateBeCalled])
        return;

    [self _dismissDisplayAnimated:YES];

    [self _processMediaInfoDictionaries:[NSArray arrayWithObject:info]
        successBlock:^(NSArray *processedResults, NSString *displayString) {
            ASSERT([processedResults count] == 1);
            _WKFileUploadItem *result = [processedResults objectAtIndex:0];
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _chooseFiles:[NSArray arrayWithObject:result.fileURL] displayString:displayString iconImage:result.displayImage];
            });
        }
        failureBlock:^{
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _cancel];
            });
        }
    ];
}

- (void)imagePickerController:(UIImagePickerController *)imagePicker didFinishPickingMultipleMediaWithInfo:(NSArray *)infos
{
    [self _dismissDisplayAnimated:YES];

    [self _processMediaInfoDictionaries:infos
        successBlock:^(NSArray *processedResults, NSString *displayString) {
            UIImage *iconImage = nil;
            NSMutableArray *fileURLs = [NSMutableArray array];
            for (_WKFileUploadItem *result in processedResults) {
                NSURL *fileURL = result.fileURL;
                if (!fileURL)
                    continue;
                [fileURLs addObject:result.fileURL];
                if (!iconImage)
                    iconImage = result.displayImage;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                [self _chooseFiles:fileURLs displayString:displayString iconImage:iconImage];
            });
        }
        failureBlock:^{
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _cancel];
            });
        }
    ];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)imagePicker
{
    [self _dismissDisplayAnimated:YES];
    [self _cancel];
}

#pragma mark - Process UIImagePicker results

- (void)_processMediaInfoDictionaries:(NSArray *)infos successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)(void))failureBlock
{
    [self _processMediaInfoDictionaries:infos atIndex:0 processedResults:[NSMutableArray array] processedImageCount:0 processedVideoCount:0 successBlock:successBlock failureBlock:failureBlock];
}

- (void)_processMediaInfoDictionaries:(NSArray *)infos atIndex:(NSUInteger)index processedResults:(NSMutableArray *)processedResults processedImageCount:(NSUInteger)processedImageCount processedVideoCount:(NSUInteger)processedVideoCount successBlock:(void (^)(NSArray *processedResults, NSString *displayString))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSUInteger count = [infos count];
    if (index == count) {
        NSString *displayString = [self _displayStringForPhotos:processedImageCount videos:processedVideoCount];
        successBlock(processedResults, displayString);
        return;
    }

    NSDictionary *info = [infos objectAtIndex:index];
    ASSERT(index < count);
    index++;

    auto uploadItemSuccessBlock = ^(_WKFileUploadItem *uploadItem) {
        NSUInteger newProcessedVideoCount = processedVideoCount + (uploadItem.isVideo ? 1 : 0);
        NSUInteger newProcessedImageCount = processedImageCount + (uploadItem.isVideo ? 0 : 1);
        [processedResults addObject:uploadItem];
        [self _processMediaInfoDictionaries:infos atIndex:index processedResults:processedResults processedImageCount:newProcessedImageCount processedVideoCount:newProcessedVideoCount successBlock:successBlock failureBlock:failureBlock];
    };

    [self _uploadItemFromMediaInfo:info successBlock:uploadItemSuccessBlock failureBlock:failureBlock];
}

- (void)_uploadItemForImageData:(NSData *)imageData imageName:(NSString *)imageName successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    ASSERT_ARG(imageData, imageData);
    ASSERT(!isMainThread());

    NSString * const kTemporaryDirectoryName = @"WKWebFileUpload";

    // Build temporary file path.
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *temporaryDirectory = [fileManager _webkit_createTemporaryDirectoryWithTemplatePrefix:kTemporaryDirectoryName];
    NSString *filePath = [temporaryDirectory stringByAppendingPathComponent:imageName];
    if (!filePath) {
        LOG_ERROR("WKFileUploadPanel: Failed to create temporary directory to save image");
        failureBlock();
        return;
    }

    // Save the image to the temporary file.
    NSError *error = nil;
    [imageData writeToFile:filePath options:NSDataWritingAtomic error:&error];
    if (error) {
        LOG_ERROR("WKFileUploadPanel: Error writing image data to temporary file: %@", error);
        failureBlock();
        return;
    }

    successBlock(adoptNS([[_WKImageFileUploadItem alloc] initWithFileURL:[NSURL fileURLWithPath:filePath]]).get());
}

- (void)_uploadItemForJPEGRepresentationOfImage:(UIImage *)image successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    ASSERT_ARG(image, image);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // FIXME: Different compression for different devices?
        // FIXME: Different compression for different UIImage sizes?
        // FIXME: Should EXIF data be maintained?
        const CGFloat compression = 0.8;
        NSData *jpeg = UIImageJPEGRepresentation(image, compression);
        if (!jpeg) {
            LOG_ERROR("WKFileUploadPanel: Failed to create JPEG representation for image");
            failureBlock();
            return;
        }

        // FIXME: Should we get the photo asset and get the actual filename for the photo instead of
        // naming each of the individual uploads image.jpg? This won't work for photos taken with
        // the camera, but would work for photos picked from the library.
        NSString * const kUploadImageName = @"image.jpg";
        [self _uploadItemForImageData:jpeg imageName:kUploadImageName successBlock:successBlock failureBlock:failureBlock];
    });
}

- (void)_uploadItemForImage:(UIImage *)image withAssetURL:(NSURL *)assetURL successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    ASSERT_ARG(image, image);
    ASSERT_ARG(assetURL, assetURL);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        PHFetchResult *result = [getPHAssetClass() fetchAssetsWithALAssetURLs:@[assetURL] options:nil];
        if (!result.count) {
            LOG_ERROR("WKFileUploadPanel: Failed to fetch asset with URL %@", assetURL);
            [self _uploadItemForJPEGRepresentationOfImage:image successBlock:successBlock failureBlock:failureBlock];
            return;
        }

        RetainPtr<PHImageRequestOptions> options = adoptNS([allocPHImageRequestOptionsInstance() init]);
        [options setVersion:PHImageRequestOptionsVersionCurrent];
        [options setSynchronous:YES];
        [options setResizeMode:PHImageRequestOptionsResizeModeNone];

        PHImageManager *manager = (PHImageManager *)[getPHImageManagerClass() defaultManager];
        [manager requestImageDataForAsset:result[0] options:options.get() resultHandler:^(NSData *imageData, NSString *dataUTI, UIImageOrientation, NSDictionary *info) {
            if (!imageData) {
                LOG_ERROR("WKFileUploadPanel: Failed to request image data for asset with URL %@", assetURL);
                [self _uploadItemForJPEGRepresentationOfImage:image successBlock:successBlock failureBlock:failureBlock];
                return;
            }

            RetainPtr<CFStringRef> extension = adoptCF(UTTypeCopyPreferredTagWithClass((CFStringRef)dataUTI, kUTTagClassFilenameExtension));
            NSString *imageName = [@"image." stringByAppendingString:(extension ? (id)extension.get() : @"jpg")];
            [self _uploadItemForImageData:imageData imageName:imageName successBlock:successBlock failureBlock:failureBlock];
        }];
    });
}

- (void)_uploadItemFromMediaInfo:(NSDictionary *)info successBlock:(void (^)(_WKFileUploadItem *))successBlock failureBlock:(void (^)(void))failureBlock
{
    NSString *mediaType = [info objectForKey:UIImagePickerControllerMediaType];

    // For videos from the existing library or camera, the media URL will give us a file path.
    if (UTTypeConformsTo((CFStringRef)mediaType, kUTTypeMovie)) {
        NSURL *mediaURL = [info objectForKey:UIImagePickerControllerMediaURL];
        if (![mediaURL isFileURL]) {
            LOG_ERROR("WKFileUploadPanel: Expected media URL to be a file path, it was not");
            ASSERT_NOT_REACHED();
            failureBlock();
            return;
        }

        successBlock(adoptNS([[_WKVideoFileUploadItem alloc] initWithFileURL:mediaURL]).get());
        return;
    }

    if (!UTTypeConformsTo((CFStringRef)mediaType, kUTTypeImage)) {
        LOG_ERROR("WKFileUploadPanel: Unexpected media type. Expected image or video, got: %@", mediaType);
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

    UIImage *originalImage = [info objectForKey:UIImagePickerControllerOriginalImage];
    if (!originalImage) {
        LOG_ERROR("WKFileUploadPanel: Expected image data but there was none");
        ASSERT_NOT_REACHED();
        failureBlock();
        return;
    }

    // If we have an asset URL, try to upload the native image.
    NSURL *referenceURL = [info objectForKey:UIImagePickerControllerReferenceURL];
    if (referenceURL) {
        [self _uploadItemForImage:originalImage withAssetURL:referenceURL successBlock:successBlock failureBlock:failureBlock];
        return;
    }

    // Photos taken with the camera will not have an asset URL. Fall back to a JPEG representation.
    [self _uploadItemForJPEGRepresentationOfImage:originalImage successBlock:successBlock failureBlock:failureBlock];
}

- (NSString *)_displayStringForPhotos:(NSUInteger)imageCount videos:(NSUInteger)videoCount
{
    if (!imageCount && !videoCount)
        return nil;

    NSString *title;
    NSString *countString;
    NSString *imageString;
    NSString *videoString;
    NSUInteger numberOfTypes = 2;

    RetainPtr<NSNumberFormatter> countFormatter = adoptNS([[NSNumberFormatter alloc] init]);
    [countFormatter setLocale:[NSLocale currentLocale]];
    [countFormatter setGeneratesDecimalNumbers:YES];
    [countFormatter setNumberStyle:NSNumberFormatterDecimalStyle];

    // Generate the individual counts for each type.
    switch (imageCount) {
    case 0:
        imageString = nil;
        --numberOfTypes;
        break;
    case 1:
        imageString = WEB_UI_STRING_KEY("1 Photo", "1 Photo (file upload on page label for one photo)", "File Upload single photo label");
        break;
    default:
        countString = [countFormatter stringFromNumber:@(imageCount)];
        imageString = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ Photos", "# Photos (file upload on page label for multiple photos)", "File Upload multiple photos label"), countString];
        break;
    }

    switch (videoCount) {
    case 0:
        videoString = nil;
        --numberOfTypes;
        break;
    case 1:
        videoString = WEB_UI_STRING_KEY("1 Video", "1 Video (file upload on page label for one video)", "File Upload single video label");
        break;
    default:
        countString = [countFormatter stringFromNumber:@(videoCount)];
        videoString = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ Videos", "# Videos (file upload on page label for multiple videos)", "File Upload multiple videos label"), countString];
        break;
    }

    // Combine into a single result string if needed.
    switch (numberOfTypes) {
    case 2:
        // FIXME: For localization we should build a complete string. We should have a localized string for each different combination.
        title = [NSString stringWithFormat:WEB_UI_STRING_KEY("%@ and %@", "# Photos and # Videos (file upload on page label for image and videos)", "File Upload images and videos label"), imageString, videoString];
        break;
    case 1:
        title = imageString ? imageString : videoString;
        break;
    default:
        ASSERT_NOT_REACHED();
        title = nil;
        break;
    }

    return [title lowercaseString];
}

@end

#pragma clang diagnostic pop

#endif // PLATFORM(IOS)
