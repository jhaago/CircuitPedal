#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cmath>

namespace {

NSColor* CPColor(CGFloat r, CGFloat g, CGFloat b, CGFloat a = 1.0)
{
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:a];
}

NSColor* CPBackground() { return CPColor(0.012, 0.016, 0.021); }
NSColor* CPGraphite0() { return CPColor(0.022, 0.028, 0.034); }
NSColor* CPGraphite1() { return CPColor(0.034, 0.042, 0.050); }
NSColor* CPGraphite2() { return CPColor(0.050, 0.060, 0.071); }
NSColor* CPGraphite3() { return CPColor(0.070, 0.082, 0.094); }
NSColor* CPBorder() { return CPColor(0.150, 0.176, 0.198); }
NSColor* CPText() { return CPColor(0.905, 0.925, 0.944); }
NSColor* CPMuted() { return CPColor(0.470, 0.520, 0.570); }
NSColor* CPSteel() { return CPColor(0.290, 0.610, 0.760); }
NSColor* CPWarm() { return CPColor(0.940, 0.310, 0.205); }
NSColor* CPLive() { return CPColor(0.195, 0.875, 0.430); }
NSColor* CPAmber() { return CPColor(0.980, 0.690, 0.230); }
NSColor* CPRed() { return CPColor(0.960, 0.245, 0.230); }

CGFloat CPClamp(CGFloat value, CGFloat low, CGFloat high)
{
    return std::max(low, std::min(value, high));
}

void CPDrawMicroTexture(NSRect rect, CGFloat alpha, NSInteger count)
{
    if (NSWidth(rect) <= 0.0 || NSHeight(rect) <= 0.0)
        return;

    for (NSInteger i = 0; i < count; ++i)
    {
        const CGFloat x = NSMinX(rect) + std::fmod(17.0 + static_cast<CGFloat>(i * 37), NSWidth(rect));
        const CGFloat y = NSMinY(rect) + std::fmod(11.0 + static_cast<CGFloat>(i * 61), NSHeight(rect));
        const CGFloat size = (i % 5 == 0) ? 1.35 : 0.75;
        [[NSColor colorWithWhite:(i % 3 == 0 ? 1.0 : 0.0) alpha:alpha] setFill];
        NSRectFill(NSMakeRect(x, y, size, size));
    }
}

void CPDrawInnerEdge(NSBezierPath* path)
{
    [[NSColor colorWithWhite:1.0 alpha:0.045] setStroke];
    path.lineWidth = 1.0;
    [path stroke];
}

void CPDrawPanelSurface(NSView* view)
{
    CGFloat radius = 10.0;
    @try {
        id value = [view valueForKey:@"cornerRadius"];
        if ([value respondsToSelector:@selector(doubleValue)])
            radius = static_cast<CGFloat>([value doubleValue]);
    } @catch (__unused NSException* exception) {
    }

    const BOOL header = radius < 1.0;
    const NSRect bounds = NSInsetRect(view.bounds, header ? 0.0 : 0.5, header ? 0.0 : 0.5);
    NSBezierPath* panel = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                          xRadius:radius
                                                          yRadius:radius];

    NSGradient* gradient = [[NSGradient alloc]
        initWithStartingColor:(header ? CPColor(0.028, 0.035, 0.042) : CPGraphite2())
        endingColor:(header ? CPColor(0.014, 0.019, 0.024) : CPGraphite0())];
    [gradient drawInBezierPath:panel angle:90.0];

    [NSGraphicsContext saveGraphicsState];
    [panel addClip];
    CPDrawMicroTexture(bounds, 0.012, header ? 110 : 74);
    [NSGraphicsContext restoreGraphicsState];

    [CPColor(0.125, 0.150, 0.172) setStroke];
    panel.lineWidth = 1.0;
    [panel stroke];

    const CGFloat inset = header ? 0.0 : 1.5;
    NSBezierPath* inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bounds, inset, inset)
                                                          xRadius:std::max<CGFloat>(0.0, radius - inset)
                                                          yRadius:std::max<CGFloat>(0.0, radius - inset)];
    CPDrawInnerEdge(inner);

    const CGFloat lineInset = header ? 16.0 : 12.0;
    [[NSColor colorWithWhite:1.0 alpha:(header ? 0.055 : 0.035)] setFill];
    NSRectFill(NSMakeRect(lineInset,
                          NSHeight(view.bounds) - 1.0,
                          std::max<CGFloat>(0.0, NSWidth(view.bounds) - 2.0 * lineInset),
                          1.0));

    if (header)
    {
        [CPColor(0.070, 0.087, 0.100) setFill];
        NSRectFill(NSMakeRect(0.0, 0.0, NSWidth(view.bounds), 1.0));
    }
}

void CPDrawLED(NSPoint center, CGFloat radius, NSColor* color, BOOL active)
{
    NSColor* activeColor = active ? color : CPColor(0.135, 0.145, 0.150);
    if (active)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[color colorWithAlphaComponent:0.30]
                                                         endingColor:[color colorWithAlphaComponent:0.0]];
        [halo drawFromCenter:center
                      radius:0.0
                    toCenter:center
                      radius:radius * 3.3
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSBezierPath* bezel = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(center.x - radius - 1.5,
                                                                            center.y - radius - 1.5,
                                                                            radius * 2.0 + 3.0,
                                                                            radius * 2.0 + 3.0)];
    [CPColor(0.055, 0.060, 0.064) setFill];
    [bezel fill];
    [CPColor(0.250, 0.270, 0.280) setStroke];
    bezel.lineWidth = 0.8;
    [bezel stroke];

    NSBezierPath* lamp = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(center.x - radius,
                                                                           center.y - radius,
                                                                           radius * 2.0,
                                                                           radius * 2.0)];
    NSGradient* lampGradient = [[NSGradient alloc]
        initWithStartingColor:(active ? [activeColor blendedColorWithFraction:0.50 ofColor:NSColor.whiteColor] : CPColor(0.23, 0.24, 0.25))
        endingColor:activeColor];
    [lampGradient drawInBezierPath:lamp angle:-55.0];
}

void CPDrawFootswitch(NSRect rect, BOOL active)
{
    NSPoint center = NSMakePoint(NSMidX(rect), NSMidY(rect));
    CGFloat radius = std::min(NSWidth(rect), NSHeight(rect)) * 0.5;

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.72];
    shadow.shadowBlurRadius = 10.0;
    shadow.shadowOffset = NSMakeSize(0.0, -4.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:rect];
    [CPColor(0.34, 0.36, 0.37) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect, 2.0, 2.0)];
    NSGradient* metal = [[NSGradient alloc] initWithColors:@[
        CPColor(0.72, 0.74, 0.75),
        CPColor(0.25, 0.27, 0.28),
        CPColor(0.55, 0.57, 0.58)
    ]];
    [metal drawInBezierPath:ring angle:48.0];
    [CPColor(0.10, 0.11, 0.12) setStroke];
    ring.lineWidth = 1.0;
    [ring stroke];

    NSRect capRect = NSInsetRect(rect, radius * 0.31, radius * 0.31);
    NSBezierPath* cap = [NSBezierPath bezierPathWithOvalInRect:capRect];
    NSGradient* capGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.42, 0.44, 0.45)
                                                            endingColor:CPColor(active ? 0.16 : 0.20,
                                                                                 active ? 0.17 : 0.21,
                                                                                 active ? 0.18 : 0.22)];
    [capGradient drawInBezierPath:cap angle:90.0];
    [[NSColor colorWithWhite:1.0 alpha:0.16] setStroke];
    cap.lineWidth = 0.8;
    [cap stroke];
    (void)center;
}

void CPPanelDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    CPDrawPanelSurface((NSView*)object);
}

void CPStatusDotDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    BOOL active = NO;
    @try { active = [[view valueForKey:@"active"] boolValue]; } @catch (__unused NSException* exception) {}
    CPDrawLED(NSMakePoint(NSMidX(view.bounds), NSMidY(view.bounds)), 3.1, CPLive(), active);
}

void CPMeterDraw(id object, SEL command, NSRect dirtyRect)
{
    (void)command;
    (void)dirtyRect;
    NSView* view = (NSView*)object;
    double level = 0.0;
    @try { level = [[view valueForKey:@"level"] doubleValue]; } @catch (__unused NSException* exception) {}
    level = std::clamp(level, 0.0, 1.0);

    NSBezierPath* well = [NSBezierPath bezierPathWithRoundedRect:view.bounds xRadius:3.0 yRadius:3.0];
    [CPColor(0.010, 0.013, 0.016) setFill];
    [well fill];
    [CPColor(0.115, 0.135, 0.150) setStroke];
    well.lineWidth = 0.8;
    [well stroke];

    const NSInteger segmentCount = 24;
    const CGFloat inset = 3.0;
    const CGFloat gap = 1.35;
    const CGFloat available = std::max<CGFloat>(0.0, NSWidth(view.bounds) - inset * 2.0 - gap * (segmentCount - 1));
    const CGFloat segmentWidth = available / static_cast<CGFloat>(segmentCount);
    const NSInteger litCount = static_cast<NSInteger>(std::ceil(level * static_cast<double>(segmentCount)));

    for (NSInteger i = 0; i < segmentCount; ++i)
    {
        const CGFloat normalized = static_cast<CGFloat>(i) / static_cast<CGFloat>(segmentCount - 1);
        NSColor* activeColor = normalized > 0.90 ? CPRed() : (normalized > 0.73 ? CPAmber() : CPLive());
        NSColor* color = i < litCount ? activeColor : CPColor(0.070, 0.082, 0.087);
        NSRect segmentRect = NSMakeRect(inset + static_cast<CGFloat>(i) * (segmentWidth + gap),
                                        3.0,
                                        segmentWidth,
                                        std::max<CGFloat>(1.0, NSHeight(view.bounds) - 6.0));
        if (i < litCount)
        {
            [NSGraphicsContext saveGraphicsState];
            NSShadow* shadow = [[NSShadow alloc] init];
            shadow.shadowColor = [activeColor colorWithAlphaComponent:0.28];
            shadow.shadowBlurRadius = 3.5;
            shadow.shadowOffset = NSZeroSize;
            [shadow set];
            [color setFill];
            NSRectFill(segmentRect);
            [NSGraphicsContext restoreGraphicsState];
        }
        else
        {
            [color setFill];
            NSRectFill(segmentRect);
        }
    }

    [[NSColor colorWithWhite:1.0 alpha:0.06] setFill];
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
    } @catch (__unused NSException* exception) {}

    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* well = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:9.0 yRadius:9.0];
    NSGradient* wellGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.025, 0.031, 0.036)
                                                              endingColor:CPColor(0.010, 0.014, 0.018)];
    [wellGradient drawInBezierPath:well angle:90.0];
    [CPColor(0.100, 0.120, 0.138) setStroke];
    well.lineWidth = 1.0;
    [well stroke];

    const CGFloat midY = NSHeight(view.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(view.bounds) - 42.0;

    NSBezierPath* cableShadow = [NSBezierPath bezierPath];
    [cableShadow moveToPoint:NSMakePoint(leftX, midY - 1.0)];
    [cableShadow lineToPoint:NSMakePoint(rightX, midY - 1.0)];
    [CPColor(0.0, 0.0, 0.0, 0.75) setStroke];
    cableShadow.lineWidth = 7.0;
    [cableShadow stroke];

    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    [CPColor(0.095, 0.108, 0.116) setStroke];
    cable.lineWidth = 5.0;
    [cable stroke];
    [CPColor(0.27, 0.30, 0.32, 0.55) setStroke];
    cable.lineWidth = 1.0;
    [cable stroke];

    const CGFloat cardWidth = std::min<CGFloat>(220.0, NSWidth(view.bounds) * 0.34);
    const NSRect cardRect = NSMakeRect((NSWidth(view.bounds) - cardWidth) * 0.5,
                                       10.0,
                                       cardWidth,
                                       NSHeight(view.bounds) - 20.0);

    if (!bypassed)
    {
        NSGradient* halo = [[NSGradient alloc] initWithStartingColor:[CPWarm() colorWithAlphaComponent:0.20]
                                                         endingColor:[CPWarm() colorWithAlphaComponent:0.0]];
        [halo drawFromCenter:NSMakePoint(NSMidX(cardRect), NSMidY(cardRect))
                      radius:0.0
                    toCenter:NSMakePoint(NSMidX(cardRect), NSMidY(cardRect))
                      radius:std::max(NSWidth(cardRect), NSHeight(cardRect)) * 0.70
                     options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    }

    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:cardRect xRadius:8.0 yRadius:8.0];
    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.78];
    shadow.shadowBlurRadius = 12.0;
    shadow.shadowOffset = NSMakeSize(0.0, -5.0);
    [shadow set];
    [CPColor(0.07, 0.075, 0.078) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* bodyGradient = [[NSGradient alloc]
        initWithStartingColor:(bypassed ? CPColor(0.12, 0.13, 0.14) : CPColor(0.245, 0.082, 0.060))
        endingColor:(bypassed ? CPColor(0.06, 0.07, 0.08) : CPColor(0.105, 0.035, 0.030))];
    [bodyGradient drawInBezierPath:body angle:90.0];
    [(bypassed ? CPBorder() : [CPWarm() colorWithAlphaComponent:0.82]) setStroke];
    body.lineWidth = bypassed ? 1.0 : 1.4;
    [body stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.13] setStroke];
    NSBezierPath* lip = [NSBezierPath bezierPath];
    [lip moveToPoint:NSMakePoint(NSMinX(cardRect) + 9.0, NSMaxY(cardRect) - 4.0)];
    [lip lineToPoint:NSMakePoint(NSMaxX(cardRect) - 9.0, NSMaxY(cardRect) - 4.0)];
    lip.lineWidth = 0.7;
    [lip stroke];

    CPDrawLED(NSMakePoint(NSMidX(cardRect), NSMaxY(cardRect) - 15.0), 2.4, CPWarm(), !bypassed);
    CPDrawFootswitch(NSMakeRect(NSMidX(cardRect) - 9.0, NSMinY(cardRect) + 8.0, 18.0, 18.0), !bypassed);

    NSDictionary* nameAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:11.5 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPText()
    };
    NSSize nameSize = [selectedName sizeWithAttributes:nameAttributes];
    const CGFloat titleX = NSMidX(cardRect) - std::min(nameSize.width, NSWidth(cardRect) - 20.0) * 0.5;
    NSRect titleRect = NSMakeRect(titleX, NSMidY(cardRect) - 7.0, NSWidth(cardRect) - 20.0, 16.0);
    [selectedName drawInRect:titleRect withAttributes:nameAttributes];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:8.8 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPMuted()
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(view.bounds) - 33.0, midY - 5.0) withAttributes:ioAttributes];

    for (NSNumber* side in @[ @(-1), @(1) ])
    {
        const CGFloat x = side.integerValue < 0 ? NSMinX(cardRect) - 4.0 : NSMaxX(cardRect) - 4.0;
        NSBezierPath* jack = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, midY - 4.0, 8.0, 8.0)];
        [CPColor(0.20, 0.21, 0.22) setFill];
        [jack fill];
        [CPColor(0.48, 0.50, 0.50) setStroke];
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
    } @catch (__unused NSException* exception) {}

    NSRect bounds = NSInsetRect(view.bounds, 0.5, 0.5);
    NSBezierPath* stage = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:14.0 yRadius:14.0];
    NSGradient* stageGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.050, 0.026, 0.027)
                                                               endingColor:CPColor(0.009, 0.013, 0.017)];
    [stageGradient drawInBezierPath:stage angle:18.0];

    [NSGraphicsContext saveGraphicsState];
    [stage addClip];
    NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds) + 8.0);
    NSGradient* bloom = [[NSGradient alloc] initWithStartingColor:[CPWarm() colorWithAlphaComponent:(bypassed ? 0.05 : 0.19)]
                                                       endingColor:[CPWarm() colorWithAlphaComponent:0.0]];
    [bloom drawFromCenter:center
                   radius:6.0
                 toCenter:center
                   radius:std::min(NSWidth(bounds), NSHeight(bounds)) * 0.58
                  options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    CPDrawMicroTexture(bounds, 0.016, 185);
    [NSGraphicsContext restoreGraphicsState];

    [CPColor(0.110, 0.132, 0.148) setStroke];
    stage.lineWidth = 1.0;
    [stage stroke];
    NSBezierPath* stageInner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bounds, 1.3, 1.3)
                                                               xRadius:12.7
                                                               yRadius:12.7];
    CPDrawInnerEdge(stageInner);

    const CGFloat enclosureWidth = std::min<CGFloat>(330.0, NSWidth(view.bounds) * 0.48);
    const CGFloat enclosureHeight = std::min<CGFloat>(410.0, NSHeight(view.bounds) - 72.0);
    const NSRect enclosure = NSMakeRect(NSMidX(view.bounds) - enclosureWidth * 0.5,
                                        42.0,
                                        enclosureWidth,
                                        enclosureHeight);
    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:enclosure xRadius:17.0 yRadius:17.0];

    [NSGraphicsContext saveGraphicsState];
    NSShadow* pedalShadow = [[NSShadow alloc] init];
    pedalShadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.82];
    pedalShadow.shadowBlurRadius = 28.0;
    pedalShadow.shadowOffset = NSMakeSize(0.0, -11.0);
    [pedalShadow set];
    [CPColor(0.14, 0.04, 0.032) setFill];
    [body fill];
    [NSGraphicsContext restoreGraphicsState];

    NSGradient* enclosureGradient = [[NSGradient alloc] initWithColors:@[
        CPColor(0.305, 0.090, 0.065),
        CPColor(0.205, 0.052, 0.042),
        CPColor(0.105, 0.027, 0.028)
    ]];
    [enclosureGradient drawInBezierPath:body angle:75.0];

    [NSGraphicsContext saveGraphicsState];
    [body addClip];
    for (NSInteger i = 0; i < 52; ++i)
    {
        const CGFloat x = NSMinX(enclosure) + 10.0 + std::fmod(static_cast<CGFloat>(i * 41), std::max<CGFloat>(1.0, NSWidth(enclosure) - 20.0));
        const CGFloat y = NSMinY(enclosure) + 12.0 + std::fmod(static_cast<CGFloat>(i * 67), std::max<CGFloat>(1.0, NSHeight(enclosure) - 24.0));
        const CGFloat width = 3.0 + static_cast<CGFloat>(i % 8);
        [[NSColor colorWithWhite:(i % 2 == 0 ? 1.0 : 0.0) alpha:(i % 2 == 0 ? 0.035 : 0.08)] setStroke];
        NSBezierPath* scratch = [NSBezierPath bezierPath];
        [scratch moveToPoint:NSMakePoint(x, y)];
        [scratch lineToPoint:NSMakePoint(x + width, y + (i % 3 == 0 ? 1.0 : -0.5))];
        scratch.lineWidth = 0.55;
        [scratch stroke];
    }
    CPDrawMicroTexture(enclosure, 0.025, 95);
    [NSGraphicsContext restoreGraphicsState];

    [(bypassed ? CPBorder() : [CPWarm() colorWithAlphaComponent:0.72]) setStroke];
    body.lineWidth = bypassed ? 1.15 : 1.55;
    [body stroke];

    NSBezierPath* innerBody = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(enclosure, 3.0, 3.0)
                                                               xRadius:14.0
                                                               yRadius:14.0];
    [[NSColor colorWithWhite:1.0 alpha:0.095] setStroke];
    innerBody.lineWidth = 0.8;
    [innerBody stroke];

    for (NSNumber* side in @[ @(-1), @(1) ])
    {
        const CGFloat x = side.integerValue < 0 ? NSMinX(enclosure) - 6.0 : NSMaxX(enclosure) - 2.0;
        NSRect jackRect = NSMakeRect(x, NSMidY(enclosure) + 22.0, 8.0, 28.0);
        NSBezierPath* jack = [NSBezierPath bezierPathWithRoundedRect:jackRect xRadius:3.0 yRadius:3.0];
        NSGradient* jackGradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.50, 0.51, 0.51)
                                                                  endingColor:CPColor(0.12, 0.13, 0.14)];
        [jackGradient drawInBezierPath:jack angle:(side.integerValue < 0 ? 0.0 : 180.0)];
    }

    CPDrawLED(NSMakePoint(NSMidX(enclosure), NSMaxY(enclosure) - 34.0), 4.0, CPWarm(), !bypassed);

    NSDictionary* brandAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:9.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: CPColor(0.78, 0.79, 0.80)
    };
    NSString* bodyBrand = @"CIRCUITPEDAL";
    NSSize bodyBrandSize = [bodyBrand sizeWithAttributes:brandAttributes];
    [bodyBrand drawAtPoint:NSMakePoint(NSMidX(enclosure) - bodyBrandSize.width * 0.5, NSMinY(enclosure) + 104.0)
            withAttributes:brandAttributes];

    NSDictionary* markAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:18.0 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: CPColor(0.86, 0.86, 0.85)
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
    const CGFloat titleX = std::max<CGFloat>(24.0, NSMidX(view.bounds) - titleSize.width * 0.5);
    [pedalName drawAtPoint:NSMakePoint(titleX, NSHeight(view.bounds) - 36.0)
            withAttributes:titleAttributes];

    NSDictionary* typeAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: CPMuted(),
        NSKernAttributeName: @0.75
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
    NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:CPColor(0.018, 0.024, 0.029)
                                                          endingColor:CPColor(0.007, 0.010, 0.013)];
    [gradient drawInBezierPath:background angle:90.0];
    [CPColor(0.105, 0.128, 0.144) setStroke];
    background.lineWidth = 0.8;
    [background stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.035] setStroke];
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
    glow.shadowColor = [CPSteel() colorWithAlphaComponent:0.28];
    glow.shadowBlurRadius = 5.0;
    glow.shadowOffset = NSZeroSize;
    [glow set];
    [CPSteel() setStroke];
    response.lineWidth = 1.6;
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
    NSBezierPath* inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(view.bounds, 0.5, 0.5)
                                                          xRadius:8.0
                                                          yRadius:8.0];
    [CPColor(0.130, 0.170, 0.195, 0.65) setStroke];
    inner.lineWidth = 1.0;
    [inner stroke];

    NSGradient* vignette = [[NSGradient alloc] initWithStartingColor:CPColor(0.0, 0.0, 0.0, 0.0)
                                                          endingColor:CPColor(0.0, 0.0, 0.0, 0.26)];
    [vignette drawInRect:NSMakeRect(0.0, 0.0, NSWidth(view.bounds), 38.0) angle:-90.0];
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

void CPInstallSwizzles()
{
    static BOOL installed = NO;
    if (installed)
        return;

    Class panelClass = NSClassFromString(@"CircuitPedalPanelView");
    Class heroClass = NSClassFromString(@"CircuitPedalHeroView");
    Class chainClass = NSClassFromString(@"CircuitPedalChainView");
    Class meterClass = NSClassFromString(@"CircuitPedalMeterView");
    Class dotClass = NSClassFromString(@"CircuitPedalStatusDotView");
    Class eqClass = NSClassFromString(@"CircuitPedalEQPreviewView");
    Class routingClass = NSClassFromString(@"CircuitPedalRoutingEditorView");
    if (panelClass == Nil || heroClass == Nil || chainClass == Nil || meterClass == Nil || dotClass == Nil || eqClass == Nil || routingClass == Nil)
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

BOOL CPColorLooksSelected(NSColor* color)
{
    if (color == nil)
        return NO;
    NSColor* rgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (rgb == nil)
        return NO;
    return rgb.blueComponent > 0.62 && rgb.greenComponent > 0.45 && rgb.redComponent < 0.55;
}

} // namespace

@interface CPPremiumKnobCell : NSSliderCell
@end

@implementation CPPremiumKnobCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    (void)controlView;
    const CGFloat side = std::min(NSWidth(cellFrame), NSHeight(cellFrame)) - 4.0;
    const NSRect knobRect = NSMakeRect(NSMidX(cellFrame) - side * 0.5,
                                       NSMidY(cellFrame) - side * 0.5,
                                       side,
                                       side);
    const NSPoint center = NSMakePoint(NSMidX(knobRect), NSMidY(knobRect));
    const CGFloat radius = side * 0.5;
    const double range = self.maxValue - self.minValue;
    const CGFloat normalized = range > 0.0
        ? CPClamp(static_cast<CGFloat>((self.doubleValue - self.minValue) / range), 0.0, 1.0)
        : 0.0;

    NSBezierPath* arcBackground = [NSBezierPath bezierPath];
    [arcBackground appendBezierPathWithArcWithCenter:center radius:radius + 0.3 startAngle:-135.0 endAngle:135.0 clockwise:NO];
    [CPColor(0.13, 0.15, 0.16) setStroke];
    arcBackground.lineWidth = 2.1;
    [arcBackground stroke];

    NSBezierPath* valueArc = [NSBezierPath bezierPath];
    [valueArc appendBezierPathWithArcWithCenter:center
                                         radius:radius + 0.3
                                     startAngle:-135.0
                                       endAngle:-135.0 + 270.0 * normalized
                                      clockwise:NO];
    [[CPSteel() colorWithAlphaComponent:0.72] setStroke];
    valueArc.lineWidth = 1.5;
    [valueArc stroke];

    [NSGraphicsContext saveGraphicsState];
    NSShadow* shadow = [[NSShadow alloc] init];
    shadow.shadowColor = [NSColor colorWithWhite:0.0 alpha:0.78];
    shadow.shadowBlurRadius = 7.0;
    shadow.shadowOffset = NSMakeSize(0.0, -3.0);
    [shadow set];
    NSBezierPath* outer = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knobRect, 2.8, 2.8)];
    [CPColor(0.055, 0.062, 0.068) setFill];
    [outer fill];
    [NSGraphicsContext restoreGraphicsState];

    NSRect faceRect = NSInsetRect(knobRect, 4.5, 4.5);
    NSBezierPath* face = [NSBezierPath bezierPathWithOvalInRect:faceRect];
    [NSGraphicsContext saveGraphicsState];
    [face addClip];
    NSGradient* faceGradient = [[NSGradient alloc] initWithColors:@[
        CPColor(0.30, 0.33, 0.35),
        CPColor(0.105, 0.120, 0.130),
        CPColor(0.030, 0.036, 0.040)
    ]];
    [faceGradient drawFromCenter:NSMakePoint(NSMidX(faceRect) - radius * 0.24,
                                             NSMidY(faceRect) + radius * 0.28)
                         radius:0.0
                       toCenter:center
                         radius:radius * 0.86
                        options:NSGradientDrawsBeforeStartingLocation | NSGradientDrawsAfterEndingLocation];
    [NSGraphicsContext restoreGraphicsState];
    [CPColor(0.33, 0.36, 0.38) setStroke];
    face.lineWidth = 0.9;
    [face stroke];

    NSBezierPath* innerRing = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(faceRect, 3.0, 3.0)];
    [[NSColor colorWithWhite:1.0 alpha:0.055] setStroke];
    innerRing.lineWidth = 0.8;
    [innerRing stroke];

    const CGFloat angle = (-135.0 + 270.0 * normalized) * static_cast<CGFloat>(M_PI / 180.0);
    const CGFloat markerInner = radius * 0.25;
    const CGFloat markerOuter = radius * 0.60;
    NSBezierPath* marker = [NSBezierPath bezierPath];
    [marker moveToPoint:NSMakePoint(center.x + std::cos(angle) * markerInner,
                                    center.y + std::sin(angle) * markerInner)];
    [marker lineToPoint:NSMakePoint(center.x + std::cos(angle) * markerOuter,
                                    center.y + std::sin(angle) * markerOuter)];
    [CPColor(0.91, 0.93, 0.93) setStroke];
    marker.lineWidth = 2.0;
    marker.lineCapStyle = NSLineCapStyleRound;
    [marker stroke];
}

@end

@interface CPPremiumButtonCell : NSButtonCell
@end

@implementation CPPremiumButtonCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
    NSButton* button = [controlView isKindOfClass:NSButton.class] ? (NSButton*)controlView : nil;
    const BOOL selected = button != nil && (button.state == NSControlStateValueOn || CPColorLooksSelected(button.contentTintColor));
    const BOOL liveButton = button != nil && ([button.title isEqualToString:@"START AUDIO"] || [button.title isEqualToString:@"ACTIVE"]);
    const BOOL enabled = self.enabled;

    NSRect rect = NSInsetRect(cellFrame, 1.0, 1.0);
    NSBezierPath* bezel = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:6.0 yRadius:6.0];
    NSGradient* gradient = [[NSGradient alloc]
        initWithStartingColor:(selected ? CPColor(0.075, 0.105, 0.122) : CPGraphite3())
        endingColor:(selected ? CPColor(0.035, 0.052, 0.064) : CPGraphite1())];
    [gradient drawInBezierPath:bezel angle:90.0];

    NSColor* border = selected ? [CPSteel() colorWithAlphaComponent:0.58] : CPColor(0.145, 0.170, 0.188);
    [border setStroke];
    bezel.lineWidth = 0.9;
    [bezel stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.055] setStroke];
    NSBezierPath* top = [NSBezierPath bezierPath];
    [top moveToPoint:NSMakePoint(NSMinX(rect) + 6.0, NSMaxY(rect) - 2.0)];
    [top lineToPoint:NSMakePoint(NSMaxX(rect) - 6.0, NSMaxY(rect) - 2.0)];
    top.lineWidth = 0.7;
    [top stroke];

    if (selected)
    {
        NSColor* underlineColor = liveButton ? CPLive() : CPSteel();
        [underlineColor setFill];
        NSRectFill(NSMakeRect(NSMinX(rect) + 7.0, NSMinY(rect) + 1.0, std::max<CGFloat>(0.0, NSWidth(rect) - 14.0), 1.5));
    }

    NSColor* titleColor = !enabled ? CPColor(0.38, 0.42, 0.45) : (liveButton ? CPLive() : (selected ? CPText() : CPColor(0.74, 0.78, 0.81)));
    NSDictionary* attributes = @{
        NSFontAttributeName: self.font ?: [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: titleColor
    };
    NSSize titleSize = [self.title sizeWithAttributes:attributes];
    NSPoint titlePoint = NSMakePoint(NSMidX(rect) - titleSize.width * 0.5,
                                    NSMidY(rect) - titleSize.height * 0.5 + 0.5);
    [self.title drawAtPoint:titlePoint withAttributes:attributes];
}

@end

namespace {

void CPStyleKnob(NSSlider* slider)
{
    if (slider.sliderType != NSSliderTypeCircular || [slider.cell isKindOfClass:CPPremiumKnobCell.class])
        return;

    const double minimum = slider.minValue;
    const double maximum = slider.maxValue;
    const double value = slider.doubleValue;
    CPPremiumKnobCell* cell = [[CPPremiumKnobCell alloc] init];
    cell.sliderType = NSSliderTypeCircular;
    cell.minValue = minimum;
    cell.maxValue = maximum;
    cell.doubleValue = value;
    cell.enabled = slider.enabled;
    slider.cell = cell;
    slider.continuous = YES;
    slider.wantsLayer = YES;
    slider.layer.backgroundColor = NSColor.clearColor.CGColor;
}

void CPStyleButton(NSButton* button)
{
    if ([button isKindOfClass:NSPopUpButton.class] || [button.cell isKindOfClass:CPPremiumButtonCell.class])
        return;

    NSButtonType buttonType = button.buttonType;
    NSControlStateValue state = button.state;
    CPPremiumButtonCell* cell = [[CPPremiumButtonCell alloc] initTextCell:button.title ?: @""];
    cell.font = button.font ?: [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
    button.cell = cell;
    button.buttonType = buttonType;
    button.state = state;
    button.bordered = NO;
}

void CPStylePopup(NSPopUpButton* popup)
{
    popup.wantsLayer = YES;
    popup.bordered = NO;
    popup.layer.backgroundColor = CPColor(0.026, 0.033, 0.039).CGColor;
    popup.layer.borderColor = CPColor(0.145, 0.170, 0.190).CGColor;
    popup.layer.borderWidth = 0.8;
    popup.layer.cornerRadius = 6.0;
    popup.contentTintColor = CPColor(0.78, 0.81, 0.84);
    popup.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
}

void CPStyleSearchField(NSSearchField* search)
{
    search.wantsLayer = YES;
    search.layer.backgroundColor = CPColor(0.022, 0.028, 0.033).CGColor;
    search.layer.borderColor = CPColor(0.130, 0.154, 0.174).CGColor;
    search.layer.borderWidth = 0.8;
    search.layer.cornerRadius = 6.0;
    search.textColor = CPColor(0.76, 0.79, 0.81);
    search.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
}

void CPApplyThemeToView(NSView* view)
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
    {
        NSScrollView* scroll = (NSScrollView*)view;
        scroll.drawsBackground = YES;
        scroll.backgroundColor = CPColor(0.018, 0.024, 0.029);
        scroll.borderType = NSNoBorder;
        scroll.scrollerStyle = NSScrollerStyleOverlay;
    }

    NSString* className = NSStringFromClass(view.class);
    if ([className hasPrefix:@"CircuitPedalPanelView"] || [className isEqualToString:@"CircuitPedalHeroView"])
    {
        view.wantsLayer = YES;
        view.layer.shadowColor = NSColor.blackColor.CGColor;
        view.layer.shadowOpacity = [className isEqualToString:@"CircuitPedalHeroView"] ? 0.24f : 0.14f;
        view.layer.shadowRadius = [className isEqualToString:@"CircuitPedalHeroView"] ? 10.0 : 5.0;
        view.layer.shadowOffset = NSMakeSize(0.0, -2.0);
    }

    for (NSView* subview in view.subviews)
        CPApplyThemeToView(subview);
}

void CPApplyWindowTheme()
{
    CPInstallSwizzles();
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
            CPApplyThemeToView(window.contentView);
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
    CPInstallSwizzles();
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
