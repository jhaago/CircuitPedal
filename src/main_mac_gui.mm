#import <Cocoa/Cocoa.h>

#include "MacAudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

NSString* nsString(const std::string& text)
{
    NSString* converted = [NSString stringWithUTF8String:text.c_str()];
    return converted != nil ? converted : @"Unknown text";
}

NSColor* cpColor(CGFloat r, CGFloat g, CGFloat b)
{
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:1.0];
}

NSColor* backgroundColor() { return cpColor(0.025, 0.031, 0.038); }
NSColor* topBarColor() { return cpColor(0.035, 0.043, 0.052); }
NSColor* panelColor() { return cpColor(0.045, 0.054, 0.064); }
NSColor* panelAltColor() { return cpColor(0.057, 0.067, 0.078); }
NSColor* heroColor() { return cpColor(0.060, 0.066, 0.071); }
NSColor* borderColor() { return cpColor(0.135, 0.157, 0.180); }
NSColor* accentColor() { return cpColor(0.25, 0.73, 0.93); }
NSColor* warmAccentColor() { return cpColor(0.95, 0.39, 0.25); }
NSColor* textColor() { return cpColor(0.92, 0.94, 0.96); }
NSColor* mutedTextColor() { return cpColor(0.50, 0.55, 0.61); }
NSColor* liveColor() { return cpColor(0.24, 0.88, 0.47); }
NSColor* warningColor() { return cpColor(0.96, 0.68, 0.24); }
NSColor* errorColor() { return cpColor(0.96, 0.34, 0.31); }

NSTextField* makeLabel(NSString* text,
                       NSRect frame,
                       CGFloat size = 13.0,
                       NSFontWeight weight = NSFontWeightRegular,
                       NSColor* color = nil)
{
    NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text;
    label.editable = NO;
    label.selectable = NO;
    label.bezeled = NO;
    label.drawsBackground = NO;
    label.font = [NSFont systemFontOfSize:size weight:weight];
    label.textColor = color != nil ? color : textColor();
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    return label;
}

NSTextField* makeSectionLabel(NSString* text, NSRect frame)
{
    NSTextField* label = makeLabel(text,
                                   frame,
                                   10.0,
                                   NSFontWeightSemibold,
                                   mutedTextColor());
    label.font = [NSFont monospacedSystemFontOfSize:10.0
                                            weight:NSFontWeightSemibold];
    return label;
}

NSButton* makeButton(NSString* title, NSRect frame, id target, SEL action)
{
    NSButton* button = [[NSButton alloc] initWithFrame:frame];
    button.title = title;
    button.bezelStyle = NSBezelStyleRounded;
    button.target = target;
    button.action = action;
    button.font = [NSFont systemFontOfSize:11.5 weight:NSFontWeightMedium];
    button.contentTintColor = textColor();
    return button;
}

void stylePopup(NSPopUpButton* popup)
{
    popup.font = [NSFont systemFontOfSize:11.5 weight:NSFontWeightRegular];
    popup.contentTintColor = textColor();
}

void styleRotarySlider(NSSlider* slider)
{
    slider.sliderType = NSSliderTypeCircular;
    slider.minValue = 0.0;
    slider.maxValue = 100.0;
    slider.continuous = YES;
}

NSString* dbText(double peak)
{
    if (!std::isfinite(peak) || peak <= 1.0e-5)
        return @"−∞ dB";
    const double db = std::max(-60.0, 20.0 * std::log10(peak));
    return [NSString stringWithFormat:@"%.1f dB", db];
}

} // namespace

@interface CircuitPedalPanelView : NSView
@property(strong) NSColor* fillColor;
@property(strong) NSColor* strokeColor;
@property CGFloat cornerRadius;
@end

@implementation CircuitPedalPanelView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        _fillColor = panelColor();
        _strokeColor = borderColor();
        _cornerRadius = 10.0;
        self.wantsLayer = YES;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                        xRadius:self.cornerRadius
                                                        yRadius:self.cornerRadius];
    [self.fillColor setFill];
    [path fill];
    [self.strokeColor setStroke];
    path.lineWidth = 1.0;
    [path stroke];

    NSRect topLine = NSMakeRect(12.0,
                                NSHeight(self.bounds) - 1.0,
                                std::max<CGFloat>(0.0, NSWidth(self.bounds) - 24.0),
                                1.0);
    [[NSColor colorWithWhite:1.0 alpha:0.025] setFill];
    NSRectFill(topLine);
}

@end

@interface CircuitPedalStatusDotView : NSView
@property(nonatomic) BOOL active;
@end

@implementation CircuitPedalStatusDotView
- (BOOL)isOpaque { return NO; }
- (void)setActive:(BOOL)active
{
    _active = active;
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    NSRect dotRect = NSInsetRect(self.bounds, 2.0, 2.0);
    NSBezierPath* glow = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dotRect, -2.0, -2.0)];
    [[self.active ? [liveColor() colorWithAlphaComponent:0.18]
                  : [NSColor colorWithWhite:0.3 alpha:0.08]] setFill];
    [glow fill];
    NSBezierPath* dot = [NSBezierPath bezierPathWithOvalInRect:dotRect];
    [(self.active ? liveColor() : cpColor(0.25, 0.28, 0.31)) setFill];
    [dot fill];
}
@end

@interface CircuitPedalMeterView : NSView
@property(nonatomic) double level;
@end

@implementation CircuitPedalMeterView
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
        _level = 0.0;
    return self;
}
- (BOOL)isOpaque { return NO; }
- (void)setLevel:(double)level
{
    _level = std::clamp(level, 0.0, 1.0);
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    const NSInteger segments = 20;
    const CGFloat gap = 2.0;
    const CGFloat width = (NSWidth(self.bounds) - gap * (segments - 1)) / segments;
    const NSInteger lit = static_cast<NSInteger>(std::ceil(self.level * segments));
    for (NSInteger i = 0; i < segments; ++i)
    {
        NSColor* color = cpColor(0.10, 0.12, 0.14);
        if (i < lit)
        {
            const double normalized = static_cast<double>(i) / static_cast<double>(segments - 1);
            if (normalized > 0.88)
                color = errorColor();
            else if (normalized > 0.70)
                color = warningColor();
            else
                color = liveColor();
        }
        const NSRect segment = NSMakeRect(static_cast<CGFloat>(i) * (width + gap),
                                          1.0,
                                          width,
                                          NSHeight(self.bounds) - 2.0);
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:segment
                                                            xRadius:1.5
                                                            yRadius:1.5];
        [color setFill];
        [path fill];
    }
}
@end

@interface CircuitPedalHistoryView : NSView {
@private
    double _history[96];
    NSUInteger _cursor;
    BOOL _filled;
}
- (void)pushLevel:(double)level;
@end

@implementation CircuitPedalHistoryView
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        std::fill(std::begin(_history), std::end(_history), 0.0);
        _cursor = 0;
        _filled = NO;
    }
    return self;
}
- (BOOL)isOpaque { return NO; }
- (void)pushLevel:(double)level
{
    _history[_cursor] = std::clamp(level, 0.0, 1.0);
    _cursor = (_cursor + 1U) % 96U;
    if (_cursor == 0U)
        _filled = YES;
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    [[NSColor colorWithWhite:0.02 alpha:0.55] setFill];
    NSBezierPath* background = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                               xRadius:6.0
                                                               yRadius:6.0];
    [background fill];

    [[NSColor colorWithWhite:1.0 alpha:0.045] setStroke];
    for (NSInteger i = 1; i < 4; ++i)
    {
        const CGFloat y = NSHeight(self.bounds) * static_cast<CGFloat>(i) / 4.0;
        NSBezierPath* grid = [NSBezierPath bezierPath];
        [grid moveToPoint:NSMakePoint(0.0, y)];
        [grid lineToPoint:NSMakePoint(NSWidth(self.bounds), y)];
        grid.lineWidth = 1.0;
        [grid stroke];
    }

    const NSUInteger count = _filled ? 96U : _cursor;
    if (count < 2U)
        return;

    NSBezierPath* trace = [NSBezierPath bezierPath];
    for (NSUInteger i = 0; i < count; ++i)
    {
        const NSUInteger index = _filled ? ((_cursor + i) % 96U) : i;
        const CGFloat x = NSWidth(self.bounds) * static_cast<CGFloat>(i)
            / static_cast<CGFloat>(std::max<NSUInteger>(1U, count - 1U));
        const CGFloat y = 6.0 + (NSHeight(self.bounds) - 12.0)
            * static_cast<CGFloat>(_history[index]);
        if (i == 0U)
            [trace moveToPoint:NSMakePoint(x, y)];
        else
            [trace lineToPoint:NSMakePoint(x, y)];
    }
    [accentColor() setStroke];
    trace.lineWidth = 1.5;
    [trace stroke];
}
@end

@interface CircuitPedalChainView : NSView
@property(copy) NSString* selectedName;
@property(nonatomic) BOOL bypassed;
@end

@implementation CircuitPedalChainView
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        _selectedName = @"Built-in Distortion+";
        _bypassed = NO;
    }
    return self;
}
- (BOOL)isOpaque { return NO; }
- (void)setSelectedName:(NSString*)selectedName
{
    _selectedName = [selectedName copy];
    self.needsDisplay = YES;
}
- (void)setBypassed:(BOOL)bypassed
{
    _bypassed = bypassed;
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    const CGFloat midY = NSHeight(self.bounds) * 0.48;
    const CGFloat leftX = 42.0;
    const CGFloat rightX = NSWidth(self.bounds) - 42.0;

    [cpColor(0.19, 0.23, 0.27) setStroke];
    NSBezierPath* cable = [NSBezierPath bezierPath];
    [cable moveToPoint:NSMakePoint(leftX, midY)];
    [cable lineToPoint:NSMakePoint(rightX, midY)];
    cable.lineWidth = 4.0;
    [cable stroke];

    const CGFloat cardWidth = std::min<CGFloat>(220.0, NSWidth(self.bounds) * 0.34);
    const NSRect cardRect = NSMakeRect((NSWidth(self.bounds) - cardWidth) * 0.5,
                                       14.0,
                                       cardWidth,
                                       NSHeight(self.bounds) - 28.0);
    NSBezierPath* card = [NSBezierPath bezierPathWithRoundedRect:cardRect
                                                         xRadius:9.0
                                                         yRadius:9.0];
    [(self.bypassed ? panelAltColor() : cpColor(0.085, 0.10, 0.115)) setFill];
    [card fill];
    [(self.bypassed ? borderColor() : accentColor()) setStroke];
    card.lineWidth = self.bypassed ? 1.0 : 1.8;
    [card stroke];

    NSDictionary* nameAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:12.5 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: textColor()
    };
    NSString* name = self.selectedName.length > 0 ? self.selectedName : @"Current Pedal";
    NSSize nameSize = [name sizeWithAttributes:nameAttributes];
    [name drawAtPoint:NSMakePoint(NSMidX(cardRect) - nameSize.width * 0.5,
                                  NSMidY(cardRect) - 5.0)
         withAttributes:nameAttributes];

    NSDictionary* ioAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.5 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: mutedTextColor()
    };
    [@"IN" drawAtPoint:NSMakePoint(12.0, midY - 5.0) withAttributes:ioAttributes];
    [@"OUT" drawAtPoint:NSMakePoint(NSWidth(self.bounds) - 32.0, midY - 5.0)
          withAttributes:ioAttributes];
}
@end

@interface CircuitPedalHeroView : CircuitPedalPanelView
@property(copy) NSString* pedalName;
@property(copy) NSString* pedalType;
@property(nonatomic) BOOL bypassed;
@end

@implementation CircuitPedalHeroView
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        self.fillColor = heroColor();
        self.strokeColor = cpColor(0.18, 0.21, 0.24);
        self.cornerRadius = 14.0;
        _pedalName = @"Built-in Distortion+";
        _pedalType = @"REFERENCE CIRCUIT MODEL";
        _bypassed = NO;
    }
    return self;
}
- (void)setPedalName:(NSString*)pedalName
{
    _pedalName = [pedalName copy];
    self.needsDisplay = YES;
}
- (void)setPedalType:(NSString*)pedalType
{
    _pedalType = [pedalType copy];
    self.needsDisplay = YES;
}
- (void)setBypassed:(BOOL)bypassed
{
    _bypassed = bypassed;
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    const CGFloat enclosureWidth = std::min<CGFloat>(300.0, NSWidth(self.bounds) * 0.46);
    const CGFloat enclosureHeight = std::min<CGFloat>(380.0, NSHeight(self.bounds) - 80.0);
    const NSRect enclosure = NSMakeRect(NSMidX(self.bounds) - enclosureWidth * 0.5,
                                        42.0,
                                        enclosureWidth,
                                        enclosureHeight);
    NSBezierPath* body = [NSBezierPath bezierPathWithRoundedRect:enclosure
                                                         xRadius:18.0
                                                         yRadius:18.0];
    [cpColor(0.09, 0.10, 0.11) setFill];
    [body fill];
    [(self.bypassed ? borderColor() : warmAccentColor()) setStroke];
    body.lineWidth = self.bypassed ? 1.3 : 2.0;
    [body stroke];

    [[NSColor colorWithWhite:1.0 alpha:0.045] setStroke];
    NSBezierPath* highlight = [NSBezierPath bezierPath];
    [highlight moveToPoint:NSMakePoint(NSMinX(enclosure) + 18.0, NSMaxY(enclosure) - 22.0)];
    [highlight lineToPoint:NSMakePoint(NSMaxX(enclosure) - 18.0, NSMaxY(enclosure) - 22.0)];
    [highlight stroke];

    const NSRect ledRect = NSMakeRect(NSMidX(enclosure) - 5.0,
                                      NSMaxY(enclosure) - 54.0,
                                      10.0,
                                      10.0);
    NSBezierPath* led = [NSBezierPath bezierPathWithOvalInRect:ledRect];
    [(self.bypassed ? cpColor(0.22, 0.23, 0.24) : warmAccentColor()) setFill];
    [led fill];

    NSDictionary* titleAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:24.0 weight:NSFontWeightBold],
        NSForegroundColorAttributeName: textColor()
    };
    NSString* title = self.pedalName.length > 0 ? self.pedalName : @"CircuitPedal";
    NSSize titleSize = [title sizeWithAttributes:titleAttributes];
    const CGFloat titleX = std::max<CGFloat>(24.0, NSMidX(self.bounds) - titleSize.width * 0.5);
    [title drawAtPoint:NSMakePoint(titleX, NSHeight(self.bounds) - 36.0)
        withAttributes:titleAttributes];

    NSDictionary* typeAttributes = @{
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.5 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: mutedTextColor()
    };
    NSString* type = self.pedalType.length > 0 ? self.pedalType : @"LIVE CIRCUIT MODEL";
    NSSize typeSize = [type sizeWithAttributes:typeAttributes];
    [type drawAtPoint:NSMakePoint(NSMidX(self.bounds) - typeSize.width * 0.5,
                                  NSHeight(self.bounds) - 57.0)
          withAttributes:typeAttributes];

    NSDictionary* markAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:15.0 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: cpColor(0.72, 0.75, 0.79)
    };
    NSString* mark = @"CIRCUITPEDAL";
    NSSize markSize = [mark sizeWithAttributes:markAttributes];
    [mark drawAtPoint:NSMakePoint(NSMidX(enclosure) - markSize.width * 0.5,
                                  NSMinY(enclosure) + 80.0)
          withAttributes:markAttributes];

    NSRect footRect = NSMakeRect(NSMidX(enclosure) - 22.0,
                                 NSMinY(enclosure) + 24.0,
                                 44.0,
                                 44.0);
    NSBezierPath* foot = [NSBezierPath bezierPathWithOvalInRect:footRect];
    [cpColor(0.21, 0.22, 0.23) setFill];
    [foot fill];
    [cpColor(0.50, 0.52, 0.54) setStroke];
    foot.lineWidth = 2.0;
    [foot stroke];
}
@end

@interface CircuitPedalFlippedView : NSView
@end
@implementation CircuitPedalFlippedView
- (BOOL)isFlipped { return YES; }
@end

@interface CircuitPedalAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> {
@private
    std::unique_ptr<circuitpedal::MacAudioEngine> _engine;
    std::vector<circuitpedal::AudioDeviceInfo> _devices;
    std::vector<circuitpedal::CircuitFileControl> _circuitControls;
    std::vector<std::string> _circuitLibraryPaths;

    NSWindow* _window;
    CircuitPedalPanelView* _topBar;
    CircuitPedalPanelView* _leftPanel;
    CircuitPedalPanelView* _workspacePanel;
    CircuitPedalPanelView* _rightPanel;
    CircuitPedalPanelView* _bottomPanel;
    CircuitPedalChainView* _chainView;
    CircuitPedalHeroView* _heroView;
    CircuitPedalHistoryView* _historyView;

    NSTextField* _topModelLabel;
    CircuitPedalStatusDotView* _liveDot;
    NSTextField* _liveText;
    NSTextField* _sampleRateLabel;
    NSTextField* _bufferStatusLabel;
    NSTextField* _latencyLabel;
    NSTextField* _deviceSummaryLabel;

    NSTextField* _modelValue;
    NSPopUpButton* _circuitLibraryPopup;
    NSButton* _builtinButton;
    NSSearchField* _searchField;

    NSPopUpButton* _devicePopup;
    NSPopUpButton* _channelPopup;
    NSPopUpButton* _bufferPopup;
    NSButton* _startButton;
    NSButton* _stopButton;

    NSTextField* _diodeLabel;
    NSPopUpButton* _diodePopup;
    NSTextField* _distortionLabel;
    NSSlider* _distortionSlider;
    NSTextField* _distortionValue;
    NSTextField* _outputControlLabel;
    NSSlider* _outputSlider;
    NSTextField* _outputValue;

    NSScrollView* _circuitScrollView;
    CircuitPedalFlippedView* _circuitDocumentView;
    NSTextField* _circuitLabels[16];
    NSSlider* _circuitSliders[16];
    NSPopUpButton* _circuitSwitchPopups[16];
    NSTextField* _circuitValues[16];

    NSButton* _bypassButton;
    CircuitPedalMeterView* _inputMeter;
    CircuitPedalMeterView* _outputMeter;
    NSTextField* _inputDbLabel;
    NSTextField* _outputDbLabel;
    NSTextField* _statusLabel;
    NSTextField* _errorLabel;
    NSTimer* _meterTimer;
}
@end

@implementation CircuitPedalAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    (void)notification;
    _engine = std::make_unique<circuitpedal::MacAudioEngine>();

    const NSRect windowRect = NSMakeRect(0.0, 0.0, 1360.0, 820.0);
    const NSWindowStyleMask style = NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable
        | NSWindowStyleMaskMiniaturizable
        | NSWindowStyleMaskResizable;
    _window = [[NSWindow alloc] initWithContentRect:windowRect
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"CircuitPedal — Circuit Lab";
    _window.releasedWhenClosed = NO;
    _window.delegate = self;
    _window.minSize = NSMakeSize(1180.0, 720.0);
    _window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    _window.backgroundColor = backgroundColor();

    NSView* content = _window.contentView;
    content.wantsLayer = YES;
    content.layer.backgroundColor = backgroundColor().CGColor;

    _topBar = [[CircuitPedalPanelView alloc] initWithFrame:NSMakeRect(0.0, 748.0, 1360.0, 72.0)];
    _topBar.cornerRadius = 0.0;
    _topBar.fillColor = topBarColor();
    _topBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:_topBar];

    NSTextField* brand = makeLabel(@"CircuitPedal",
                                   NSMakeRect(22.0, 30.0, 210.0, 30.0),
                                   24.0,
                                   NSFontWeightSemibold);
    [_topBar addSubview:brand];
    NSTextField* brandTag = makeLabel(@"TONE LIVES HERE",
                                      NSMakeRect(24.0, 15.0, 180.0, 14.0),
                                      8.5,
                                      NSFontWeightMedium,
                                      mutedTextColor());
    brandTag.font = [NSFont monospacedSystemFontOfSize:8.5 weight:NSFontWeightMedium];
    [_topBar addSubview:brandTag];

    _topModelLabel = makeLabel(@"Built-in Distortion+",
                               NSMakeRect(410.0, 32.0, 420.0, 24.0),
                               14.0,
                               NSFontWeightSemibold);
    _topModelLabel.alignment = NSTextAlignmentCenter;
    _topModelLabel.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin;
    [_topBar addSubview:_topModelLabel];
    NSTextField* topMode = makeLabel(@"CIRCUIT LAB  •  LIVE R&D",
                                     NSMakeRect(410.0, 15.0, 420.0, 14.0),
                                     8.5,
                                     NSFontWeightMedium,
                                     mutedTextColor());
    topMode.alignment = NSTextAlignmentCenter;
    topMode.font = [NSFont monospacedSystemFontOfSize:8.5 weight:NSFontWeightMedium];
    topMode.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin;
    [_topBar addSubview:topMode];

    _liveDot = [[CircuitPedalStatusDotView alloc] initWithFrame:NSMakeRect(900.0, 39.0, 13.0, 13.0)];
    _liveDot.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_liveDot];
    _liveText = makeLabel(@"AUDIO STOPPED",
                          NSMakeRect(919.0, 34.0, 118.0, 22.0),
                          9.5,
                          NSFontWeightSemibold,
                          mutedTextColor());
    _liveText.font = [NSFont monospacedSystemFontOfSize:9.5 weight:NSFontWeightSemibold];
    _liveText.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_liveText];

    _sampleRateLabel = makeLabel(@"-- kHz", NSMakeRect(1040.0, 34.0, 70.0, 22.0), 9.5, NSFontWeightMedium, mutedTextColor());
    _sampleRateLabel.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_sampleRateLabel];
    _bufferStatusLabel = makeLabel(@"-- smp", NSMakeRect(1112.0, 34.0, 72.0, 22.0), 9.5, NSFontWeightMedium, mutedTextColor());
    _bufferStatusLabel.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_bufferStatusLabel];
    _latencyLabel = makeLabel(@"-- ms", NSMakeRect(1186.0, 34.0, 64.0, 22.0), 9.5, NSFontWeightMedium, mutedTextColor());
    _latencyLabel.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_latencyLabel];
    _deviceSummaryLabel = makeLabel(@"No device",
                                    NSMakeRect(900.0, 15.0, 330.0, 16.0),
                                    9.0,
                                    NSFontWeightRegular,
                                    mutedTextColor());
    _deviceSummaryLabel.autoresizingMask = NSViewMinXMargin;
    [_topBar addSubview:_deviceSummaryLabel];

    _leftPanel = [[CircuitPedalPanelView alloc] initWithFrame:NSMakeRect(12.0, 124.0, 260.0, 612.0)];
    _leftPanel.autoresizingMask = NSViewHeightSizable | NSViewMaxXMargin;
    [content addSubview:_leftPanel];

    NSTextField* libraryTitle = makeSectionLabel(@"LIBRARY", NSMakeRect(16.0, 574.0, 120.0, 18.0));
    libraryTitle.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:libraryTitle];

    NSButton* pedalsTab = makeButton(@"Pedals", NSMakeRect(14.0, 536.0, 72.0, 28.0), nil, nil);
    pedalsTab.enabled = NO;
    pedalsTab.contentTintColor = accentColor();
    pedalsTab.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:pedalsTab];
    NSButton* presetsTab = makeButton(@"Presets", NSMakeRect(91.0, 536.0, 72.0, 28.0), nil, nil);
    presetsTab.enabled = NO;
    presetsTab.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:presetsTab];
    NSButton* favoritesTab = makeButton(@"Favorites", NSMakeRect(168.0, 536.0, 78.0, 28.0), nil, nil);
    favoritesTab.enabled = NO;
    favoritesTab.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:favoritesTab];

    _searchField = [[NSSearchField alloc] initWithFrame:NSMakeRect(14.0, 497.0, 232.0, 28.0)];
    _searchField.placeholderString = @"Search pedals (Phase 2)";
    _searchField.enabled = NO;
    _searchField.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:_searchField];

    _modelValue = makeLabel(@"Built-in Distortion+",
                            NSMakeRect(16.0, 455.0, 228.0, 34.0),
                            14.5,
                            NSFontWeightSemibold);
    _modelValue.usesSingleLineMode = NO;
    _modelValue.lineBreakMode = NSLineBreakByWordWrapping;
    _modelValue.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:_modelValue];

    _circuitLibraryPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(14.0, 416.0, 232.0, 30.0) pullsDown:NO];
    _circuitLibraryPopup.target = self;
    _circuitLibraryPopup.action = @selector(circuitLibraryChanged:);
    _circuitLibraryPopup.autoresizingMask = NSViewMinYMargin;
    stylePopup(_circuitLibraryPopup);
    [_leftPanel addSubview:_circuitLibraryPopup];

    _builtinButton = makeButton(@"Use Built-in Distortion+",
                                NSMakeRect(14.0, 378.0, 232.0, 30.0),
                                self,
                                @selector(useBuiltin:));
    _builtinButton.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:_builtinButton];

    NSTextField* ioTitle = makeSectionLabel(@"AUDIO I/O", NSMakeRect(16.0, 330.0, 120.0, 18.0));
    ioTitle.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:ioTitle];

    _devicePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(14.0, 292.0, 232.0, 30.0) pullsDown:NO];
    _devicePopup.target = self;
    _devicePopup.action = @selector(deviceChanged:);
    _devicePopup.autoresizingMask = NSViewMinYMargin;
    stylePopup(_devicePopup);
    [_leftPanel addSubview:_devicePopup];

    _channelPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(14.0, 254.0, 112.0, 30.0) pullsDown:NO];
    _channelPopup.autoresizingMask = NSViewMinYMargin;
    stylePopup(_channelPopup);
    [_leftPanel addSubview:_channelPopup];

    _bufferPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(134.0, 254.0, 112.0, 30.0) pullsDown:NO];
    [_bufferPopup addItemsWithTitles:@[ @"64 samples", @"128 samples", @"256 samples" ]];
    [_bufferPopup selectItemAtIndex:0];
    _bufferPopup.autoresizingMask = NSViewMinYMargin;
    stylePopup(_bufferPopup);
    [_leftPanel addSubview:_bufferPopup];

    _startButton = makeButton(@"START AUDIO", NSMakeRect(14.0, 202.0, 112.0, 36.0), self, @selector(startAudio:));
    _startButton.contentTintColor = liveColor();
    _startButton.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:_startButton];
    _stopButton = makeButton(@"STOP", NSMakeRect(134.0, 202.0, 112.0, 36.0), self, @selector(stopAudio:));
    _stopButton.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:_stopButton];

    NSTextField* safety = makeLabel(@"Start with interface / amp volume low.",
                                    NSMakeRect(16.0, 164.0, 228.0, 28.0),
                                    10.0,
                                    NSFontWeightRegular,
                                    warningColor());
    safety.usesSingleLineMode = NO;
    safety.autoresizingMask = NSViewMinYMargin;
    [_leftPanel addSubview:safety];

    NSTextField* phaseNote = makeLabel(@"Pedal packages are bundled, but package-aware browsing remains a later UI phase.",
                                       NSMakeRect(16.0, 28.0, 228.0, 82.0),
                                       10.0,
                                       NSFontWeightRegular,
                                       mutedTextColor());
    phaseNote.usesSingleLineMode = NO;
    phaseNote.lineBreakMode = NSLineBreakByWordWrapping;
    [_leftPanel addSubview:phaseNote];

    _workspacePanel = [[CircuitPedalPanelView alloc] initWithFrame:NSMakeRect(284.0, 124.0, 742.0, 612.0)];
    _workspacePanel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [content addSubview:_workspacePanel];

    NSTextField* chainTitle = makeSectionLabel(@"SIGNAL CHAIN", NSMakeRect(18.0, 574.0, 130.0, 18.0));
    chainTitle.autoresizingMask = NSViewMinYMargin;
    [_workspacePanel addSubview:chainTitle];
    NSTextField* chainMode = makeLabel(@"CHAIN VIEW  •  Phase 1 shell",
                                       NSMakeRect(450.0, 574.0, 270.0, 18.0),
                                       9.0,
                                       NSFontWeightMedium,
                                       mutedTextColor());
    chainMode.alignment = NSTextAlignmentRight;
    chainMode.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    [_workspacePanel addSubview:chainMode];

    _chainView = [[CircuitPedalChainView alloc] initWithFrame:NSMakeRect(16.0, 470.0, 710.0, 96.0)];
    _chainView.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [_workspacePanel addSubview:_chainView];

    _heroView = [[CircuitPedalHeroView alloc] initWithFrame:NSMakeRect(16.0, 16.0, 710.0, 442.0)];
    _heroView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [_workspacePanel addSubview:_heroView];

    _rightPanel = [[CircuitPedalPanelView alloc] initWithFrame:NSMakeRect(1038.0, 124.0, 310.0, 612.0)];
    _rightPanel.autoresizingMask = NSViewHeightSizable | NSViewMinXMargin;
    [content addSubview:_rightPanel];

    NSTextField* inspectorTitle = makeSectionLabel(@"INSPECTOR", NSMakeRect(16.0, 574.0, 120.0, 18.0));
    inspectorTitle.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:inspectorTitle];
    NSTextField* parameterTab = makeLabel(@"Parameters", NSMakeRect(16.0, 542.0, 86.0, 22.0), 12.0, NSFontWeightSemibold, accentColor());
    parameterTab.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:parameterTab];
    NSTextField* futureTabs = makeLabel(@"Circuit    EQ    Settings", NSMakeRect(112.0, 542.0, 180.0, 22.0), 10.0, NSFontWeightMedium, mutedTextColor());
    futureTabs.alignment = NSTextAlignmentRight;
    futureTabs.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:futureTabs];

    _diodeLabel = makeSectionLabel(@"CLIPPING DIODES", NSMakeRect(18.0, 502.0, 150.0, 18.0));
    _diodeLabel.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:_diodeLabel];
    _diodePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16.0, 466.0, 278.0, 30.0) pullsDown:NO];
    [_diodePopup addItemsWithTitles:@[ @"Reference germanium (V0.2)", @"Silicon-like (experimental)", @"LED-like (experimental)", @"No clipping diodes" ]];
    [_diodePopup selectItemAtIndex:0];
    _diodePopup.autoresizingMask = NSViewMinYMargin;
    stylePopup(_diodePopup);
    [_rightPanel addSubview:_diodePopup];

    _distortionLabel = makeLabel(@"DISTORTION", NSMakeRect(24.0, 430.0, 110.0, 18.0), 10.0, NSFontWeightSemibold, textColor());
    _distortionLabel.alignment = NSTextAlignmentCenter;
    _distortionLabel.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:_distortionLabel];
    _distortionSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(38.0, 340.0, 82.0, 82.0)];
    _distortionSlider.doubleValue = 65.0;
    _distortionSlider.target = self;
    _distortionSlider.action = @selector(distortionSliderChanged:);
    _distortionSlider.autoresizingMask = NSViewMinYMargin;
    styleRotarySlider(_distortionSlider);
    [_rightPanel addSubview:_distortionSlider];
    _distortionValue = makeLabel(@"65%", NSMakeRect(24.0, 318.0, 110.0, 18.0), 11.0, NSFontWeightMedium, accentColor());
    _distortionValue.alignment = NSTextAlignmentCenter;
    _distortionValue.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:_distortionValue];

    _outputControlLabel = makeLabel(@"OUTPUT", NSMakeRect(176.0, 430.0, 110.0, 18.0), 10.0, NSFontWeightSemibold, textColor());
    _outputControlLabel.alignment = NSTextAlignmentCenter;
    _outputControlLabel.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:_outputControlLabel];
    _outputSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(190.0, 340.0, 82.0, 82.0)];
    _outputSlider.doubleValue = 70.0;
    _outputSlider.target = self;
    _outputSlider.action = @selector(outputSliderChanged:);
    _outputSlider.autoresizingMask = NSViewMinYMargin;
    styleRotarySlider(_outputSlider);
    [_rightPanel addSubview:_outputSlider];
    _outputValue = makeLabel(@"70%", NSMakeRect(176.0, 318.0, 110.0, 18.0), 11.0, NSFontWeightMedium, accentColor());
    _outputValue.alignment = NSTextAlignmentCenter;
    _outputValue.autoresizingMask = NSViewMinYMargin;
    [_rightPanel addSubview:_outputValue];

    _circuitScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(12.0, 118.0, 286.0, 410.0)];
    _circuitScrollView.hasVerticalScroller = YES;
    _circuitScrollView.hasHorizontalScroller = NO;
    _circuitScrollView.autohidesScrollers = YES;
    _circuitScrollView.borderType = NSNoBorder;
    _circuitScrollView.drawsBackground = NO;
    _circuitScrollView.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    _circuitDocumentView = [[CircuitPedalFlippedView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 268.0, 410.0)];
    _circuitScrollView.documentView = _circuitDocumentView;
    [_rightPanel addSubview:_circuitScrollView];

    for (NSInteger i = 0; i < 16; ++i)
    {
        const NSInteger column = i % 2;
        const NSInteger row = i / 2;
        const CGFloat x = 5.0 + static_cast<CGFloat>(column) * 132.0;
        const CGFloat y = 8.0 + static_cast<CGFloat>(row) * 120.0;
        _circuitLabels[i] = makeLabel(@"CONTROL", NSMakeRect(x, y, 122.0, 18.0), 9.5, NSFontWeightSemibold, textColor());
        _circuitLabels[i].alignment = NSTextAlignmentCenter;
        [_circuitDocumentView addSubview:_circuitLabels[i]];

        _circuitSliders[i] = [[NSSlider alloc] initWithFrame:NSMakeRect(x + 25.0, y + 22.0, 72.0, 72.0)];
        _circuitSliders[i].doubleValue = 50.0;
        _circuitSliders[i].target = self;
        _circuitSliders[i].action = @selector(circuitSliderChanged:);
        _circuitSliders[i].tag = i;
        styleRotarySlider(_circuitSliders[i]);
        [_circuitDocumentView addSubview:_circuitSliders[i]];

        _circuitSwitchPopups[i] = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y + 42.0, 122.0, 30.0) pullsDown:NO];
        _circuitSwitchPopups[i].target = self;
        _circuitSwitchPopups[i].action = @selector(circuitSwitchChanged:);
        _circuitSwitchPopups[i].tag = i;
        stylePopup(_circuitSwitchPopups[i]);
        _circuitSwitchPopups[i].hidden = YES;
        [_circuitDocumentView addSubview:_circuitSwitchPopups[i]];

        _circuitValues[i] = makeLabel(@"50%", NSMakeRect(x, y + 96.0, 122.0, 17.0), 10.0, NSFontWeightMedium, accentColor());
        _circuitValues[i].alignment = NSTextAlignmentCenter;
        [_circuitDocumentView addSubview:_circuitValues[i]];
    }

    NSTextField* inspectorHint = makeLabel(@"Controls are generated from the loaded circuit definition.",
                                           NSMakeRect(16.0, 72.0, 278.0, 38.0),
                                           9.5,
                                           NSFontWeightRegular,
                                           mutedTextColor());
    inspectorHint.usesSingleLineMode = NO;
    inspectorHint.lineBreakMode = NSLineBreakByWordWrapping;
    [_rightPanel addSubview:inspectorHint];

    _bottomPanel = [[CircuitPedalPanelView alloc] initWithFrame:NSMakeRect(12.0, 12.0, 1336.0, 100.0)];
    _bottomPanel.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    [content addSubview:_bottomPanel];

    [ _bottomPanel addSubview:makeSectionLabel(@"INPUT", NSMakeRect(14.0, 69.0, 60.0, 16.0)) ];
    _inputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(166.0, 69.0, 74.0, 16.0), 9.5, NSFontWeightMedium, textColor());
    _inputDbLabel.alignment = NSTextAlignmentRight;
    [_bottomPanel addSubview:_inputDbLabel];
    _inputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(14.0, 48.0, 226.0, 16.0)];
    [_bottomPanel addSubview:_inputMeter];

    _historyView = [[CircuitPedalHistoryView alloc] initWithFrame:NSMakeRect(260.0, 20.0, 330.0, 62.0)];
    _historyView.autoresizingMask = NSViewWidthSizable;
    [_bottomPanel addSubview:_historyView];
    NSTextField* monitorLabel = makeSectionLabel(@"LIVE MONITOR", NSMakeRect(270.0, 69.0, 110.0, 16.0));
    [_bottomPanel addSubview:monitorLabel];

    _bypassButton = makeButton(@"ACTIVE", NSMakeRect(610.0, 26.0, 120.0, 48.0), self, @selector(bypassChanged:));
    _bypassButton.buttonType = NSButtonTypePushOnPushOff;
    _bypassButton.contentTintColor = liveColor();
    _bypassButton.autoresizingMask = NSViewMinXMargin | NSViewMaxXMargin;
    [_bottomPanel addSubview:_bypassButton];

    _statusLabel = makeLabel(@"Audio stopped — choose a pedal and audio device, then Start Audio.",
                             NSMakeRect(752.0, 48.0, 340.0, 34.0),
                             10.0,
                             NSFontWeightRegular,
                             cpColor(0.72, 0.76, 0.80));
    _statusLabel.font = [NSFont monospacedSystemFontOfSize:10.0 weight:NSFontWeightRegular];
    _statusLabel.usesSingleLineMode = NO;
    _statusLabel.autoresizingMask = NSViewMinXMargin;
    [_bottomPanel addSubview:_statusLabel];

    _errorLabel = makeLabel(@"Ready.", NSMakeRect(752.0, 18.0, 340.0, 26.0), 9.5, NSFontWeightRegular, mutedTextColor());
    _errorLabel.autoresizingMask = NSViewMinXMargin;
    [_bottomPanel addSubview:_errorLabel];

    [ _bottomPanel addSubview:makeSectionLabel(@"OUTPUT", NSMakeRect(1112.0, 69.0, 70.0, 16.0)) ];
    _outputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(1260.0, 69.0, 62.0, 16.0), 9.5, NSFontWeightMedium, textColor());
    _outputDbLabel.alignment = NSTextAlignmentRight;
    _outputDbLabel.autoresizingMask = NSViewMinXMargin;
    [_bottomPanel addSubview:_outputDbLabel];
    _outputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(1112.0, 48.0, 210.0, 16.0)];
    _outputMeter.autoresizingMask = NSViewMinXMargin;
    [_bottomPanel addSubview:_outputMeter];

    [self populateCircuitLibrary];
    [self populateDevices];
    [self refreshModelControls];
    [self setRunningControls:NO];
    [self refreshBypassAppearance];

    _meterTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                   target:self
                                                 selector:@selector(updateMeters:)
                                                 userInfo:nil
                                                  repeats:YES];

    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    (void)sender;
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
    (void)notification;
    [_meterTimer invalidate];
    if (_engine)
        _engine->stop();
}

- (void)populateCircuitLibrary
{
    [_circuitLibraryPopup removeAllItems];
    _circuitLibraryPaths.clear();
    [_circuitLibraryPopup addItemWithTitle:@"Choose Circuit…"];
    _circuitLibraryPaths.emplace_back();

    NSArray<NSURL*>* bundled = [[NSBundle mainBundle] URLsForResourcesWithExtension:@"cpedal" subdirectory:@"circuits"];
    struct LibraryEntry { std::string name; std::string path; };
    std::vector<LibraryEntry> entries;
    if (bundled != nil)
    {
        entries.reserve(bundled.count);
        for (NSURL* url in bundled)
        {
            const char* utf8Path = url.path.UTF8String;
            if (utf8Path == nullptr)
                continue;
            circuitpedal::CircuitFileDocument document;
            std::string error;
            if (circuitpedal::loadCircuitFile(utf8Path, document, error))
                entries.push_back({ document.name, utf8Path });
        }
    }

    std::sort(entries.begin(), entries.end(), [](const LibraryEntry& a, const LibraryEntry& b) { return a.name < b.name; });
    for (const auto& entry : entries)
    {
        [_circuitLibraryPopup addItemWithTitle:nsString(entry.name)];
        _circuitLibraryPaths.push_back(entry.path);
    }

    [_circuitLibraryPopup.menu addItem:[NSMenuItem separatorItem]];
    _circuitLibraryPaths.emplace_back();
    [_circuitLibraryPopup addItemWithTitle:@"Load External…"];
    _circuitLibraryPaths.emplace_back("__external__");
    [_circuitLibraryPopup selectItemAtIndex:0];
}

- (void)circuitLibraryChanged:(id)sender
{
    (void)sender;
    if (_engine->isRunning())
        return;
    const NSInteger row = _circuitLibraryPopup.indexOfSelectedItem;
    if (row < 0 || static_cast<std::size_t>(row) >= _circuitLibraryPaths.size())
        return;
    const std::string path = _circuitLibraryPaths[static_cast<std::size_t>(row)];
    if (path.empty())
        return;
    if (path == "__external__")
    {
        [_circuitLibraryPopup selectItemAtIndex:0];
        [self loadCircuit:nil];
        return;
    }

    std::string error;
    if (!_engine->loadCircuitFile(path, error))
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = nsString(error);
        [_circuitLibraryPopup selectItemAtIndex:0];
        NSBeep();
        return;
    }
    _bypassButton.state = NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Circuit selected. Start Audio when ready.";
    [self refreshModelControls];
    [self setRunningControls:NO];
    [self refreshBypassAppearance];
}

- (void)populateDevices
{
    [_devicePopup removeAllItems];
    _devices.clear();
    std::string error;
    const auto allDevices = circuitpedal::MacAudioEngine::enumerateDevices(error);
    for (const auto& device : allDevices)
    {
        if (device.isDuplex())
            _devices.push_back(device);
    }
    for (const auto& device : _devices)
    {
        NSString* title = [NSString stringWithFormat:@"%@ — %u in / %u out", nsString(device.name), device.inputChannels, device.outputChannels];
        [_devicePopup addItemWithTitle:title];
    }

    if (!error.empty())
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = nsString(error);
    }
    else if (_devices.empty())
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = @"No duplex audio device found.";
    }
    else
    {
        std::string defaultError;
        const std::uint32_t defaultId = circuitpedal::MacAudioEngine::defaultOutputDeviceId(defaultError);
        const auto preferred = std::find_if(_devices.begin(), _devices.end(), [defaultId](const circuitpedal::AudioDeviceInfo& device) { return device.id == defaultId; });
        const NSInteger row = preferred == _devices.end() ? 0 : static_cast<NSInteger>(std::distance(_devices.begin(), preferred));
        [_devicePopup selectItemAtIndex:row];
        _errorLabel.textColor = mutedTextColor();
        _errorLabel.stringValue = @"Ready.";
    }
    [self updateInputChannels];
    [self updateDeviceSummary];
}

- (void)updateInputChannels
{
    [_channelPopup removeAllItems];
    const NSInteger row = _devicePopup.indexOfSelectedItem;
    if (row < 0 || static_cast<std::size_t>(row) >= _devices.size())
        return;
    const std::uint32_t channelCount = _devices[static_cast<std::size_t>(row)].inputChannels;
    for (std::uint32_t channel = 1; channel <= channelCount; ++channel)
        [_channelPopup addItemWithTitle:[NSString stringWithFormat:@"Input %u", channel]];
    if (channelCount > 0)
        [_channelPopup selectItemAtIndex:0];
}

- (void)updateDeviceSummary
{
    const NSInteger row = _devicePopup.indexOfSelectedItem;
    if (row >= 0 && static_cast<std::size_t>(row) < _devices.size())
        _deviceSummaryLabel.stringValue = nsString(_devices[static_cast<std::size_t>(row)].name);
    else
        _deviceSummaryLabel.stringValue = @"No audio device";
}

- (void)deviceChanged:(id)sender
{
    (void)sender;
    [self updateInputChannels];
    [self updateDeviceSummary];
}

- (void)refreshModelControls
{
    const BOOL generic = _engine->usingCircuitFile();
    NSString* activeName = nsString(_engine->activeModelName());
    _modelValue.stringValue = activeName;
    _topModelLabel.stringValue = activeName;
    _chainView.selectedName = activeName;
    _heroView.pedalName = activeName;
    _heroView.pedalType = generic ? @"GENERIC CIRCUIT MODEL  •  48 kHz / 1×" : @"REFERENCE DISTORTION MODEL";

    _diodeLabel.hidden = generic;
    _diodePopup.hidden = generic;
    _distortionLabel.hidden = generic;
    _distortionSlider.hidden = generic;
    _distortionValue.hidden = generic;
    _outputControlLabel.hidden = generic;
    _outputSlider.hidden = generic;
    _outputValue.hidden = generic;

    _circuitControls = _engine->circuitControls();
    _circuitScrollView.hidden = !generic;
    const std::size_t visibleControlCount = std::min<std::size_t>(_circuitControls.size(), 16);
    const std::size_t rows = (visibleControlCount + 1U) / 2U;
    const double documentHeight = std::max(410.0, 12.0 + 120.0 * static_cast<double>(rows));
    _circuitDocumentView.frame = NSMakeRect(0.0, 0.0, 268.0, documentHeight);

    for (std::size_t i = 0; i < 16; ++i)
    {
        const BOOL visible = generic && i < visibleControlCount;
        _circuitLabels[i].hidden = !visible;
        _circuitSliders[i].hidden = YES;
        _circuitSwitchPopups[i].hidden = YES;
        _circuitValues[i].hidden = YES;
        if (!visible)
            continue;

        const auto& control = _circuitControls[i];
        _circuitLabels[i].stringValue = nsString(control.name);
        if (control.kind == circuitpedal::CircuitFileControlKind::Switch)
        {
            [_circuitSwitchPopups[i] removeAllItems];
            for (const auto& label : control.switchPositionNames)
                [_circuitSwitchPopups[i] addItemWithTitle:nsString(label)];
            const std::uint32_t count = std::max<std::uint32_t>(2U, control.switchPositionCount);
            const double normalized = static_cast<double>(_engine->circuitControl(i));
            const NSInteger position = static_cast<NSInteger>(std::llround(normalized * static_cast<double>(count - 1U)));
            if (_circuitSwitchPopups[i].numberOfItems > 0)
                [_circuitSwitchPopups[i] selectItemAtIndex:std::clamp<NSInteger>(position, 0, _circuitSwitchPopups[i].numberOfItems - 1)];
            _circuitSwitchPopups[i].hidden = NO;
        }
        else
        {
            const double value = 100.0 * static_cast<double>(_engine->circuitControl(i));
            _circuitSliders[i].doubleValue = value;
            _circuitValues[i].stringValue = [NSString stringWithFormat:@"%.0f%%", value];
            _circuitSliders[i].hidden = NO;
            _circuitValues[i].hidden = NO;
        }
    }
    if (generic)
        [_circuitDocumentView scrollPoint:NSMakePoint(0.0, 0.0)];
}

- (void)setRunningControls:(BOOL)running
{
    _devicePopup.enabled = !running && !_devices.empty();
    _channelPopup.enabled = !running && _channelPopup.numberOfItems > 0;
    _bufferPopup.enabled = !running;
    _circuitLibraryPopup.enabled = !running;
    _builtinButton.enabled = !running && _engine->usingCircuitFile();
    _diodePopup.enabled = !running && !_engine->usingCircuitFile();
    _distortionSlider.enabled = !_engine->usingCircuitFile();
    _outputSlider.enabled = !_engine->usingCircuitFile();
    for (NSInteger i = 0; i < 16; ++i)
    {
        _circuitSliders[i].enabled = _engine->usingCircuitFile();
        _circuitSwitchPopups[i].enabled = _engine->usingCircuitFile();
    }
    _startButton.enabled = !running && !_devices.empty() && _channelPopup.numberOfItems > 0;
    _stopButton.enabled = running;
    _liveDot.active = running;
    _liveText.stringValue = running ? @"AUDIO ACTIVE" : @"AUDIO STOPPED";
    _liveText.textColor = running ? liveColor() : mutedTextColor();
}

- (void)refreshBypassAppearance
{
    const BOOL bypassed = _bypassButton.state == NSControlStateValueOn;
    _bypassButton.title = bypassed ? @"BYPASSED" : @"ACTIVE";
    _bypassButton.contentTintColor = bypassed ? warningColor() : liveColor();
    _chainView.bypassed = bypassed;
    _heroView.bypassed = bypassed;
}

- (void)loadCircuit:(id)sender
{
    (void)sender;
    if (_engine->isRunning())
        return;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = NO;
    panel.canChooseFiles = YES;
    panel.allowsMultipleSelection = NO;
    panel.title = @"Load CircuitPedal Circuit";
    panel.prompt = @"Load";
    if ([panel runModal] != NSModalResponseOK || panel.URL == nil)
        return;
    const char* utf8Path = panel.URL.path.UTF8String;
    if (utf8Path == nullptr)
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = @"Could not read the selected file path.";
        return;
    }
    std::string error;
    if (!_engine->loadCircuitFile(utf8Path, error))
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = nsString(error);
        NSBeep();
        return;
    }
    [_circuitLibraryPopup selectItemAtIndex:0];
    _bypassButton.state = NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"External circuit loaded.";
    [self refreshModelControls];
    [self setRunningControls:NO];
    [self refreshBypassAppearance];
}

- (void)useBuiltin:(id)sender
{
    (void)sender;
    if (!_engine->useBuiltInDistortionPlus())
        return;
    [_circuitLibraryPopup selectItemAtIndex:0];
    _bypassButton.state = _engine->bypassed() ? NSControlStateValueOn : NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Built-in Distortion+ selected.";
    [self refreshModelControls];
    [self setRunningControls:NO];
    [self refreshBypassAppearance];
}

- (void)startAudio:(id)sender
{
    (void)sender;
    const NSInteger deviceRow = _devicePopup.indexOfSelectedItem;
    const NSInteger channelRow = _channelPopup.indexOfSelectedItem;
    if (deviceRow < 0 || static_cast<std::size_t>(deviceRow) >= _devices.size() || channelRow < 0)
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = @"Select a duplex device and input channel before starting.";
        return;
    }

    circuitpedal::AudioStartConfiguration configuration;
    configuration.deviceId = _devices[static_cast<std::size_t>(deviceRow)].id;
    configuration.inputChannel = static_cast<std::uint32_t>(channelRow);
    const NSInteger bufferIndex = std::max<NSInteger>(0, _bufferPopup.indexOfSelectedItem);
    const std::uint32_t bufferValues[] = { 64U, 128U, 256U };
    configuration.requestedBufferFrames = bufferValues[std::min<NSInteger>(bufferIndex, 2)];

    if (!_engine->usingCircuitFile())
    {
        _engine->setDistortion(static_cast<float>(_distortionSlider.doubleValue / 100.0));
        _engine->setOutput(static_cast<float>(_outputSlider.doubleValue / 100.0));
        const NSInteger diodeRow = _diodePopup.indexOfSelectedItem;
        const auto diodePreset = static_cast<circuitpedal::ClippingDiodePreset>(diodeRow >= 0 ? static_cast<std::uint32_t>(diodeRow) : 0U);
        _engine->setClippingDiodePreset(diodePreset);
    }
    _engine->setBypass(_bypassButton.state == NSControlStateValueOn);

    std::string error;
    if (!_engine->start(configuration, error))
    {
        _errorLabel.textColor = errorColor();
        _errorLabel.stringValue = nsString(error);
        _statusLabel.stringValue = @"Audio stopped.";
        [self setRunningControls:NO];
        NSBeep();
        return;
    }
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Live audio running.";
    [self setRunningControls:YES];
    [self updateRunningStatus];
}

- (void)stopAudio:(id)sender
{
    (void)sender;
    _engine->stop();
    _inputMeter.level = 0.0;
    _outputMeter.level = 0.0;
    _inputDbLabel.stringValue = @"−∞ dB";
    _outputDbLabel.stringValue = @"−∞ dB";
    _statusLabel.stringValue = @"Audio stopped — settings unlocked.";
    _sampleRateLabel.stringValue = @"-- kHz";
    _bufferStatusLabel.stringValue = @"-- smp";
    _latencyLabel.stringValue = @"-- ms";
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Audio stopped.";
    [self refreshModelControls];
    [self setRunningControls:NO];
}

- (void)distortionSliderChanged:(id)sender
{
    (void)sender;
    const double value = _distortionSlider.doubleValue;
    _distortionValue.stringValue = [NSString stringWithFormat:@"%.0f%%", value];
    _engine->setDistortion(static_cast<float>(value / 100.0));
}

- (void)outputSliderChanged:(id)sender
{
    (void)sender;
    const double value = _outputSlider.doubleValue;
    _outputValue.stringValue = [NSString stringWithFormat:@"%.0f%%", value];
    _engine->setOutput(static_cast<float>(value / 100.0));
}

- (void)circuitSliderChanged:(id)sender
{
    NSSlider* slider = (NSSlider*)sender;
    const NSInteger index = slider.tag;
    if (index < 0 || index >= 16)
        return;
    const double value = slider.doubleValue;
    _circuitValues[index].stringValue = [NSString stringWithFormat:@"%.0f%%", value];
    (void)_engine->setCircuitControl(static_cast<std::size_t>(index), static_cast<float>(value / 100.0));
}

- (void)circuitSwitchChanged:(id)sender
{
    NSPopUpButton* popup = (NSPopUpButton*)sender;
    const NSInteger index = popup.tag;
    if (index < 0 || index >= 16 || static_cast<std::size_t>(index) >= _circuitControls.size())
        return;
    const auto& control = _circuitControls[static_cast<std::size_t>(index)];
    const std::uint32_t count = std::max<std::uint32_t>(2U, control.switchPositionCount);
    const NSInteger selected = popup.indexOfSelectedItem;
    if (selected < 0)
        return;
    const float normalized = static_cast<float>(selected) / static_cast<float>(count - 1U);
    (void)_engine->setCircuitControl(static_cast<std::size_t>(index), normalized);
}

- (void)bypassChanged:(id)sender
{
    (void)sender;
    _engine->setBypass(_bypassButton.state == NSControlStateValueOn);
    [self refreshBypassAppearance];
}

- (void)updateMeters:(NSTimer*)timer
{
    (void)timer;
    if (!_engine->isRunning())
        return;
    const double input = std::clamp(static_cast<double>(_engine->inputPeak()), 0.0, 1.0);
    const double output = std::clamp(static_cast<double>(_engine->outputPeak()), 0.0, 1.0);
    _inputMeter.level = input;
    _outputMeter.level = output;
    _inputDbLabel.stringValue = dbText(input);
    _outputDbLabel.stringValue = dbText(output);
    [_historyView pushLevel:output];
}

- (void)updateRunningStatus
{
    const circuitpedal::AudioRuntimeInfo info = _engine->runtimeInfo();
    _sampleRateLabel.stringValue = [NSString stringWithFormat:@"%.1f kHz", info.sampleRate / 1000.0];
    _bufferStatusLabel.stringValue = [NSString stringWithFormat:@"%u smp", info.actualBufferFrames];
    _latencyLabel.stringValue = [NSString stringWithFormat:@"%.1f ms", info.reportedLatencyMilliseconds()];
    if (_engine->usingCircuitFile())
    {
        _statusLabel.stringValue = [NSString stringWithFormat:@"%@  •  1× live  •  %.2f ms buffer", nsString(_engine->activeModelName()), info.bufferDurationMilliseconds()];
    }
    else
    {
        _statusLabel.stringValue = [NSString stringWithFormat:@"Built-in Distortion+  •  %s  •  %.2f ms buffer", circuitpedal::clippingDiodePresetName(_engine->clippingDiodePreset()), info.bufferDurationMilliseconds()];
    }
}

@end

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        application.activationPolicy = NSApplicationActivationPolicyRegular;

        NSMenu* menuBar = [[NSMenu alloc] init];
        NSMenuItem* applicationMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:applicationMenuItem];
        application.mainMenu = menuBar;

        NSMenu* applicationMenu = [[NSMenu alloc] init];
        NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit CircuitPedal"
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
        [applicationMenu addItem:quitItem];
        applicationMenuItem.submenu = applicationMenu;

        CircuitPedalAppDelegate* delegate = [[CircuitPedalAppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
