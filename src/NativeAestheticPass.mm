#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cmath>

namespace {

NSColor* CPColor(CGFloat r, CGFloat g, CGFloat b, CGFloat a = 1.0)
{
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:a];
}

NSColor* CPBackground() { return CPColor(0.010, 0.014, 0.019); }
NSColor* CPGraphite0() { return CPColor(0.018, 0.023, 0.029); }
NSColor* CPGraphite1() { return CPColor(0.030, 0.037, 0.045); }
NSColor* CPGraphite2() { return CPColor(0.044, 0.053, 0.063); }
NSColor* CPGraphite3() { return CPColor(0.064, 0.075, 0.086); }
NSColor* CPBorder() { return CPColor(0.130, 0.154, 0.176); }
NSColor* CPText() { return CPColor(0.900, 0.920, 0.940); }
NSColor* CPMuted() { return CPColor(0.455, 0.505, 0.555); }
NSColor* CPSteel() { return CPColor(0.270, 0.600, 0.750); }
NSColor* CPWarm() { return CPColor(0.940, 0.300, 0.195); }
NSColor* CPLive() { return CPColor(0.185, 0.860, 0.415); }
NSColor* CPAmber() { return CPColor(0.970, 0.675, 0.220); }
NSColor* CPRed() { return CPColor(0.950, 0.235, 0.220); }

CGFloat CPClamp(CGFloat value, CGFloat low, CGFloat high)
{
    return std::max(low, std::min(value, high));
}

void CPDrawTexture(NSRect rect, CGFloat alpha, NSInteger count)
{
    if (NSWidth(rect) <= 0.0 || NSHeight(rect) <= 0.0)
        return;

    for (NSInteger i = 0; i < count; ++i)
    {
        const CGFloat x = NSMinX(rect) + std::fmod(13.0 + static_cast<CGFloat>(i * 37), NSWidth(rect));
        const CGFloat y = NSMinY(rect) + std::fmod(7.0 + static_cast<CGFloat>(i * 61), NSHeight(rect));
        const CGFloat size = (i % 7 == 0) ? 1.25 : 0.65;
        [[NSColor colorWithWhite:(i % 3 == 0 ? 1.0 : 0.0) alpha:alpha] setFill];
        NSRectFill(NSMakeRect(x, y, size, size));
    }
}

void CPDrawLED(NSPoint center, CGFloat radius, NSColor* color, BOOL active)
{
    if (active)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[color colorWithAlphaComponent:0.30]
                                                         endingColor:[color colorWithAlphaComponent:0.0]];
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:radius * 3.4
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSRect bezelRect = NSMakeRect(center.x - radius - 1.5,
                                  center.y - radius - 1.5,
                                  radius * 2.0 + 3.0,
                                  radius * 2.0 + 3.0);
    NSBezierPath* bezel = [NSBezierPath bezierPathWithOvalInRect:bezelRect];
    [CPColor(0.045, 0.050, 0.055) setFill];
    [bezel fill];
    [CPColor(0.245, 0.260, 0.270) setStroke];
    bezel.lineWidth = 0.7;
    [bezel stroke];

    NSRect lampRect = NSMakeRect(center.x - radius,
                                 center.y - radius,
                                 radius * 2.0,
                                 radius * 2.0);
    NSBezierPath* lamp = [NSBezierPath bezierPathWithOvalInRect:lampRect];
    NSColor* base = active ? color : CPColor(0.125, 0.135, 0.140);
    NSColor* top = active ? [color blendedColorWithFraction:0.45 ofColor:NSColor.whiteColor]
                          : CPColor(0.215, 0.225, 0.230);
    NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:top endingColor:base];
    [gradient drawInBezierPath:lamp angle:-55.0];
}

void CPDrawFootswitch(NSRect rect, BOOL pressed)
{
    const CGFloat radius = std::min(NSWidth(rect), NSHeight(rect)) * 0.5;

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.78];
    shadow.shadowBlurRadius = 9.0;
    shadow.shadowOffset = NSMakeSize(0.0, -3.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:rect];
    [CPColor(0.34, 0.36, 0.37) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect, 2.0, 2.0)];
    NSGradient* metal = [[NSGradient alloc] initWithStartingColor:CPColor(0.72, 0.74, 0.75)
                                                            endingColor:CPColor(0.22, 0.24, 0.25)];
    [metal drawInBezierPath:ring angle:48.0];
    [CPColor(0.09, 0.10, 0.11) setStroke];
    ring.lineWidth = 1.0;
    [ring stroke];

    NSRect capRect = NSInsetRect(rect, radius * 0.33, radius * 0.33);
    NSBezierPath* cap = [NSBezierPath bezierPathWithOvalInRect:capRect];
    NSGradient* capGradient = [[NSGradient alloc]
        initWithStartingColor:CPColor(0.44, 0.46, 0.47)
        endingColor:(pressed ? CPColor(0.135, 0.145, 0.150) : CPColor(0.195, 0.205, 0.210))];
    [capGradient drawInBezierPath:cap angle:90.0];
    [[NSColor colorWithWhite:1.0 alpha:0.13] setStroke];
    cap.lineWidth = 0.8;
    [cap stroke];
}

void CPPanelDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    CGFloat radius = 10.0;
    @try {
        id value = [view valueForKey:@"cornerRadius"];
        if ([value respondsToSelector:@selector(doubleValue)])
            radius = static_cast<CGFloat>([value doubleValue]);
    } @catch (__unused NSException* exception) {
    }

    const BOOL header = radius < 1.0;
    NSRect bounds = NSInsetRect(view.bounds, header ? 0.0 : 0.5, header ? 0.0 : 0.5);
    NSBezierPath* panel = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:radius yRadius:radius];
    NSGradient* gradient = [[NSGradient alloc]
        initWithStartingColor:(header ? CPColor(0.027, 0.033, 0.040) : CPGraphite2())
        endingColor:(header ? CPColor(0.012, 0.017, 0.022) : CPGraphite0())];
    [gradient drawInBezierPath:panel angle:90.0];

    [NSGraphicsContext saveGraphicsState];
    [panel addClip];
    CPDrawTexture(bounds, 0.011, header ? 105 : 74);
    [NSGraphicsContext restoreGraphicsState];

    [CPBorder() setStroke];
    panel.lineWidth = 0.9;
    [panel stroke];

    if (!header)
    {
        NSBezierPath* inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bounds, 1.4, 1.4)
                                                              xRadius:std::max<CGFloat>(0.0, radius - 1.4)
                                                              yRadius:std::max<CGFloat>(0.0, radius - 1.4)];
        [[NSColor colorWithWhite:1.0 alpha:0.032] setStroke];
        inner.lineWidth = 0.7;
        [inner stroke];
    }

    const CGFloat lineInset = header ? 16.0 : 12.0;
    [[NSColor colorWithWhite:1.0 alpha:(header ? 0.050 : 0.030)] setFill];
    NSRectFill(NSMakeRect(lineInset,
                          NSHeight(view.bounds) - 1.0,
                          std::max<CGFloat>(0.0, NSWidth(view.bounds) - 2.0 * lineInset),
                          1.0));
    if (header)
    {
        [CPColor(0.065, 0.080, 0.094) setFill];
        NSRectFill(NSMakeRect(0.0, 0.0, NSWidth(view.bounds), 1.0));
    }
}

void CPStatusDotDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    BOOL active = NO;
    @try { active = [[view valueForKey:@"active"] boolValue]; }
    @catch (__unused NSException* exception) { }
    CPDrawLED(NSMakePoint(NSMidX(view.bounds), NSMidY(view.bounds)), 3.0, CPLive(), active);
}

void CPMeterDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    double level = 0.0;
    @try { level = [[view valueForKey:@"level"] doubleValue]; }
    @catch (__unused NSException* exception) { }
    level = std::clamp(level, 0.0, 1.0);

    NSBezierPath* well = [NSBezierPath bezierPathWithRoundedRect:view.bounds xRadius:3.0 yRadius:3.0];
    [CPColor(0.006, 0.009, 0.012) setFill];
    [well fill];
    [CPColor(0.105, 0.125, 0.143) setStroke];
    well.lineWidth = 0.8;
    [well stroke];

    const NSInteger count = 24;
    const CGFloat inset = 3.0;
    const CGFloat gap = 1.3;
    const CGFloat usable = std::max<CGFloat>(0.0, NSWidth(view.bounds) - inset * 2.0 - gap * static_cast<CGFloat>(count - 1));
    const CGFloat width = usable / static_cast<CGFloat>(count);
    const NSInteger lit = static_cast<NSInteger>(std::ceil(level * static_cast<double>(count)));

    for (NSInteger i = 0; i < count; ++i)
    {
        const CGFloat normalized = static_cast<CGFloat>(i) / static_cast<CGFloat>(count - 1);
        NSColor* active = normalized > 0.90 ? CPRed() : (normalized > 0.73 ? CPAmber() : CPLive());
        NSColor* color = i < lit ? active : CPColor(0.060, 0.070, 0.076);
        NSRect segment = NSMakeRect(inset + static_cast<CGFloat>(i) * (width + gap),
                                    3.0,
                                    width,
                                    std::max<CGFloat>(1.0, NSHeight(view.bounds) - 6.0));
        if (i < lit)
        {
            [NSGraphicsContext saveGraphicsState];
            NSShadow* glow = [[NSShadow alloc] init];
            glow.shadowColor = [active colorWithAlphaComponent:0.26];
            glow.shadowBlurRadius = 3.2;
            glow.shadowOffset = NSZeroSize;
            [glow set];
            [color setFill];
            NSRectFill(segment);
            [NSGraphicsContext restoreGraphicsState];
        }
        else
        {
            [color setFill];
            NSRectFill(segment);
        }
    }

    [[NSColor colorWithWhite:1.0 alpha:0.055] setFill];
    NSRectFill(NSMakeRect(3.0, NSHeight(view.bounds) - 3.0, std::max<CGFloat>(0.0, NSWidth(view.bounds) - 6.0), 1.0));
}

void CPChainDraw(id object, SEL command, NSRect dirtyRect)
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
    NSGradient* wellGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.025, 0.030, 0.035)
                                                              endingColor:CPColor(0.006, 0.010, 0.013)];
    [wellGradient drawInBezierPath:well angle:90.0];
    [CPColor(0.090, 0.110, 0.127) setStroke];
    well.lineWidth = 0.9;
    [well stroke];

    const CGFloat midY = NSHeight(view.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(view.bounds) - 42.0;

    NSBezierPath* cableShadow = [NSBezierPath bezierPath];
    [cableShadow moveToPoint:NSMakePoint(leftX, midY - 1.0)];
    [cableShadow lineToPoint:NSMakePoint(rightX, midY - 1.0)];
    [CPColor(0.0, 0.0, 0.0, 0.78) setStroke];
    cableShadow.lineWidth = 7.0;
    [cableShadow stroke];

    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    [CPColor(0.088, 0.100, 0.108) setStroke];
    cable.lineWidth = 5.0;
    [cable stroke];
    [CPColor(0.29, 0.31, 0.32, 0.48) setStroke];
    cable.lineWidth = 0.9;
    [cable stroke];

    const CGFloat cardWidth = std::min<CGFloat>(220.0, NSWidth(view.bounds) * 0.34);
    const NSRect cardRect = NSMakeRect((NSWidth(view.bounds) - cardWidth) * 0.5,
                                       10.0,
                                       cardWidth,
                                       NSHeight(view.bounds) - 20.0);

    if (!bypassed)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[CPWarm() colorWithAlphaComponent:0.19]
                                                         endingColor:[CPWarm() colorWithAlphaComponent:0.0]];
        NSPoint center = NSMakePoint(NSMidX(cardRect), NSMidY(cardRect));
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:std::max(NSWidth(cardRect), NSHeight(cardRect)) * 0.70
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:cardRect xRadius:8.0 yRadius:8.0];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.80];
    shadow.shadowBlurRadius = 12.0;
    shadow.shadowOffset = NSMakeSize(0.0, -5.0);
    [shadow set];
    [CPColor(0.08, 0.08, 0.08) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* bodyGradient = [[NSGradient alloc]
        initWithStartingColor:(bypassed ? CPColor(0.105, 0.115, 0.123) : CPColor(0.235, 0.071, 0.052))
        endingColor:(bypassed ? CPColor(0.045, 0.052, 0.060) : CPColor(0.092, 0.027, 0.024))];
    [bodyGradient drawInBezierPath:body angle:90.0];
    [(bypassed ? CPBorder() : [CPWarm() colorWithAlphaComponent:0.78]) setStroke];
    body.lineWidth = bypassed ? 0.9 : 1.35;
    [body stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.12] setStroke];
    NSBezierPath* lip = [NSBezierPath bezierPath];
    [lip moveToPoint:NSMakePoint(NSMinX(cardRect) + 9.0, NSMaxY(cardRect) - 4.0)];
    [lip lineToPoint:NSMakePoint(NSMaxX(cardRect) - 9.0, NSMaxY(cardRect) - 4.0)];
    lip.lineWidth = 0.7;
    [lip stroke];

    CPDrawLED(NSMakePoint(NSMidX(cardRect), NSMaxY(cardRect) - 15.0), 2.3, CPWarm(), !bypassed);
    CPDrawFootswitch(NSMakeRect(NSMidX(cardRect) - 9.0, NSMinY(cardRect) + 8.0, 18.0, 18.0), !bypassed);

    NSDictionary* nameAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:11.3 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPText()
    };
    NSSize nameSize = [selectedName sizeWithAttributes:nameAttributes];
    CGFloat titleWidth = std::min(nameSize.width, NSWidth(cardRect) - 20.0);
    NSRect titleRect = NSMakeRect(NSMidX(cardRect) - titleWidth * 0.5,
                                  NSMidY(cardRect) - 7.0,
                                  std::max<CGFloat>(20.0, NSWidth(cardRect) - 20.0),
                                  16.0);
    [selectedName drawInRect:titleRect withAttributes:nameAttributes];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:8.8 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPMuted()
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(view.bounds) - 33.0, midY - 5.0) withAttributes:ioAttributes];

    const CGFloat jackY = midY - 4.0;
    for (NSInteger side = -1; side <= 1; side += 2)
    {
        const CGFloat x = side < 0 ? NSMinX(cardRect) - 4.0 : NSMaxX(cardRect) - 4.0;
        NSBezierPath* jack = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, jackY, 8.0, 8.0)];
        [CPColor(0.19, 0.20, 0.21) setFill];
        [jack fill];
        [CPColor(0.46, 0.48, 0.49) setStroke];
        jack.lineWidth = 0.8;
        [jack stroke];
    }
}

void CPHeroDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    NSString* pedalName = @"CircuitPedal";
    NSString* pedalType = @"LIVE CIRCUIT MODEL";
    BOOL bypassed = NO;
    @try {
        NSString* nameCandidate = [view valueForKey:@"pedalName"];
        NSString* typeCandidate = [view valueForKey:@"pedalType"];
        if (nameCandidate.length > 0)
            pedalName = nameCandidate;
        if (typeCandidate.length > 0)
            pedalType = typeCandidate;
        bypassed = [[view valueForKey:@"bypassed"] boolValue];
    } @catch (__unused NSException* exception) { }

    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* stage = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:14.0 yRadius:14.0];
    NSGradient* stageGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.052, 0.024, 0.024)
                                                               endingColor:CPColor(0.006, 0.010, 0.014)];
    [stageGradient drawInBezierPath:stage angle:16.0];

    [NSGraphicsContext saveGraphicsState];
    [stage addClip];
    NSPoint bloomCenter = NSMakePoint(NSMidX(bounds), NSMidY(bounds) + 8.0);
    NSGradient* bloom = [[NSGradient alloc] initWithStartingColor:[CPWarm() colorWithAlphaComponent:(bypassed ? 0.045 : 0.18)]
                                                       endingColor:[CPWarm() colorWithAlphaComponent:0.0]];
    [bloom drawFromCenter:bloomCenter
                   radius:6.0
                 toCenter:bloomCenter
                   radius:std::min(NSWidth(bounds), NSHeight(bounds)) * 0.60
                  options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    CPDrawTexture(bounds, 0.015, 185);

    NSGradient* vignette = [[NSGradient alloc] initWithStartingColor:CPColor(0.0, 0.0, 0.0, 0.0)
                                                          endingColor:CPColor(0.0, 0.0, 0.0, 0.44)];
    [vignette drawInRect:NSMakeRect(0.0, 0.0, NSWidth(bounds), 54.0) angle:-90.0];
    [NSGraphicsContext restoreGraphicsState];

    [CPColor(0.102, 0.124, 0.143) setStroke];
    stage.lineWidth = 0.9;
    [stage stroke];
    NSBezierPath* stageInner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bounds, 1.3, 1.3)
                                                               xRadius:12.7
                                                               yRadius:12.7];
    [[NSColor colorWithWhite:1.0 alpha:0.033] setStroke];
    stageInner.lineWidth = 0.7;
    [stageInner stroke];

    const CGFloat enclosureWidth = std::min<CGFloat>(330.0, NSWidth(view.bounds) * 0.48);
    const CGFloat enclosureHeight = std::min<CGFloat>(410.0, NSHeight(view.bounds) - 72.0);
    const NSRect enclosure = NSMakeRect(NSMidX(view.bounds) - enclosureWidth * 0.5,
                                        42.0,
                                        enclosureWidth,
                                        enclosureHeight);
    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:enclosure xRadius:17.0 yRadius:17.0];

    [NSGraphicsContext saveGraphicsState];
    NSShadow* pedalShadow = [[NSShadow alloc] init];
    pedalShadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.84];
    pedalShadow.shadowBlurRadius = 28.0;
    pedalShadow.shadowOffset = NSMakeSize(0.0, -11.0);
    [pedalShadow set];
    [CPColor(0.13, 0.035, 0.029) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* enclosureGradient = [[NSGradient alloc]
        initWithStartingColor:CPColor(0.300, 0.083, 0.060)
        endingColor:CPColor(0.090, 0.022, 0.025)];
    [enclosureGradient drawInBezierPath:body angle:77.0];

    [NSGraphicsContext saveGraphicsState];
    [body addClip];
    CPDrawTexture(enclosure, 0.026, 105);
    for (NSInteger i = 0; i < 46; ++i)
    {
        const CGFloat innerWidth = std::max<CGFloat>(1.0, NSWidth(enclosure) - 20.0);
        const CGFloat innerHeight = std::max<CGFloat>(1.0, NSHeight(enclosure) - 24.0);
        const CGFloat x = NSMinX(enclosure) + 10.0 + std::fmod(static_cast<CGFloat>(i * 41), innerWidth);
        const CGFloat y = NSMinY(enclosure) + 12.0 + std::fmod(static_cast<CGFloat>(i * 67), innerHeight);
        const CGFloat scratchWidth = 3.0 + static_cast<CGFloat>(i % 8);
        [[NSColor colorWithWhite:(i % 2 == 0 ? 1.0 : 0.0) alpha:(i % 2 == 0 ? 0.033 : 0.072)] setStroke];
        NSBezierPath* scratch = [NSBezierPath bezierPath];
        [scratch moveToPoint:NSMakePoint(x, y)];
        [scratch lineToPoint:NSMakePoint(x + scratchWidth, y + (i % 3 == 0 ? 1.0 : -0.5))];
        scratch.lineWidth = 0.55;
        [scratch stroke];
    }
    [NSGraphicsContext restoreGraphicsState];

    [(bypassed ? CPBorder() : [CPWarm() colorWithAlphaComponent:0.70]) setStroke];
    body.lineWidth = bypassed ? 1.0 : 1.5;
    [body stroke];
    NSBezierPath* innerBody = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(enclosure, 3.0, 3.0)
                                                               xRadius:14.0
                                                               yRadius:14.0];
    [[NSColor colorWithWhite:1.0 alpha:0.090] setStroke];
    innerBody.lineWidth = 0.8;
    [innerBody stroke];

    for (NSInteger side = -1; side <= 1; side += 2)
    {
        const CGFloat x = side < 0 ? NSMinX(enclosure) - 6.0 : NSMaxX(enclosure) - 2.0;
        NSRect jackRect = NSMakeRect(x, NSMidY(enclosure) + 22.0, 8.0, 28.0);
        NSBezierPath* jack = [NSBezierPath bezierPathWithRoundedRect:jackRect xRadius:3.0 yRadius:3.0];
        NSGradient* jackGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.50, 0.51, 0.51)
                                                                  endingColor:CPColor(0.11, 0.12, 0.13)];
        [jackGradient drawInBezierPath:jack angle:(side < 0 ? 0.0 : 180.0)];
    }

    CPDrawLED(NSMakePoint(NSMidX(enclosure), NSMaxY(enclosure) - 34.0), 4.0, CPWarm(), !bypassed);

    NSDictionary* bodyBrandAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:9.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: CPColor(0.77, 0.78, 0.79),
        NSKernAttributeName: @1.35
    };
    NSString* bodyBrand = @"CIRCUITPEDAL";
    NSSize bodyBrandSize = [bodyBrand sizeWithAttributes:bodyBrandAttributes];
    [bodyBrand drawAtPoint:NSMakePoint(NSMidX(enclosure) - bodyBrandSize.width * 0.5, NSMinY(enclosure) + 104.0)
            withAttributes:bodyBrandAttributes];

    NSDictionary* markAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:18.0 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPColor(0.85, 0.85, 0.84),
        NSKernAttributeName: @2.4
    };
    NSString* mark = @"CIRCUIT";
    NSSize markSize = [mark sizeWithAttributes:markAttributes];
    [mark drawAtPoint:NSMakePoint(NSMidX(enclosure) - markSize.width * 0.5, NSMinY(enclosure) + 76.0)
        withAttributes:markAttributes];

    CPDrawFootswitch(NSMakeRect(NSMidX(enclosure) - 22.0,
                                NSMinY(enclosure) + 22.0,
                                44.0,
                                44.0),
                     !bypassed);

    NSDictionary* titleAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:23.0 weight:NSFontWeightBold],
        NSForegroundColorAttributeName: CPText(),
        NSKernAttributeName: @0.25
    };
    NSSize titleSize = [pedalName sizeWithAttributes:titleAttributes];
    CGFloat titleX = std::max<CGFloat>(24.0, NSMidX(view.bounds) - titleSize.width * 0.5);
    [pedalName drawAtPoint:NSMakePoint(titleX, NSHeight(view.bounds) - 36.0)
            withAttributes:titleAttributes];

    NSDictionary* typeAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: CPMuted(),
        NSKernAttributeName: @0.70
    };
    NSSize typeSize = [pedalType sizeWithAttributes:typeAttributes];
    [pedalType drawAtPoint:NSMakePoint(NSMidX(view.bounds) - typeSize.width * 0.5,
                                       NSHeight(view.bounds) - 56.0)
             withAttributes:typeAttributes];
}

void CPEQDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* background = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:8.0 yRadius:8.0];
    NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.016, 0.022, 0.027)
                                                          endingColor:CPColor(0.004, 0.007, 0.010)];
    [gradient drawInBezierPath:background angle:90.0];
    [CPColor(0.095, 0.117, 0.134) setStroke];
    background.lineWidth = 0.8;
    [background stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.032] setStroke];
    for (NSInteger i = 1; i < 7; ++i)
    {
        CGFloat x = NSWidth(view.bounds) * static_cast<CGFloat>(i) / 7.0;
        NSBezierPath* grid = [NSBezierPath bezierPath];
        [grid moveToPoint:NSMakePoint(x, 0.0)];
        [grid lineToPoint:NSMakePoint(x, NSHeight(view.bounds))];
        grid.lineWidth = 0.7;
        [grid stroke];
    }
    for (NSInteger i = 1; i < 5; ++i)
    {
        CGFloat y = NSHeight(view.bounds) * static_cast<CGFloat>(i) / 5.0;
        NSBezierPath* grid = [NSBezierPath bezierPath];
        [grid moveToPoint:NSMakePoint(0.0, y)];
        [grid lineToPoint:NSMakePoint(NSWidth(view.bounds), y)];
        grid.lineWidth = 0.7;
        [grid stroke];
    }

    NSBezierPath* response = [NSBezierPath bezierPath];
    for (NSInteger i = 0; i < 140; ++i)
    {
        CGFloat t = static_cast<CGFloat>(i) / 139.0;
        CGFloat x = t * NSWidth(view.bounds);
        CGFloat y = NSHeight(view.bounds) * (0.58 - 0.18 * std::sin(t * 2.7) + 0.04 * std::sin(t * 13.0));
        if (i == 0)
            [response moveToPoint:NSMakePoint(x, y)];
        else
            [response lineToPoint:NSMakePoint(x, y)];
    }

    [NSGraphicsContext saveGraphicsState];
    NSShadow* glow = [[NSShadow alloc] init];
    glow.shadowColor = [CPSteel() colorWithAlphaComponent:0.25];
    glow.shadowBlurRadius = 4.5;
    glow.shadowOffset = NSZeroSize;
    [glow set];
    [CPSteel() setStroke];
    response.lineWidth = 1.55;
    [response stroke];
    [NSGraphicsContext restoreGraphicsState];
}

using DrawRectFunction = void (*)(id, SEL, NSRect);
IMP gOriginalRoutingDraw = nullptr;

void CPRoutingDraw(id object, SEL command, NSRect dirtyRect)
{
    if (gOriginalRoutingDraw != nullptr)
        reinterpret_cast<DrawRectFunction>(gOriginalRoutingDraw)(object, command, dirtyRect);

    NSView* view = (NSView*)object;
    NSBezierPath* frame = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(view.bounds, 0.5, 0.5)
                                                          xRadius:8.0
                                                          yRadius:8.0];
    [CPColor(0.115, 0.155, 0.182, 0.66) setStroke];
    frame.lineWidth = 0.9;
    [frame stroke];

    NSGradient* lowerVignette = [[NSGradient alloc] initWithStartingColor:CPColor(0.0, 0.0, 0.0, 0.0)
                                                               endingColor:CPColor(0.0, 0.0, 0.0, 0.22)];
    [lowerVignette drawInRect:NSMakeRect(0.0, 0.0, NSWidth(view.bounds), 38.0) angle:-90.0];
}

void CPInstallDrawOverride(NSString* className, IMP replacement, IMP* originalStorage)
{
    Class cls = NSClassFromString(className);
    if (cls == Nil)
        return;
    Method method = class_getInstanceMethod(cls, @selector(drawRect:));
    if (method == nullptr)
        return;
    if (originalStorage != nullptr && *originalStorage == nullptr)
        *originalStorage = method_getImplementation(method);
    method_setImplementation(method, replacement);
}

void CPInstallDrawPass()
{
    static BOOL installed = NO;
    if (installed)
        return;

    if (NSClassFromString(@"CircuitPedalPanelView") == Nil ||
        NSClassFromString(@"CircuitPedalHeroView") == Nil ||
        NSClassFromString(@"CircuitPedalChainView") == Nil ||
        NSClassFromString(@"CircuitPedalMeterView") == Nil ||
        NSClassFromString(@"CircuitPedalStatusDotView") == Nil ||
        NSClassFromString(@"CircuitPedalEQPreviewView") == Nil ||
        NSClassFromString(@"CircuitPedalRoutingEditorView") == Nil)
        return;

    CPInstallDrawOverride(@"CircuitPedalPanelView", reinterpret_cast<IMP>(CPPanelDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalStatusDotView", reinterpret_cast<IMP>(CPStatusDotDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalMeterView", reinterpret_cast<IMP>(CPMeterDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalChainView", reinterpret_cast<IMP>(CPChainDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalHeroView", reinterpret_cast<IMP>(CPHeroDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalEQPreviewView", reinterpret_cast<IMP>(CPEQDraw), nullptr);
    CPInstallDrawOverride(@"CircuitPedalRoutingEditorView", reinterpret_cast<IMP>(CPRoutingDraw), &gOriginalRoutingDraw);
    installed = YES;
}

} // namespace

@interface CPPremiumKnobCell : NSSliderCell
@end

@implementation CPPremiumKnobCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    (void)controlView;
    const CGFloat side = std::max<CGFloat>(10.0, std::min(NSWidth(cellFrame), NSHeight(cellFrame)) - 4.0);
    NSRect knobRect = NSMakeRect(NSMidX(cellFrame) - side * 0.5,
                                 NSMidY(cellFrame) - side * 0.5,
                                 side,
                                 side);
    NSPoint center = NSMakePoint(NSMidX(knobRect), NSMidY(knobRect));
    const CGFloat radius = side * 0.5;
    const double range = self.maxValue - self.minValue;
    const CGFloat normalized = range > 0.0
        ? CPClamp(static_cast<CGFloat>((self.doubleValue - self.minValue) / range), 0.0, 1.0)
        : 0.0;

    NSBezierPath* arcBackground = [NSBezierPath bezierPath];
    [arcBackground appendBezierPathWithArcWithCenter:center
                                              radius:radius + 0.3
                                          startAngle:-135.0
                                            endAngle:135.0
                                           clockwise:NO];
    [CPColor(0.105, 0.122, 0.132) setStroke];
    arcBackground.lineWidth = 2.0;
    [arcBackground stroke];

    NSBezierPath* valueArc = [NSBezierPath bezierPath];
    [valueArc appendBezierPathWithArcWithCenter:center
                                         radius:radius + 0.3
                                     startAngle:-135.0
                                       endAngle:-135.0 + 270.0 * normalized
                                      clockwise:NO];
    [[CPSteel() colorWithAlphaComponent:0.70] setStroke];
    valueArc.lineWidth = 1.45;
    [valueArc stroke];

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.80];
    shadow.shadowBlurRadius = 7.0;
    shadow.shadowOffset = NSMakeSize(0.0, -3.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knobRect, 2.8, 2.8)];
    [CPColor(0.045, 0.052, 0.058) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSRect faceRect = NSInsetRect(knobRect, 4.5, 4.5);
    NSBezierPath* face = [NSBezierPath bezierPathWithOvalInRect:faceRect];
    [NSGraphicsContext saveGraphicsState];
    [face addClip];
    NSGradient* faceGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.285, 0.310, 0.325)
                                                               endingColor:CPColor(0.025, 0.030, 0.034)];
    [faceGradient drawFromCenter:NSMakePoint(NSMidX(faceRect) - radius * 0.22,
                                             NSMidY(faceRect) + radius * 0.26)
                         radius:0.0
                       toCenter:center
                         radius:radius * 0.88
                        options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    [NSGraphicsContext restoreGraphicsState];
    [CPColor(0.300, 0.330, 0.350) setStroke];
    face.lineWidth = 0.85;
    [face stroke];

    NSBezierPath* innerRing = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(faceRect, 3.0, 3.0)];
    [[NSColor colorWithWhite:1.0 alpha:0.052] setStroke];
    innerRing.lineWidth = 0.75;
    [innerRing stroke];

    const CGFloat angle = (-135.0 + 270.0 * normalized) * static_cast<CGFloat>(M_PI / 180.0);
    NSBezierPath* marker = [NSBezierPath bezierPath];
    [marker moveToPoint:NSMakePoint(center.x + std::cos(angle) * radius * 0.24,
                                    center.y + std::sin(angle) * radius * 0.24)];
    [marker lineToPoint:NSMakePoint(center.x + std::cos(angle) * radius * 0.60,
                                    center.y + std::sin(angle) * radius * 0.60)];
    [CPColor(0.91, 0.93, 0.93) setStroke];
    marker.lineWidth = 2.0;
    marker.lineCapStyle = NSLineCapStyleRound;
    [marker stroke];
}

@end

namespace {

void CPStyleKnob(NSSlider* slider)
{
    if (slider.sliderType != NSSliderTypeCircular || [slider.cell isKindOfClass:CPPremiumKnobCell.class])
        return;

    id target = slider.target;
    SEL action = slider.action;
    const double minimum = slider.minValue;
    const double maximum = slider.maxValue;
    const double value = slider.doubleValue;
    const BOOL enabled = slider.enabled;
    const BOOL continuous = slider.continuous;

    CPPremiumKnobCell* cell = [[CPPremiumKnobCell alloc] init];
    cell.sliderType = NSSliderTypeCircular;
    cell.minValue = minimum;
    cell.maxValue = maximum;
    cell.doubleValue = value;
    cell.enabled = enabled;
    slider.cell = cell;
    slider.target = target;
    slider.action = action;
    slider.continuous = continuous;
    slider.wantsLayer = YES;
    slider.layer.backgroundColor = NSColor.clearColor.CGColor;
}

void CPStyleButton(NSButton* button)
{
    // Keep the existing NSButtonCell and button type intact. This preserves the
    // bypass push-on/push-off semantics and every existing target/action while
    // replacing only the visual bezel with a shared graphite surface.
    button.wantsLayer = YES;
    button.bordered = NO;
    button.layer.backgroundColor = CPGraphite1().CGColor;
    button.layer.borderColor = CPColor(0.135, 0.158, 0.177).CGColor;
    button.layer.borderWidth = 0.8;
    button.layer.cornerRadius = 6.0;
    button.layer.shadowColor = NSColor.blackColor.CGColor;
    button.layer.shadowOpacity = 0.14f;
    button.layer.shadowRadius = 2.5;
    button.layer.shadowOffset = NSMakeSize(0.0, -1.0);
    if (button.font == nil)
        button.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
}

void CPStylePopup(NSPopUpButton* popup)
{
    popup.wantsLayer = YES;
    popup.bordered = NO;
    popup.layer.backgroundColor = CPColor(0.021, 0.027, 0.033).CGColor;
    popup.layer.borderColor = CPColor(0.135, 0.160, 0.182).CGColor;
    popup.layer.borderWidth = 0.8;
    popup.layer.cornerRadius = 6.0;
    popup.layer.shadowColor = NSColor.blackColor.CGColor;
    popup.layer.shadowOpacity = 0.12f;
    popup.layer.shadowRadius = 2.0;
    popup.layer.shadowOffset = NSMakeSize(0.0, -1.0);
    popup.contentTintColor = CPColor(0.77, 0.80, 0.83);
    popup.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
}

void CPStyleSearchField(NSSearchField* search)
{
    search.wantsLayer = YES;
    search.layer.backgroundColor = CPColor(0.017, 0.022, 0.027).CGColor;
    search.layer.borderColor = CPColor(0.120, 0.143, 0.163).CGColor;
    search.layer.borderWidth = 0.8;
    search.layer.cornerRadius = 6.0;
    search.textColor = CPColor(0.75, 0.78, 0.81);
    search.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
}

void CPStyleScrollView(NSScrollView* scroll)
{
    scroll.drawsBackground = YES;
    scroll.backgroundColor = CPColor(0.014, 0.019, 0.024);
    scroll.borderType = NSNoBorder;
    scroll.scrollerStyle = NSScrollerStyleOverlay;
}

void CPApplyTheme(NSView* view)
{
    if ([view isKindOfClass:NSPopUpButton.class])
        CPStylePopup((NSPopUpButton*)view);
    else if ([view isKindOfClass:NSSearchField.class])
        CPStyleSearchField((NSSearchField*)view);
    else if ([view isKindOfClass:NSSlider.class])
        CPStyleKnob((NSSlider*)view);
    else if ([view isKindOfClass:NSButton.class])
        CPStyleButton((NSButton*)view);
    else if ([view isKindOfClass:NSScrollView.class])
        CPStyleScrollView((NSScrollView*)view);

    NSString* className = NSStringFromClass(view.class);
    if ([className isEqualToString:@"CircuitPedalPanelView"] ||
        [className isEqualToString:@"CircuitPedalHeroView"])
    {
        view.wantsLayer = YES;
        view.layer.shadowColor = NSColor.blackColor.CGColor;
        view.layer.shadowOpacity = [className isEqualToString:@"CircuitPedalHeroView"] ? 0.25f : 0.13f;
        view.layer.shadowRadius = [className isEqualToString:@"CircuitPedalHeroView"] ? 10.0 : 5.0;
        view.layer.shadowOffset = NSMakeSize(0.0, -2.0);
    }

    for (NSView* subview in view.subviews)
        CPApplyTheme(subview);
}

void CPApplyWindowTheme()
{
    CPInstallDrawPass();
    for (NSWindow* window in NSApp.windows)
    {
        if (![window.title containsString:@"CircuitPedal"])
            continue;

        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        window.backgroundColor = CPBackground();
        window.titlebarAppearsTransparent = YES;
        if (window.contentView != nil)
        {
            window.contentView.wantsLayer = YES;
            window.contentView.layer.backgroundColor = CPBackground().CGColor;
            CPApplyTheme(window.contentView);
            [window.contentView setNeedsDisplay:YES];
        }
    }
}

} // namespace

@interface CPAestheticPassInstaller : NSObject
@end

@implementation CPAestheticPassInstaller

+ (void)load
{
    CPInstallDrawPass();
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        CPApplyWindowTheme();
                    });
                }];
}

@end
