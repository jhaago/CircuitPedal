#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "KnobInteraction.h"
#include "PedalPackage.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

using DrawRectFunction = void (*)(id, SEL, NSRect);

IMP gPreviousPackageHeroDraw = nullptr;
IMP gPreviousPackageChainDraw = nullptr;

enum class CPPackageAssetRole {
    HeroPedal,
    HeroBackground,
    MiniPedal
};

NSArray<NSDictionary<NSString*, id>*>* CPPackageArtworkCatalog()
{
    static NSArray<NSDictionary<NSString*, id>*>* catalog = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString* pedalsRoot =
            [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"pedals"];
        NSArray<NSString*>* packageNames = [[NSFileManager defaultManager]
            contentsOfDirectoryAtPath:pedalsRoot error:nil];
        NSMutableArray<NSDictionary<NSString*, id>*>* discovered =
            [NSMutableArray array];

        for (NSString* packageName in packageNames)
        {
            NSString* manifestPath = [[pedalsRoot stringByAppendingPathComponent:packageName]
                stringByAppendingPathComponent:@"pedal.json"];
            circuitpedal::PedalPackageManifest manifest;
            std::string error;
            if (!circuitpedal::loadPedalPackageManifest(
                    manifestPath.fileSystemRepresentation, manifest, error))
            {
                continue;
            }

            const auto resolve = [&](const std::string& relativePath) -> NSString* {
                if (relativePath.empty())
                    return nil;
                const std::string resolved =
                    circuitpedal::resolvePedalPackagePath(manifest, relativePath);
                return [NSString stringWithUTF8String:resolved.c_str()];
            };

            NSString* displayName =
                [NSString stringWithUTF8String:manifest.displayName.c_str()];
            NSString* packageID = [NSString stringWithUTF8String:manifest.id.c_str()];
            NSString* heroPedal = resolve(manifest.assets.faceplate);
            NSString* heroBackground = resolve(manifest.assets.heroBackground);
            NSString* miniPedal = resolve(manifest.assets.thumbnail);
            if (displayName.length == 0 || packageID.length == 0 ||
                heroPedal.length == 0 || heroBackground.length == 0 || miniPedal.length == 0)
            {
                continue;
            }

            NSMutableArray<NSDictionary<NSString*, id>*>* controls =
                [NSMutableArray arrayWithCapacity:manifest.controls.size()];
            for (const auto& control : manifest.controls)
            {
                NSString* controlID =
                    [[NSString stringWithUTF8String:control.id.c_str()] uppercaseString];
                NSString* controlType =
                    [[NSString stringWithUTF8String:control.type.c_str()] lowercaseString];
                if (controlID.length == 0 || controlType.length == 0)
                    continue;
                [controls addObject:@{
                    @"id": controlID,
                    @"type": controlType,
                    @"x": @(control.x),
                    @"y": @(control.y),
                    @"size": @(control.size)
                }];
            }

            [discovered addObject:@{
                @"displayName": displayName,
                @"packageID": packageID,
                @"heroPedal": heroPedal,
                @"heroBackground": heroBackground,
                @"miniPedal": miniPedal,
                @"controls": controls
            }];
        }
        catalog = [discovered copy];
    });

    return catalog;
}

NSDictionary<NSString*, id>* CPPackageForModelName(NSString* modelName)
{
    if (modelName.length == 0)
        return nil;

    for (NSDictionary<NSString*, id>* package in CPPackageArtworkCatalog())
    {
        NSString* displayName = package[@"displayName"];
        if ([modelName rangeOfString:displayName
                             options:NSCaseInsensitiveSearch].location != NSNotFound)
        {
            return package;
        }
    }
    return nil;
}

NSString* CPPackageAssetPath(NSDictionary<NSString*, id>* package,
                             CPPackageAssetRole role)
{
    switch (role)
    {
        case CPPackageAssetRole::HeroPedal: return package[@"heroPedal"];
        case CPPackageAssetRole::HeroBackground: return package[@"heroBackground"];
        case CPPackageAssetRole::MiniPedal: return package[@"miniPedal"];
    }
    return nil;
}

NSImage* CPPackageArtwork(NSDictionary<NSString*, id>* package,
                          CPPackageAssetRole role)
{
    static NSMutableDictionary<NSString*, NSImage*>* cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [[NSMutableDictionary alloc] init];
    });

    NSString* fullPath = CPPackageAssetPath(package, role);
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

NSRect CPPackageAspectRect(NSSize imageSize, NSRect container, BOOL fill)
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

void CPPackageDrawImage(NSImage* image, NSRect rect, CGFloat fraction)
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

void CPPackageDrawKnobOverlay(NSPoint center,
                              CGFloat diameter,
                              double normalizedValue,
                              CGFloat opacity)
{
    const CGFloat safeDiameter = std::max<CGFloat>(12.0, diameter);
    const CGFloat radius = safeDiameter * 0.5;
    const NSRect outerRect = NSMakeRect(center.x - radius,
                                        center.y - radius,
                                        safeDiameter,
                                        safeDiameter);

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.72 * opacity];
    shadow.shadowBlurRadius = std::max<CGFloat>(2.0, radius * 0.18);
    shadow.shadowOffset = NSMakeSize(0.0, -std::max<CGFloat>(1.0, radius * 0.08));
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:outerRect];
    [[NSColor colorWithSRGBRed:0.035 green:0.040 blue:0.044 alpha:opacity] setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    const NSRect faceRect = NSInsetRect(outerRect, radius * 0.10, radius * 0.10);
    NSBezierPath* face = [NSBezierPath bezierPathWithOvalInRect:faceRect];
    [NSGraphicsContext saveGraphicsState];
    [face addClip];
    NSGradient* gradient = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithSRGBRed:0.31 green:0.33 blue:0.34 alpha:opacity]
        endingColor:[NSColor colorWithSRGBRed:0.045 green:0.050 blue:0.054 alpha:opacity]];
    [gradient drawFromCenter:NSMakePoint(center.x - radius * 0.22,
                                         center.y + radius * 0.25)
                         radius:0.0
                       toCenter:center
                         radius:radius
                        options:NSGradientDrawsBeforeStartingLocation |
                                NSGradientDrawsAfterEndingLocation];
    [NSGraphicsContext restoreGraphicsState];
    [[NSColor colorWithWhite:0.75 alpha:0.72 * opacity] setStroke];
    face.lineWidth = std::max<CGFloat>(0.8, radius * 0.045);
    [face stroke];

    const CGFloat radians = static_cast<CGFloat>(
        circuitpedal::knobAngleDegrees(normalizedValue) * (M_PI / 180.0));
    NSBezierPath* marker = [NSBezierPath bezierPath];
    [marker moveToPoint:NSMakePoint(center.x + std::cos(radians) * radius * 0.18,
                                    center.y + std::sin(radians) * radius * 0.18)];
    [marker lineToPoint:NSMakePoint(center.x + std::cos(radians) * radius * 0.68,
                                    center.y + std::sin(radians) * radius * 0.68)];
    [[NSColor colorWithSRGBRed:0.93 green:0.94 blue:0.94 alpha:opacity] setStroke];
    marker.lineWidth = std::max<CGFloat>(1.4, radius * 0.10);
    marker.lineCapStyle = NSLineCapStyleRound;
    [marker stroke];
}

void CPPackageHeroDraw(id object, SEL command, NSRect dirtyRect)
{
    NSView* view = (NSView*)object;
    NSString* pedalName = nil;
    NSDictionary<NSString*, NSNumber*>* controlValues = nil;
    BOOL bypassed = NO;
    @try {
        pedalName = [view valueForKey:@"pedalName"];
        controlValues = [view valueForKey:@"controlValues"];
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) {
    }

    NSDictionary<NSString*, id>* package = CPPackageForModelName(pedalName);
    if (package == nil)
    {
        if (gPreviousPackageHeroDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousPackageHeroDraw)(object, command, dirtyRect);
        return;
    }

    NSImage* background =
        CPPackageArtwork(package, CPPackageAssetRole::HeroBackground);
    NSImage* pedal = CPPackageArtwork(package, CPPackageAssetRole::HeroPedal);
    if (background == nil || pedal == nil)
    {
        if (gPreviousPackageHeroDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousPackageHeroDraw)(object, command, dirtyRect);
        return;
    }

    const NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* stage =
        [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:14.0 yRadius:14.0];

    [NSGraphicsContext saveGraphicsState];
    [stage addClip];

    const NSRect backgroundRect = CPPackageAspectRect(background.size, bounds, YES);
    CPPackageDrawImage(background, backgroundRect, 1.0);

    // Keep the package scenery atmospheric so the enclosure remains the focus.
    [[NSColor colorWithWhite:0.0 alpha:(bypassed ? 0.46 : 0.16)] setFill];
    // NSRectFill uses copy compositing, which replaces the freshly drawn image
    // with a translucent black surface.  When that surface is composited over
    // the dark hero view it makes the package background appear to be missing.
    // Source-over preserves the scenery and applies the intended darkening.
    NSRectFillUsingOperation(bounds, NSCompositingOperationSourceOver);

    const CGFloat maximumPedalHeight =
        std::min<CGFloat>(410.0, std::max<CGFloat>(80.0, NSHeight(bounds) - 20.0));
    const CGFloat maximumPedalWidth =
        std::min<CGFloat>(320.0, std::max<CGFloat>(80.0, NSWidth(bounds) * 0.48));
    const NSRect pedalContainer =
        NSMakeRect(NSMidX(bounds) - maximumPedalWidth * 0.5,
                   NSMinY(bounds) + 10.0,
                   maximumPedalWidth,
                   maximumPedalHeight);
    NSRect pedalRect = CPPackageAspectRect(pedal.size, pedalContainer, NO);

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.88];
    shadow.shadowBlurRadius = 24.0;
    shadow.shadowOffset = NSMakeSize(0.0, -9.0);
    [shadow set];
    CPPackageDrawImage(pedal, pedalRect, bypassed ? 0.48 : 1.0);
    [NSGraphicsContext restoreGraphicsState];

    const CGFloat overlayOpacity = bypassed ? 0.48 : 1.0;
    NSArray<NSDictionary<NSString*, id>*>* controls = package[@"controls"];
    for (NSDictionary<NSString*, id>* control in controls)
    {
        if (![control[@"type"] isEqualToString:@"knob"])
            continue;

        NSString* controlID = control[@"id"];
        NSNumber* value = controlValues[controlID];
        if (value == nil)
            continue;

        const CGFloat x = std::clamp<CGFloat>([control[@"x"] doubleValue], 0.0, 1.0);
        const CGFloat y = std::clamp<CGFloat>([control[@"y"] doubleValue], 0.0, 1.0);
        const CGFloat size = std::clamp<CGFloat>([control[@"size"] doubleValue], 0.5, 2.0);
        const NSPoint center = NSMakePoint(NSMinX(pedalRect) + x * NSWidth(pedalRect),
                                           NSMaxY(pedalRect) - y * NSHeight(pedalRect));
        CPPackageDrawKnobOverlay(center,
                                 NSWidth(pedalRect) * 0.16 * size,
                                 value.doubleValue,
                                 overlayOpacity);
    }

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

void CPPackageChainDraw(id object, SEL command, NSRect dirtyRect)
{
    NSView* view = (NSView*)object;
    NSString* selectedName = nil;
    BOOL bypassed = NO;
    @try {
        selectedName = [view valueForKey:@"selectedName"];
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) {
    }

    NSDictionary<NSString*, id>* package = CPPackageForModelName(selectedName);
    if (package == nil)
    {
        if (gPreviousPackageChainDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousPackageChainDraw)(object, command, dirtyRect);
        return;
    }

    NSImage* pedal = CPPackageArtwork(package, CPPackageAssetRole::MiniPedal);
    if (pedal == nil)
    {
        if (gPreviousPackageChainDraw != nullptr)
            reinterpret_cast<DrawRectFunction>(gPreviousPackageChainDraw)(object, command, dirtyRect);
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
    const NSRect pedalRect = CPPackageAspectRect(pedal.size, pedalContainer, NO);

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
    CPPackageDrawImage(pedal, pedalRect, bypassed ? 0.48 : 1.0);
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

void CPPackageInstallDrawOverride(NSString* className, IMP replacement, IMP* previousStorage)
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

void CPPackageInstallArtworkPass()
{
    static BOOL installed = NO;
    if (installed)
        return;

    Class heroClass = NSClassFromString(@"CircuitPedalHeroView");
    Class chainClass = NSClassFromString(@"CircuitPedalChainView");
    if (heroClass == Nil || chainClass == Nil)
        return;

    CPPackageInstallDrawOverride(@"CircuitPedalHeroView",
                                 reinterpret_cast<IMP>(CPPackageHeroDraw),
                                 &gPreviousPackageHeroDraw);
    CPPackageInstallDrawOverride(@"CircuitPedalChainView",
                                 reinterpret_cast<IMP>(CPPackageChainDraw),
                                 &gPreviousPackageChainDraw);
    installed = YES;

    for (NSWindow* window in NSApp.windows)
    {
        if ([window.title containsString:@"CircuitPedal"] && window.contentView != nil)
            [window.contentView setNeedsDisplay:YES];
    }
}

} // namespace

@interface CPPedalPackageArtworkInstaller : NSObject
@end

@implementation CPPedalPackageArtworkInstaller

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
                                CPPackageInstallArtworkPass();
                            });
                        });
                    });
                }];
}

@end
