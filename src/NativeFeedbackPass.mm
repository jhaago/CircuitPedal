#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cmath>

namespace {

NSColor* CPFeedbackColor(CGFloat r, CGFloat g, CGFloat b, CGFloat a = 1.0)
{
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:a];
}

NSColor* CPFeedbackWarm() { return CPFeedbackColor(0.940, 0.300, 0.195); }
NSColor* CPFeedbackText() { return CPFeedbackColor(0.900, 0.920, 0.940); }
NSColor* CPFeedbackMuted() { return CPFeedbackColor(0.455, 0.505, 0.555); }
NSColor* CPFeedbackBorder() { return CPFeedbackColor(0.130, 0.154, 0.176); }

void CPFeedbackDrawLED(NSPoint center, CGFloat radius, BOOL active)
{
    if (active)
    {
        NSGradient* halo = [[NSGradient alloc]
            initWithStartingColor:[CPFeedbackWarm() colorWithAlphaComponent:0.28]
            endingColor:[CPFeedbackWarm() colorWithAlphaComponent:0.0]];
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:radius * 3.4
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSRect bezelRect = NSMakeRect(center.x - radius - 1.0,
                                  center.y - radius - 1.0,
                                  radius * 2.0 + 2.0,
                                  radius * 2.0 + 2.0);
    NSBezierPath* bezel = [NSBezierPath bezierPathWithOvalInRect:bezelRect];
    [CPFeedbackColor(0.045, 0.050, 0.055) setFill];
    [bezel fill];

    NSRect lampRect = NSMakeRect(center.x - radius,
                                 center.y - radius,
                                 radius * 2.0,
                                 radius * 2.0);
    NSBezierPath* lamp = [NSBezierPath bezierPathWithOvalInRect:lampRect];
    NSColor* base = active ? CPFeedbackWarm() : CPFeedbackColor(0.120, 0.130, 0.135);
    NSColor* top = active
        ? [CPFeedbackWarm() blendedColorWithFraction:0.48 ofColor:NSColor.whiteColor]
        : CPFeedbackColor(0.205, 0.215, 0.220);
    NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:top endingColor:base];
    [gradient drawInBezierPath:lamp angle:-55.0];
}

void CPFeedbackDrawFootswitch(NSRect rect, BOOL pressed)
{
    const CGFloat radius = std::min(NSWidth(rect), NSHeight(rect)) * 0.5;

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.72];
    shadow.shadowBlurRadius = 5.0;
    shadow.shadowOffset = NSMakeSize(0.0, -2.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:rect];
    [CPFeedbackColor(0.35, 0.37, 0.38) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect, 1.2, 1.2)];
    NSGradient* metal = [[NSGradient alloc]
        initWithStartingColor:CPFeedbackColor(0.72, 0.74, 0.75)
        endingColor:CPFeedbackColor(0.22, 0.24, 0.25)];
    [metal drawInBezierPath:ring angle:48.0];

    NSRect capRect = NSInsetRect(rect, radius * 0.34, radius * 0.34);
    NSBezierPath* cap = [NSBezierPath bezierPathWithOvalInRect:capRect];
    NSGradient* capGradient = [[NSGradient alloc]
        initWithStartingColor:CPFeedbackColor(0.43, 0.45, 0.46)
        endingColor:(pressed
            ? CPFeedbackColor(0.13, 0.14, 0.15)
            : CPFeedbackColor(0.19, 0.20, 0.21))];
    [capGradient drawInBezierPath:cap angle:90.0];
}

NSRect CPFeedbackHeroEnclosureRect(NSView* view)
{
    const CGFloat enclosureWidth = std::min<CGFloat>(330.0, NSWidth(view.bounds) * 0.48);
    const CGFloat enclosureHeight = std::min<CGFloat>(410.0, NSHeight(view.bounds) - 72.0);
    return NSMakeRect(NSMidX(view.bounds) - enclosureWidth * 0.5,
                      42.0,
                      enclosureWidth,
                      enclosureHeight);
}

NSRect CPFeedbackHeroFootswitchRect(NSView* view)
{
    const NSRect enclosure = CPFeedbackHeroEnclosureRect(view);
    return NSMakeRect(NSMidX(enclosure) - 22.0,
                      NSMinY(enclosure) + 22.0,
                      44.0,
                      44.0);
}

void CPFeedbackChainDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    NSString* selectedName = @"Current Pedal";
    BOOL bypassed = NO;
    @try {
        NSString* candidate = [view valueForKey:@"selectedName"];
        if (candidate.length > 0)
            selectedName = candidate;
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) {
    }

    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* well = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:9.0 yRadius:9.0];
    NSGradient* wellGradient = [[NSGradient alloc]
        initWithStartingColor:CPFeedbackColor(0.025, 0.030, 0.035)
        endingColor:CPFeedbackColor(0.006, 0.010, 0.013)];
    [wellGradient drawInBezierPath:well angle:90.0];
    [CPFeedbackColor(0.090, 0.110, 0.127) setStroke];
    well.lineWidth = 0.9;
    [well stroke];

    const CGFloat midY = NSHeight(view.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(view.bounds) - 42.0;

    NSBezierPath* cableShadow = [NSBezierPath bezierPath];
    [cableShadow moveToPoint:NSMakePoint(leftX, midY - 1.0)];
    [cableShadow lineToPoint:NSMakePoint(rightX, midY - 1.0)];
    [CPFeedbackColor(0.0, 0.0, 0.0, 0.78) setStroke];
    cableShadow.lineWidth = 7.0;
    [cableShadow stroke];

    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    [CPFeedbackColor(0.088, 0.100, 0.108) setStroke];
    cable.lineWidth = 5.0;
    [cable stroke];
    [CPFeedbackColor(0.29, 0.31, 0.32, 0.48) setStroke];
    cable.lineWidth = 0.9;
    [cable stroke];

    // Keep the mini enclosure at the same width:height ratio as the hero pedal
    // (330 x 410) rather than stretching it into a horizontal card.
    const CGFloat referenceAspect = 330.0 / 410.0;
    const CGFloat availableHeight = std::max<CGFloat>(28.0, NSHeight(view.bounds) - 16.0);
    const CGFloat pedalHeight = std::min<CGFloat>(78.0, availableHeight);
    const CGFloat pedalWidth = pedalHeight * referenceAspect;
    const NSRect pedalRect = NSMakeRect(NSMidX(view.bounds) - pedalWidth * 0.5,
                                        midY - pedalHeight * 0.5,
                                        pedalWidth,
                                        pedalHeight);

    if (!bypassed)
    {
        NSGradient* halo = [[NSGradient alloc]
            initWithStartingColor:[CPFeedbackWarm() colorWithAlphaComponent:0.20]
            endingColor:[CPFeedbackWarm() colorWithAlphaComponent:0.0]];
        const NSPoint center = NSMakePoint(NSMidX(pedalRect), NSMidY(pedalRect));
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:pedalHeight * 0.76
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:pedalRect xRadius:7.0 yRadius:7.0];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.80];
    shadow.shadowBlurRadius = 10.0;
    shadow.shadowOffset = NSMakeSize(0.0, -4.0);
    [shadow set];
    [CPFeedbackColor(0.08, 0.08, 0.08) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* bodyGradient = [[NSGradient alloc]
        initWithStartingColor:(bypassed
            ? CPFeedbackColor(0.105, 0.115, 0.123)
            : CPFeedbackColor(0.235, 0.071, 0.052))
        endingColor:(bypassed
            ? CPFeedbackColor(0.045, 0.052, 0.060)
            : CPFeedbackColor(0.092, 0.027, 0.024))];
    [bodyGradient drawInBezierPath:body angle:90.0];
    [(bypassed ? CPFeedbackBorder() : [CPFeedbackWarm() colorWithAlphaComponent:0.78]) setStroke];
    body.lineWidth = bypassed ? 0.9 : 1.3;
    [body stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.11] setStroke];
    NSBezierPath* lip = [NSBezierPath bezierPath];
    [lip moveToPoint:NSMakePoint(NSMinX(pedalRect) + 6.0, NSMaxY(pedalRect) - 4.0)];
    [lip lineToPoint:NSMakePoint(NSMaxX(pedalRect) - 6.0, NSMaxY(pedalRect) - 4.0)];
    lip.lineWidth = 0.7;
    [lip stroke];

    CPFeedbackDrawLED(NSMakePoint(NSMidX(pedalRect), NSMaxY(pedalRect) - 12.0),
                      2.0,
                      !bypassed);

    const CGFloat switchSide = 12.0;
    CPFeedbackDrawFootswitch(NSMakeRect(NSMidX(pedalRect) - switchSide * 0.5,
                                        NSMinY(pedalRect) + 7.0,
                                        switchSide,
                                        switchSide),
                             !bypassed);

    NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
    paragraph.alignment = NSTextAlignmentCenter;
    paragraph.lineBreakMode = NSLineBreakByTruncatingTail;
    NSDictionary* nameAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:7.7 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPFeedbackText(),
        NSParagraphStyleAttributeName: paragraph
    };
    NSRect titleRect = NSMakeRect(NSMinX(pedalRect) + 4.0,
                                  NSMidY(pedalRect) - 7.0,
                                  std::max<CGFloat>(8.0, NSWidth(pedalRect) - 8.0),
                                  15.0);
    [selectedName drawInRect:titleRect withAttributes:nameAttributes];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:8.8 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPFeedbackMuted()
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(view.bounds) - 33.0, midY - 5.0)
          withAttributes:ioAttributes];

    const CGFloat jackSize = 7.0;
    const CGFloat jackY = midY - jackSize * 0.5;
    const CGFloat leftJackX = NSMinX(pedalRect) - jackSize * 0.5;
    const CGFloat rightJackX = NSMaxX(pedalRect) - jackSize * 0.5;
    for (NSInteger side = 0; side < 2; ++side)
    {
        const CGFloat x = side == 0 ? leftJackX : rightJackX;
        NSBezierPath* jack = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, jackY, jackSize, jackSize)];
        [CPFeedbackColor(0.19, 0.20, 0.21) setFill];
        [jack fill];
        [CPFeedbackColor(0.46, 0.48, 0.49) setStroke];
        jack.lineWidth = 0.8;
        [jack stroke];
    }
}

using VoidFunction = void (*)(id, SEL);
using ActionFunction = void (*)(id, SEL, id);
using MouseFunction = void (*)(id, SEL, NSEvent*);

IMP gOriginalRefreshModelControls = nullptr;
IMP gOriginalStartAudio = nullptr;
IMP gOriginalHeroMouseDown = nullptr;

void CPFeedbackReapplyInputTrim(id delegate)
{
    @try {
        id candidate = [delegate valueForKey:@"inputTrimSlider"];
        if (![candidate isKindOfClass:NSSlider.class])
            return;
        NSSlider* slider = (NSSlider*)candidate;
        if (slider.action != nullptr)
            [slider sendAction:slider.action to:slider.target];
    } @catch (__unused NSException* exception) {
    }
}

void CPFeedbackRefreshModelControls(id object, SEL command)
{
    if (gOriginalRefreshModelControls != nullptr)
        reinterpret_cast<VoidFunction>(gOriginalRefreshModelControls)(object, command);

    // Input trim is application-level state. Re-send the unchanged visible value
    // after every pedal/model refresh so a model swap cannot leave DSP at a
    // different trim than the bottom-panel control shows.
    CPFeedbackReapplyInputTrim(object);
}

void CPFeedbackStartAudio(id object, SEL command, id sender)
{
    // Reapply before starting as a second guard against model-load initialization
    // altering the engine-side trim while the control itself stayed unchanged.
    CPFeedbackReapplyInputTrim(object);
    if (gOriginalStartAudio != nullptr)
        reinterpret_cast<ActionFunction>(gOriginalStartAudio)(object, command, sender);
}

void CPFeedbackHeroMouseDown(id object, SEL command, NSEvent* event)
{
    NSView* view = (NSView*)object;
    const NSPoint point = [view convertPoint:event.locationInWindow fromView:nil];
    const NSRect hitRect = NSInsetRect(CPFeedbackHeroFootswitchRect(view), -10.0, -10.0);

    if (NSPointInRect(point, hitRect))
    {
        @try {
            id delegate = NSApp.delegate;
            id candidate = [delegate valueForKey:@"bypassButton"];
            if ([candidate isKindOfClass:NSButton.class])
            {
                NSButton* bypass = (NSButton*)candidate;
                const BOOL currentlyBypassed = bypass.state == NSControlStateValueOn;
                bypass.state = currentlyBypassed
                    ? NSControlStateValueOff
                    : NSControlStateValueOn;
                if (bypass.action != nullptr)
                    [bypass sendAction:bypass.action to:bypass.target];
                bypass.needsDisplay = YES;
                view.needsDisplay = YES;
                return;
            }
        } @catch (__unused NSException* exception) {
        }
    }

    if (gOriginalHeroMouseDown != nullptr)
        reinterpret_cast<MouseFunction>(gOriginalHeroMouseDown)(object, command, event);
}

void CPFeedbackInstallInstanceOverride(Class cls,
                                       SEL selector,
                                       IMP replacement,
                                       IMP* originalStorage)
{
    if (cls == Nil)
        return;

    Method inheritedOrOwn = class_getInstanceMethod(cls, selector);
    if (inheritedOrOwn == nullptr)
        return;

    const IMP original = method_getImplementation(inheritedOrOwn);
    const char* typeEncoding = method_getTypeEncoding(inheritedOrOwn);

    // For selectors inherited from NSView, create an override on the CircuitPedal
    // class instead of altering the AppKit superclass implementation globally.
    if (class_addMethod(cls, selector, replacement, typeEncoding))
    {
        if (originalStorage != nullptr && *originalStorage == nullptr)
            *originalStorage = original;
        return;
    }

    Method ownMethod = class_getInstanceMethod(cls, selector);
    if (ownMethod == nullptr)
        return;
    if (originalStorage != nullptr && *originalStorage == nullptr)
        *originalStorage = method_getImplementation(ownMethod);
    method_setImplementation(ownMethod, replacement);
}

void CPInstallNativeFeedbackPass()
{
    static BOOL installed = NO;
    if (installed)
        return;

    Class chainClass = NSClassFromString(@"CircuitPedalChainView");
    Class heroClass = NSClassFromString(@"CircuitPedalHeroView");
    Class delegateClass = NSClassFromString(@"CircuitPedalAppDelegate");
    if (chainClass == Nil || heroClass == Nil || delegateClass == Nil)
        return;

    CPFeedbackInstallInstanceOverride(chainClass,
                                      @selector(drawRect:),
                                      reinterpret_cast<IMP>(CPFeedbackChainDraw),
                                      nullptr);
    CPFeedbackInstallInstanceOverride(heroClass,
                                      @selector(mouseDown:),
                                      reinterpret_cast<IMP>(CPFeedbackHeroMouseDown),
                                      &gOriginalHeroMouseDown);
    CPFeedbackInstallInstanceOverride(delegateClass,
                                      @selector(refreshModelControls),
                                      reinterpret_cast<IMP>(CPFeedbackRefreshModelControls),
                                      &gOriginalRefreshModelControls);
    CPFeedbackInstallInstanceOverride(delegateClass,
                                      @selector(startAudio:),
                                      reinterpret_cast<IMP>(CPFeedbackStartAudio),
                                      &gOriginalStartAudio);

    installed = YES;

    for (NSWindow* window in NSApp.windows)
    {
        if ([window.title containsString:@"CircuitPedal"] && window.contentView != nil)
            [window.contentView setNeedsDisplay:YES];
    }
}

} // namespace

@interface CPNativeFeedbackPassInstaller : NSObject
@end

@implementation CPNativeFeedbackPassInstaller

+ (void)load
{
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    // NativeAestheticPass also schedules work after launch. Two
                    // main-queue hops guarantee this feedback layer is installed
                    // after the aesthetic draw overrides, regardless of +load order.
                    dispatch_async(dispatch_get_main_queue(), ^{
                        dispatch_async(dispatch_get_main_queue(), ^{
                            CPInstallNativeFeedbackPass();
                        });
                    });
                }];
}

@end
