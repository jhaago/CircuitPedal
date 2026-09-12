#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "PedalPackage.h"

#include <algorithm>
#include <string>

namespace {

using DrawRectFunction = void (*)(id, SEL, NSRect);

IMP gPreviousWoollyHeroDraw = nullptr;
IMP gPreviousWoollyChainDraw = nullptr;

BOOL CPWMIsWoollyName(NSString* name)
{
    if (name.length == 0)
        return NO;
    return [name rangeOfString:@"Woolly Mammoth"
                       options:NSCaseInsensitiveSearch].location != NSNotFound;
}

enum class CPWMAssetRole {
    HeroPedal,
    HeroBackground,
    MiniPedal
};

NSString* CPWMAssetPath(CPWMAssetRole role)
{
    static NSDictionary<NSNumber*, NSString*>* paths = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString* manifestPath = [NSBundle.mainBundle.resourcePath
            stringByAppendingPathComponent:@"pedals/woolly_mammoth/pedal.json"];
        circuitpedal::PedalPackageManifest manifest;
        std::string error;
        if (!circuitpedal::loadPedalPackageManifest(
                manifestPath.fileSystemRepresentation, manifest, error))
        {
            paths = @{};
            return;
        }

        const auto resolve = [&](const std::string& relativePath) -> NSString* {
            if (relativePath.empty())
                return nil;
            const std::string resolved =
                circuitpedal::resolvePedalPackagePath(manifest, relativePath);
            return [NSString stringWithUTF8String:resolved.c_str()];
        };

        NSMutableDictionary<NSNumber*, NSString*>* resolvedPaths =
            [NSMutableDictionary dictionary];
        NSString* heroPedal = resolve(manifest.assets.faceplate);
        NSString* heroBackground = resolve(manifest.assets.heroBackground);
        NSString* miniPedal = resolve(manifest.assets.thumbnail);
        if (heroPedal != nil)
            resolvedPaths[@(static_cast<NSInteger>(CPWMAssetRole::HeroPedal))] = heroPedal;
        if (heroBackground != nil)
            resolvedPaths[@(static_cast<NSInteger>(CPWMAssetRole::HeroBackground))] = heroBackground;
        if (miniPedal != nil)
            resolvedPaths[@(static_cast<NSInteger>(CPWMAssetRole::MiniPedal))] = miniPedal;
        paths = [resolvedPaths copy];
    });

    return paths[@(static_cast<NSInteger>(role))];
}

NSImage* CPWMArtwork(CPWMAssetRole role)
{
    static NSMutableDictionary<NSString*, NSImage*>* cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [[NSMutableDictionary alloc] init];
    });

    NSString* fullPath = CPWMAssetPath(role);
    if (fullPath.length == 0)
        return nil;

    NSImage* cached = cache[fullPath];
    if (cached != nil)
        return cached;

    NSImage* image = [[NSImage alloc] initWithContentsOfFile:fullPath];
    if (image != nil && image.isValid)
        cache[fullPath] = image;
    return image;
}

NSRect CPWMAspectRect(NSSize imageSize, NSRect container, BOOL fill)
{
    if (imageSize.width <= 0.0 || imageSize.height <= 0.0 ||
        NSWidth(container) <= 0.0 || NSHeight(container) <= 0.0)
    {
        return container;
    }

    const CGFloat scaleX = NSWidth(container) / imageSize.width;
    const CGFloat scaleY = NSHeight(container) / imageSize.height;
    const CGFloat scale = fill ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
    const CGFloat width = imageSize.width * scale;
    const CGFloat height = imageSize.height * scale;
    return NSMakeRect(NSMidX(container) - width * 0.5,
                      NSMidY(container) - height * 0.5,
                      width,
                      height);
}

void CPWMDrawImage(NSImage* image, NSRect rect, CGFloat fraction)
{
    if (image == nil)
        return;

    [NSGraphicsContext saveGraphicsState];
    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
    [image drawInRect:rect
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:fraction
       respectFlipped:YES
                hints:nil];
    [NSGraphicsContext restoreGraphicsState];
}

void CPWMHeroDraw(id object, SEL command, NSRect dirtyRect)
{
    NSView* view = (NSView*)object;
    NSString* pedalName = nil;
    BOOL bypassed = NO;
    @try {
        pedalName = [view valueForKey:@"pedalName"];
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) {
    }

    if (!CPWMIsWoollyName(pedalName))
    {
        if (gPreviousWoollyHeroDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousWoollyHeroDraw)(object, command, dirtyRect);
        return;
    }

    NSImage* background = CPWMArtwork(CPWMAssetRole::HeroBackground);
    NSImage* pedal = CPWMArtwork(CPWMAssetRole::HeroPedal);
    if (background == nil || pedal == nil)
    {
        if (gPreviousWoollyHeroDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousWoollyHeroDraw)(object, command, dirtyRect);
        return;
    }

    const NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* stage =
        [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:14.0 yRadius:14.0];

    [NSGraphicsContext saveGraphicsState];
    [stage addClip];

    const NSRect backgroundRect = CPWMAspectRect(background.size, bounds, YES);
    CPWMDrawImage(background, backgroundRect, 1.0);

    // Keep the supplied scenery atmospheric so the enclosure remains the focus.
    [[NSColor colorWithWhite:0.0 alpha:(bypassed ? 0.46 : 0.16)] setFill];
    NSRectFill(bounds);

    const CGFloat maximumPedalHeight =
        std::min<CGFloat>(410.0, std::max<CGFloat>(80.0, NSHeight(bounds) - 20.0));
    const CGFloat maximumPedalWidth =
        std::min<CGFloat>(320.0, std::max<CGFloat>(80.0, NSWidth(bounds) * 0.48));
    const NSRect pedalContainer =
        NSMakeRect(NSMidX(bounds) - maximumPedalWidth * 0.5,
                   NSMinY(bounds) + 10.0,
                   maximumPedalWidth,
                   maximumPedalHeight);
    NSRect pedalRect = CPWMAspectRect(pedal.size, pedalContainer, NO);

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.88];
    shadow.shadowBlurRadius = 24.0;
    shadow.shadowOffset = NSMakeSize(0.0, -9.0);
    [shadow set];
    CPWMDrawImage(pedal, pedalRect, bypassed ? 0.48 : 1.0);
    [NSGraphicsContext restoreGraphicsState];

    [NSGraphicsContext restoreGraphicsState];

    [[NSColor colorWithSRGBRed:0.130 green:0.154 blue:0.176 alpha:1.0] setStroke];
    stage.lineWidth = 0.9;
    [stage stroke];

    NSBezierPath* inner =
        [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bounds, 1.3, 1.3)
                                        xRadius:12.7
                                        yRadius:12.7];
    [[NSColor colorWithWhite:1.0 alpha:0.035] setStroke];
    inner.lineWidth = 0.7;
    [inner stroke];
}

void CPWMChainDraw(id object, SEL command, NSRect dirtyRect)
{
    NSView* view = (NSView*)object;
    NSString* selectedName = nil;
    BOOL bypassed = NO;
    @try {
        selectedName = [view valueForKey:@"selectedName"];
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) {
    }

    if (!CPWMIsWoollyName(selectedName))
    {
        if (gPreviousWoollyChainDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousWoollyChainDraw)(object, command, dirtyRect);
        return;
    }

    NSImage* pedal = CPWMArtwork(CPWMAssetRole::MiniPedal);
    if (pedal == nil)
    {
        if (gPreviousWoollyChainDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousWoollyChainDraw)(object, command, dirtyRect);
        return;
    }

    const NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* well =
        [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:9.0 yRadius:9.0];
    NSGradient* wellGradient = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithSRGBRed:0.025 green:0.030 blue:0.035 alpha:1.0]
        endingColor:[NSColor colorWithSRGBRed:0.006 green:0.010 blue:0.013 alpha:1.0]];
    [wellGradient drawInBezierPath:well angle:90.0];
    [[NSColor colorWithSRGBRed:0.090 green:0.110 blue:0.127 alpha:1.0] setStroke];
    well.lineWidth = 0.9;
    [well stroke];

    const CGFloat midY = NSHeight(view.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(view.bounds) - 42.0;

    NSBezierPath* cableShadow = [NSBezierPath bezierPath];
    [cableShadow moveToPoint:NSMakePoint(leftX, midY - 1.0)];
    [cableShadow lineToPoint:NSMakePoint(rightX, midY - 1.0)];
    [[NSColor colorWithWhite:0.0 alpha:0.78] setStroke];
    cableShadow.lineWidth = 7.0;
    [cableShadow stroke];

    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    [[NSColor colorWithSRGBRed:0.088 green:0.100 blue:0.108 alpha:1.0] setStroke];
    cable.lineWidth = 5.0;
    [cable stroke];
    [[NSColor colorWithWhite:0.31 alpha:0.44] setStroke];
    cable.lineWidth = 0.9;
    [cable stroke];

    const CGFloat pedalHeight =
        std::min<CGFloat>(80.0, std::max<CGFloat>(30.0, NSHeight(view.bounds) - 14.0));
    const NSRect pedalContainer =
        NSMakeRect(NSMidX(view.bounds) - 42.0,
                   midY - pedalHeight * 0.5,
                   84.0,
                   pedalHeight);
    const NSRect pedalRect = CPWMAspectRect(pedal.size, pedalContainer, NO);

    if (!bypassed)
    {
        NSGradient* halo = [[NSGradient alloc]
            initWithStartingColor:[NSColor colorWithSRGBRed:0.94 green:0.30 blue:0.195 alpha:0.20]
            endingColor:[NSColor colorWithSRGBRed:0.94 green:0.30 blue:0.195 alpha:0.0]];
        const NSPoint center = NSMakePoint(NSMidX(pedalRect), NSMidY(pedalRect));
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:pedalHeight * 0.78
                     options:NSGradientDrawsBeforeStartingLocation |
                             NSGradientDrawsAfterEndingLocation];
    }

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.90];
    shadow.shadowBlurRadius = 9.0;
    shadow.shadowOffset = NSMakeSize(0.0, -3.0);
    [shadow set];
    CPWMDrawImage(pedal, pedalRect, bypassed ? 0.48 : 1.0);
    [NSGraphicsContext restoreGraphicsState];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:8.8
                                                         weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName:
            [NSColor colorWithSRGBRed:0.455 green:0.505 blue:0.555 alpha:1.0]
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(view.bounds) - 33.0, midY - 5.0)
          withAttributes:ioAttributes];

    const CGFloat jackSize = 7.0;
    const CGFloat jackY = midY - jackSize * 0.5;
    const CGFloat jackXs[] = {
        NSMinX(pedalRect) - jackSize * 0.5,
        NSMaxX(pedalRect) - jackSize * 0.5
    };
    for (CGFloat x : jackXs)
    {
        NSBezierPath* jack =
            [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, jackY, jackSize, jackSize)];
        [[NSColor colorWithSRGBRed:0.19 green:0.20 blue:0.21 alpha:1.0] setFill];
        [jack fill];
        [[NSColor colorWithSRGBRed:0.46 green:0.48 blue:0.49 alpha:1.0] setStroke];
        jack.lineWidth = 0.8;
        [jack stroke];
    }
}

void CPWMInstallDrawOverride(NSString* className, IMP replacement, IMP* previousStorage)
{
    Class cls = NSClassFromString(className);
    if (cls == Nil)
        return;

    Method method = class_getInstanceMethod(cls, @selector(drawRect:));
    if (method == nullptr)
        return;

    if (*previousStorage == nullptr)
        *previousStorage = method_getImplementation(method);
    method_setImplementation(method, replacement);
}

void CPWMInstallArtworkPass()
{
    static BOOL installed = NO;
    if (installed)
        return;

    Class heroClass = NSClassFromString(@"CircuitPedalHeroView");
    Class chainClass = NSClassFromString(@"CircuitPedalChainView");
    if (heroClass == Nil || chainClass == Nil)
        return;

    CPWMInstallDrawOverride(@"CircuitPedalHeroView",
                            reinterpret_cast<IMP>(CPWMHeroDraw),
                            &gPreviousWoollyHeroDraw);
    CPWMInstallDrawOverride(@"CircuitPedalChainView",
                            reinterpret_cast<IMP>(CPWMChainDraw),
                            &gPreviousWoollyChainDraw);
    installed = YES;

    for (NSWindow* window in NSApp.windows)
    {
        if ([window.title containsString:@"CircuitPedal"] && window.contentView != nil)
            [window.contentView setNeedsDisplay:YES];
    }
}

} // namespace

@interface CPWoollyMammothArtworkInstaller : NSObject
@end

@implementation CPWoollyMammothArtworkInstaller

+ (void)load
{
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    // NativeFeedbackPass installs its chain override after two
                    // main-queue turns. One extra turn guarantees this artwork
                    // wrapper captures that final Phase 1 implementation first.
                    dispatch_async(dispatch_get_main_queue(), ^{
                        dispatch_async(dispatch_get_main_queue(), ^{
                            dispatch_async(dispatch_get_main_queue(), ^{
                                CPWMInstallArtworkPass();
                            });
                        });
                    });
                }];
}

@end
