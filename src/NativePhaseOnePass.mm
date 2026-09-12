#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cmath>

namespace {

NSColor* P1Color(CGFloat r, CGFloat g, CGFloat b, CGFloat a = 1.0)
{
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:a];
}

NSColor* P1Border() { return P1Color(0.130, 0.154, 0.176); }
NSColor* P1Text() { return P1Color(0.900, 0.920, 0.940); }
NSColor* P1Muted() { return P1Color(0.455, 0.505, 0.555); }
NSColor* P1Warm() { return P1Color(0.940, 0.300, 0.195); }

id P1ObjectIvar(id object, const char* name)
{
    if (object == nil)
        return nil;
    Ivar ivar = class_getInstanceVariable([object class], name);
    return ivar != nullptr ? object_getIvar(object, ivar) : nil;
}

void P1DrawLED(NSPoint center, CGFloat radius, NSColor* color, BOOL active)
{
    if (active)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[color colorWithAlphaComponent:0.28]
                                                         endingColor:[color colorWithAlphaComponent:0.0]];
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:radius * 3.2
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSRect bezelRect = NSMakeRect(center.x - radius - 1.2,
                                  center.y - radius - 1.2,
                                  radius * 2.0 + 2.4,
                                  radius * 2.0 + 2.4);
    NSBezierPath* bezel = [NSBezierPath bezierPathWithOvalInRect:bezelRect];
    [P1Color(0.045, 0.050, 0.055) setFill];
    [bezel fill];
    [P1Color(0.245, 0.260, 0.270) setStroke];
    bezel.lineWidth = 0.6;
    [bezel stroke];

    NSRect lampRect = NSMakeRect(center.x - radius,
                                 center.y - radius,
                                 radius * 2.0,
                                 radius * 2.0);
    NSBezierPath* lamp = [NSBezierPath bezierPathWithOvalInRect:lampRect];
    NSColor* base = active ? color : P1Color(0.125, 0.135, 0.140);
    NSColor* top = active ? [color blendedColorWithFraction:0.45 ofColor:NSColor.whiteColor]
                          : P1Color(0.205, 0.215, 0.220);
    NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:top endingColor:base];
    [gradient drawInBezierPath:lamp angle:-55.0];
}

void P1DrawFootswitch(NSRect rect, BOOL pressed)
{
    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.76];
    shadow.shadowBlurRadius = 5.0;
    shadow.shadowOffset = NSMakeSize(0.0, -2.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:rect];
    [P1Color(0.34, 0.36, 0.37) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect, 1.2, 1.2)];
    NSGradient* metal = [[NSGradient alloc] initWithStartingColor:P1Color(0.72, 0.74, 0.75)
                                                            endingColor:P1Color(0.22, 0.24, 0.25)];
    [metal drawInBezierPath:ring angle:48.0];
    [P1Color(0.09, 0.10, 0.11) setStroke];
    ring.lineWidth = 0.7;
    [ring stroke];

    const CGFloat inset = NSWidth(rect) * 0.27;
    NSBezierPath* cap = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect, inset, inset)];
    NSGradient* capGradient = [[NSGradient alloc]
        initWithStartingColor:P1Color(0.44, 0.46, 0.47)
        endingColor:(pressed ? P1Color(0.135, 0.145, 0.150) : P1Color(0.195, 0.205, 0.210))];
    [capGradient drawInBezierPath:cap angle:90.0];
}

CGFloat P1MainPedalAspectForChain(NSView* chainView)
{
    CGFloat aspect = 330.0 / 370.0;
    NSView* parent = chainView.superview;
    if (parent == nil)
        return aspect;

    for (NSView* sibling in parent.subviews)
    {
        if (![NSStringFromClass([sibling class]) isEqualToString:@"CircuitPedalHeroView"])
            continue;

        const CGFloat width = std::min<CGFloat>(330.0, NSWidth(sibling.bounds) * 0.48);
        const CGFloat height = std::min<CGFloat>(410.0, NSHeight(sibling.bounds) - 72.0);
        if (height > 1.0)
            aspect = width / height;
        break;
    }
    return std::clamp(aspect, 0.68, 1.02);
}

NSString* P1CompactPedalName(NSString* selectedName)
{
    NSString* compact = selectedName.length > 0 ? selectedName : @"Current Pedal";
    NSArray<NSString*>* suffixes = @[ @" Reference Draft", @" Reference", @" Draft" ];
    for (NSString* suffix in suffixes)
    {
        if ([compact hasSuffix:suffix])
        {
            compact = [compact substringToIndex:compact.length - suffix.length];
            break;
        }
    }
    return compact;
}

void P1ChainDraw(id object, SEL command, NSRect dirtyRect)
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
    } @catch (__unused NSException* exception) { }

    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* well = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:9.0 yRadius:9.0];
    NSGradient* wellGradient = [[NSGradient alloc] initWithStartingColor:P1Color(0.025, 0.030, 0.035)
                                                              endingColor:P1Color(0.006, 0.010, 0.013)];
    [wellGradient drawInBezierPath:well angle:90.0];
    [P1Color(0.090, 0.110, 0.127) setStroke];
    well.lineWidth = 0.9;
    [well stroke];

    const CGFloat midY = NSHeight(view.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(view.bounds) - 42.0;

    NSBezierPath* cableShadow = [NSBezierPath bezierPath];
    [cableShadow moveToPoint:NSMakePoint(leftX, midY - 1.0)];
    [cableShadow lineToPoint:NSMakePoint(rightX, midY - 1.0)];
    [P1Color(0.0, 0.0, 0.0, 0.78) setStroke];
    cableShadow.lineWidth = 7.0;
    [cableShadow stroke];

    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    [P1Color(0.088, 0.100, 0.108) setStroke];
    cable.lineWidth = 5.0;
    [cable stroke];
    [P1Color(0.29, 0.31, 0.32, 0.48) setStroke];
    cable.lineWidth = 0.9;
    [cable stroke];

    const CGFloat cardHeight = std::max<CGFloat>(50.0, std::min<CGFloat>(76.0, NSHeight(view.bounds) - 16.0));
    const CGFloat cardWidth = cardHeight * P1MainPedalAspectForChain(view);
    const NSRect cardRect = NSMakeRect(NSMidX(view.bounds) - cardWidth * 0.5,
                                       NSMidY(view.bounds) - cardHeight * 0.5,
                                       cardWidth,
                                       cardHeight);

    if (!bypassed)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[P1Warm() colorWithAlphaComponent:0.20]
                                                         endingColor:[P1Warm() colorWithAlphaComponent:0.0]];
        NSPoint center = NSMakePoint(NSMidX(cardRect), NSMidY(cardRect));
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:cardHeight * 0.78
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:cardRect xRadius:6.0 yRadius:6.0];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.82];
    shadow.shadowBlurRadius = 10.0;
    shadow.shadowOffset = NSMakeSize(0.0, -4.0);
    [shadow set];
    [P1Color(0.08, 0.08, 0.08) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* bodyGradient = [[NSGradient alloc]
        initWithStartingColor:(bypassed ? P1Color(0.105, 0.115, 0.123) : P1Color(0.235, 0.071, 0.052))
        endingColor:(bypassed ? P1Color(0.045, 0.052, 0.060) : P1Color(0.092, 0.027, 0.024))];
    [bodyGradient drawInBezierPath:body angle:90.0];
    [(bypassed ? P1Border() : [P1Warm() colorWithAlphaComponent:0.78]) setStroke];
    body.lineWidth = bypassed ? 0.9 : 1.3;
    [body stroke];

    NSBezierPath* inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(cardRect, 2.0, 2.0)
                                                           xRadius:4.5
                                                           yRadius:4.5];
    [[NSColor colorWithWhite:1.0 alpha:0.075] setStroke];
    inner.lineWidth = 0.55;
    [inner stroke];

    P1DrawLED(NSMakePoint(NSMidX(cardRect), NSMaxY(cardRect) - 9.5), 2.0, P1Warm(), !bypassed);
    P1DrawFootswitch(NSMakeRect(NSMidX(cardRect) - 7.0,
                                NSMinY(cardRect) + 6.0,
                                14.0,
                                14.0),
                     !bypassed);

    NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
    paragraph.alignment = NSTextAlignmentCenter;
    paragraph.lineBreakMode = NSLineBreakByWordWrapping;
    NSDictionary* nameAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:6.8 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: P1Text(),
        NSParagraphStyleAttributeName: paragraph
    };
    NSString* compactName = P1CompactPedalName(selectedName);
    NSRect titleRect = NSMakeRect(NSMinX(cardRect) + 4.0,
                                  NSMinY(cardRect) + 23.0,
                                  std::max<CGFloat>(10.0, NSWidth(cardRect) - 8.0),
                                  std::max<CGFloat>(16.0, NSHeight(cardRect) - 42.0));
    [compactName drawInRect:titleRect withAttributes:nameAttributes];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:8.8 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: P1Muted()
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(view.bounds) - 33.0, midY - 5.0) withAttributes:ioAttributes];

    const CGFloat jackY = midY - 3.5;
    for (NSInteger side = -1; side <= 1; side += 2)
    {
        const CGFloat x = side < 0 ? NSMinX(cardRect) - 3.5 : NSMaxX(cardRect) - 3.5;
        NSBezierPath* jack = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, jackY, 7.0, 7.0)];
        [P1Color(0.19, 0.20, 0.21) setFill];
        [jack fill];
        [P1Color(0.46, 0.48, 0.49) setStroke];
        jack.lineWidth = 0.7;
        [jack stroke];
    }
}

void P1ReapplyInputTrim(id delegate)
{
    NSSlider* slider = (NSSlider*)P1ObjectIvar(delegate, "_inputTrimSlider");
    if (slider == nil)
        return;

    SEL selector = NSSelectorFromString(@"inputTrimChanged:");
    if (![delegate respondsToSelector:selector])
        return;

    IMP implementation = [delegate methodForSelector:selector];
    using ActionFn = void (*)(id, SEL, id);
    reinterpret_cast<ActionFn>(implementation)(delegate, selector, slider);
}

IMP gOriginalRefreshModelControls = nullptr;
IMP gOriginalStartAudio = nullptr;

void P1RefreshModelControls(id object, SEL command)
{
    if (gOriginalRefreshModelControls != nullptr)
    {
        using Fn = void (*)(id, SEL);
        reinterpret_cast<Fn>(gOriginalRefreshModelControls)(object, command);
    }
    P1ReapplyInputTrim(object);
}

void P1StartAudio(id object, SEL command, id sender)
{
    P1ReapplyInputTrim(object);
    if (gOriginalStartAudio != nullptr)
    {
        using Fn = void (*)(id, SEL, id);
        reinterpret_cast<Fn>(gOriginalStartAudio)(object, command, sender);
    }
}

void P1HeroMouseDown(id object, SEL command, NSEvent* event)
{
    (void)command;
    NSView* view = (NSView*)object;
    NSPoint point = [view convertPoint:event.locationInWindow fromView:nil];

    const CGFloat enclosureWidth = std::min<CGFloat>(330.0, NSWidth(view.bounds) * 0.48);
    const CGFloat enclosureHeight = std::min<CGFloat>(410.0, NSHeight(view.bounds) - 72.0);
    const NSRect enclosure = NSMakeRect(NSMidX(view.bounds) - enclosureWidth * 0.5,
                                        42.0,
                                        enclosureWidth,
                                        enclosureHeight);
    const NSRect footswitch = NSMakeRect(NSMidX(enclosure) - 22.0,
                                         NSMinY(enclosure) + 22.0,
                                         44.0,
                                         44.0);

    if (!NSPointInRect(point, NSInsetRect(footswitch, -8.0, -8.0)))
        return;

    id delegate = NSApp.delegate;
    NSButton* bypassButton = (NSButton*)P1ObjectIvar(delegate, "_bypassButton");
    if (bypassButton == nil)
        return;

    bypassButton.state = bypassButton.state == NSControlStateValueOn
        ? NSControlStateValueOff
        : NSControlStateValueOn;

    SEL selector = NSSelectorFromString(@"bypassChanged:");
    if ([delegate respondsToSelector:selector])
        [NSApp sendAction:selector to:delegate from:bypassButton];
}

void P1InstallPhaseOnePass()
{
    Class chainClass = NSClassFromString(@"CircuitPedalChainView");
    if (chainClass != Nil)
    {
        Method draw = class_getInstanceMethod(chainClass, @selector(drawRect:));
        if (draw != nullptr)
            method_setImplementation(draw, reinterpret_cast<IMP>(P1ChainDraw));
    }

    Class heroClass = NSClassFromString(@"CircuitPedalHeroView");
    if (heroClass != Nil)
    {
        class_addMethod(heroClass,
                        @selector(mouseDown:),
                        reinterpret_cast<IMP>(P1HeroMouseDown),
                        "v@:@");
    }

    Class delegateClass = NSClassFromString(@"CircuitPedalAppDelegate");
    if (delegateClass != Nil)
    {
        Method refresh = class_getInstanceMethod(delegateClass, NSSelectorFromString(@"refreshModelControls"));
        if (refresh != nullptr)
        {
            gOriginalRefreshModelControls = method_getImplementation(refresh);
            method_setImplementation(refresh, reinterpret_cast<IMP>(P1RefreshModelControls));
        }

        Method start = class_getInstanceMethod(delegateClass, NSSelectorFromString(@"startAudio:"));
        if (start != nullptr)
        {
            gOriginalStartAudio = method_getImplementation(start);
            method_setImplementation(start, reinterpret_cast<IMP>(P1StartAudio));
        }
    }

    id delegate = NSApp.delegate;
    NSView* chainView = (NSView*)P1ObjectIvar(delegate, "_chainView");
    [chainView setNeedsDisplay:YES];
    P1ReapplyInputTrim(delegate);
}

} // namespace

@interface CPPhaseOneInstaller : NSObject
@end

@implementation CPPhaseOneInstaller
+ (void)load
{
    [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidFinishLaunchingNotification
                                                      object:nil
                                                       queue:NSOperationQueue.mainQueue
                                                  usingBlock:^(__unused NSNotification* notification) {
        dispatch_async(dispatch_get_main_queue(), ^{
            P1InstallPhaseOnePass();
        });
    }];
}
@end
