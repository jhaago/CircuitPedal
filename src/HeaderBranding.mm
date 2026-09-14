#import <Cocoa/Cocoa.h>

namespace {

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
