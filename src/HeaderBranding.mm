#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

namespace {

using CPImageInitWithFile = id (*)(id, SEL, NSString*);
IMP gOriginalImageInitWithFile = nullptr;

BOOL CPIsWoollyArtworkPath(NSString* path)
{
    if (path.length == 0)
        return NO;
    return [path containsString:@"/pedals/woolly_mammoth/assets/"]
        && [[path pathExtension].lowercaseString isEqualToString:@"png"];
}

NSImage* CPDecodeWoollyArtworkChunks(NSString* path)
{
    NSString* stem = [[path lastPathComponent] stringByDeletingPathExtension];
    if (![stem isEqualToString:@"hero_pedal"]
        && ![stem isEqualToString:@"hero_background"]
        && ![stem isEqualToString:@"mini_pedal"])
    {
        return nil;
    }

    NSString* directory = [[[NSBundle mainBundle] resourcePath]
        stringByAppendingPathComponent:@"pedals/woolly_mammoth/assets"];
    NSError* listError = nil;
    NSArray<NSString*>* names = [[NSFileManager defaultManager]
        contentsOfDirectoryAtPath:directory
                            error:&listError];
    if (names == nil || listError != nil)
        return nil;

    NSString* prefix = [stem stringByAppendingString:@"_"];
    NSMutableArray<NSString*>* chunks = [NSMutableArray array];
    for (NSString* name in names)
    {
        if ([name hasPrefix:prefix] && [[name pathExtension].lowercaseString isEqualToString:@"b64"])
            [chunks addObject:name];
    }
    [chunks sortUsingSelector:@selector(compare:)];
    if (chunks.count == 0)
        return nil;

    NSMutableString* encoded = [NSMutableString string];
    for (NSString* name in chunks)
    {
        NSString* chunkPath = [directory stringByAppendingPathComponent:name];
        NSError* readError = nil;
        NSString* chunk = [NSString stringWithContentsOfFile:chunkPath
                                                    encoding:NSASCIIStringEncoding
                                                       error:&readError];
        if (chunk == nil || readError != nil)
            return nil;
        [encoded appendString:chunk];
    }

    NSData* data = [[NSData alloc]
        initWithBase64EncodedString:encoded
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
    if (data.length == 0)
        return nil;

    NSImage* image = [[NSImage alloc] initWithData:data];
    return image.isValid ? image : nil;
}

id CPImageInitWithContentsOfFile(id object, SEL command, NSString* path)
{
    if (CPIsWoollyArtworkPath(path))
    {
        NSImage* corrected = CPDecodeWoollyArtworkChunks(path);
        if (corrected != nil)
            return corrected;
    }

    if (gOriginalImageInitWithFile == nullptr)
        return nil;
    return reinterpret_cast<CPImageInitWithFile>(gOriginalImageInitWithFile)(object, command, path);
}

void CPInstallWoollyArtworkResourceFix()
{
    static BOOL installed = NO;
    if (installed)
        return;

    Method method = class_getInstanceMethod(NSImage.class, @selector(initWithContentsOfFile:));
    if (method == nullptr)
        return;

    gOriginalImageInitWithFile = method_getImplementation(method);
    method_setImplementation(method, reinterpret_cast<IMP>(CPImageInitWithContentsOfFile));
    installed = YES;
}

NSTextField* CPFindHeaderText(NSView* topBar, NSString* text)
{
    for (NSView* subview in topBar.subviews)
    {
        if (![subview isKindOfClass:NSTextField.class])
            continue;

        NSTextField* label = (NSTextField*)subview;
        if ([label.stringValue isEqualToString:text])
            return label;
    }
    return nil;
}

void CPInstallHeaderBranding()
{
    id delegate = NSApp.delegate;
    if (delegate == nil)
        return;

    NSView* topBar = nil;
    @try {
        id candidate = [delegate valueForKey:@"topBar"];
        if ([candidate isKindOfClass:NSView.class])
            topBar = (NSView*)candidate;
    } @catch (__unused NSException* exception) {
        return;
    }

    if (topBar == nil)
        return;

    // Work with the real branding controls created by CircuitPedalAppDelegate.
    // The image replaces only these two redundant text labels in their existing
    // top-left header area; no other header views are moved or restructured.
    NSTextField* brand = CPFindHeaderText(topBar, @"CircuitPedal");
    NSTextField* tagline = CPFindHeaderText(topBar, @"REAL CIRCUITS. REAL TONE.");
    if (brand == nil)
        return;

    NSRect brandingRect = brand.frame;
    if (tagline != nil)
        brandingRect = NSUnionRect(brandingRect, tagline.frame);

    NSURL* logoURL = [NSBundle.mainBundle URLForResource:@"circuitpedal_header_logo"
                                           withExtension:@"png"];
    if (logoURL == nil)
        return;

    NSImage* logo = [[NSImage alloc] initWithContentsOfURL:logoURL];
    if (logo == nil || !logo.isValid)
        return;

    [brand removeFromSuperview];
    [tagline removeFromSuperview];

    // Slightly expand the original text footprint for breathing room while
    // staying within the established header height and left branding zone.
    NSRect imageFrame = NSInsetRect(brandingRect, -4.0, -4.0);
    imageFrame.origin.x = MAX(12.0, imageFrame.origin.x);
    imageFrame.origin.y = MAX(8.0, imageFrame.origin.y);
    imageFrame.size.height = MIN(imageFrame.size.height,
                                 NSHeight(topBar.bounds) - imageFrame.origin.y - 8.0);

    NSImageView* imageView = [[NSImageView alloc] initWithFrame:imageFrame];
    imageView.image = logo;
    imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
    imageView.imageAlignment = NSImageAlignLeft;
    imageView.imageFrameStyle = NSImageFrameNone;
    imageView.animates = NO;
    imageView.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    imageView.accessibilityLabel = @"CircuitPedal";
    [topBar addSubview:imageView];
}

} // namespace

@interface CPHeaderBrandingInstaller : NSObject
@end

@implementation CPHeaderBrandingInstaller

+ (void)load
{
    // The first Woolly artwork upload used binary PNG blobs that proved unreliable
    // in the native app. Decode the verified text chunks for those exact package
    // assets before any hero/chain artwork can be cached by the drawing pass.
    CPInstallWoollyArtworkResourceFix();

    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    // The app delegate builds the real top bar during launch.
                    // Defer one turn so all approved header controls exist first.
                    dispatch_async(dispatch_get_main_queue(), ^{
                        CPInstallHeaderBranding();
                    });
                }];
}

@end
