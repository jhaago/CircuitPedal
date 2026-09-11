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

NSColor* backgroundColor() { return cpColor(0.035, 0.043, 0.050); }
NSColor* panelColor() { return cpColor(0.055, 0.065, 0.075); }
NSColor* pedalColor() { return cpColor(0.085, 0.090, 0.095); }
NSColor* borderColor() { return cpColor(0.16, 0.18, 0.20); }
NSColor* accentColor() { return cpColor(0.94, 0.64, 0.22); }
NSColor* textColor() { return cpColor(0.92, 0.93, 0.94); }
NSColor* mutedTextColor() { return cpColor(0.52, 0.56, 0.60); }
NSColor* liveColor() { return cpColor(0.35, 0.90, 0.45); }

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
    return label;
}

NSTextField* makeSectionLabel(NSString* text, NSRect frame)
{
    NSTextField* label = makeLabel(text,
                                   frame,
                                   10.5,
                                   NSFontWeightSemibold,
                                   mutedTextColor());
    label.font = [NSFont monospacedSystemFontOfSize:10.5
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
    button.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium];
    button.contentTintColor = textColor();
    return button;
}

void stylePopup(NSPopUpButton* popup)
{
    popup.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightRegular];
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
}

@end

@interface CircuitPedalFaceView : CircuitPedalPanelView
@end

@implementation CircuitPedalFaceView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self != nil)
    {
        self.fillColor = pedalColor();
        self.strokeColor = cpColor(0.28, 0.25, 0.20);
        self.cornerRadius = 16.0;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    [[NSColor colorWithWhite:1.0 alpha:0.035] setStroke];
    NSBezierPath* highlight = [NSBezierPath bezierPath];
    [highlight moveToPoint:NSMakePoint(18.0, NSHeight(self.bounds) - 22.0)];
    [highlight lineToPoint:NSMakePoint(NSWidth(self.bounds) - 18.0,
                                       NSHeight(self.bounds) - 22.0)];
    highlight.lineWidth = 1.0;
    [highlight stroke];

    const NSPoint screws[] = {
        NSMakePoint(18.0, 18.0),
        NSMakePoint(NSWidth(self.bounds) - 18.0, 18.0),
        NSMakePoint(18.0, NSHeight(self.bounds) - 18.0),
        NSMakePoint(NSWidth(self.bounds) - 18.0, NSHeight(self.bounds) - 18.0)
    };
    for (const NSPoint point : screws)
    {
        NSBezierPath* screw = [NSBezierPath bezierPathWithOvalInRect:
            NSMakeRect(point.x - 5.0, point.y - 5.0, 10.0, 10.0)];
        [cpColor(0.20, 0.20, 0.19) setFill];
        [screw fill];
        [cpColor(0.42, 0.40, 0.35) setStroke];
        [screw stroke];
    }
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
    const NSInteger segments = 18;
    const CGFloat gap = 2.0;
    const CGFloat width = (NSWidth(self.bounds) - gap * (segments - 1)) / segments;
    const NSInteger lit = static_cast<NSInteger>(std::ceil(self.level * segments));

    for (NSInteger i = 0; i < segments; ++i)
    {
        NSColor* color = cpColor(0.12, 0.14, 0.15);
        if (i < lit)
        {
            const double normalized = static_cast<double>(i) / static_cast<double>(segments - 1);
            if (normalized > 0.86)
                color = cpColor(0.95, 0.30, 0.24);
            else if (normalized > 0.68)
                color = cpColor(0.95, 0.73, 0.22);
            else
                color = liveColor();
        }

        NSRect segment = NSMakeRect(static_cast<CGFloat>(i) * (width + gap),
                                    1.0,
                                    width,
                                    NSHeight(self.bounds) - 2.0);
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:segment
                                                            xRadius:2.0
                                                            yRadius:2.0];
        [color setFill];
        [path fill];
    }
}

@end

@interface CircuitPedalStatusDotView : NSView
@property(nonatomic) BOOL active;
@end

@implementation CircuitPedalStatusDotView

- (void)setActive:(BOOL)active
{
    _active = active;
    self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    NSRect dotRect = NSInsetRect(self.bounds, 2.0, 2.0);
    NSBezierPath* dot = [NSBezierPath bezierPathWithOvalInRect:dotRect];
    [(self.active ? liveColor() : cpColor(0.25, 0.28, 0.30)) setFill];
    [dot fill];
}

@end

@interface CircuitPedalFlippedView : NSView
@end

@implementation CircuitPedalFlippedView
- (BOOL)isFlipped { return YES; }
@end

@interface CircuitPedalAppDelegate : NSObject <NSApplicationDelegate> {
@private
    std::unique_ptr<circuitpedal::MacAudioEngine> _engine;
    std::vector<circuitpedal::AudioDeviceInfo> _devices;
    std::vector<circuitpedal::CircuitFileControl> _circuitControls;
    std::vector<std::string> _circuitLibraryPaths;

    NSWindow* _window;
    NSTextField* _pedalTitle;
    NSTextField* _pedalSubtitle;
    NSTextField* _modelValue;
    CircuitPedalStatusDotView* _liveDot;
    NSTextField* _liveText;

    NSPopUpButton* _devicePopup;
    NSPopUpButton* _channelPopup;
    NSPopUpButton* _bufferPopup;
    NSPopUpButton* _circuitLibraryPopup;
    NSButton* _builtinButton;

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

    NSButton* _startButton;
    NSButton* _stopButton;
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

    const NSRect windowRect = NSMakeRect(0.0, 0.0, 1180.0, 760.0);
    const NSWindowStyleMask style = NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    _window = [[NSWindow alloc] initWithContentRect:windowRect
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"CircuitPedal — Circuit Lab";
    _window.releasedWhenClosed = NO;
    _window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    _window.backgroundColor = backgroundColor();

    NSView* content = _window.contentView;
    content.wantsLayer = YES;

    CircuitPedalPanelView* header = [[CircuitPedalPanelView alloc]
        initWithFrame:NSMakeRect(0.0, 700.0, 1180.0, 60.0)];
    header.cornerRadius = 0.0;
    header.fillColor = cpColor(0.045, 0.052, 0.060);
    [content addSubview:header];

    NSTextField* brand = makeLabel(@"CircuitPedal",
                                   NSMakeRect(24.0, 719.0, 260.0, 28.0),
                                   24.0,
                                   NSFontWeightSemibold);
    [content addSubview:brand];
    NSTextField* brandTag = makeLabel(@"REAL CIRCUITS  •  REAL TONE",
                                      NSMakeRect(26.0, 705.0, 280.0, 14.0),
                                      9.0,
                                      NSFontWeightMedium,
                                      mutedTextColor());
    brandTag.font = [NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightMedium];
    [content addSubview:brandTag];

    NSTextField* mode = makeLabel(@"PLAY",
                                  NSMakeRect(545.0, 719.0, 90.0, 24.0),
                                  12.0,
                                  NSFontWeightSemibold,
                                  accentColor());
    mode.alignment = NSTextAlignmentCenter;
    [content addSubview:mode];

    _liveDot = [[CircuitPedalStatusDotView alloc]
        initWithFrame:NSMakeRect(925.0, 724.0, 14.0, 14.0)];
    [content addSubview:_liveDot];
    _liveText = makeLabel(@"AUDIO STOPPED",
                          NSMakeRect(946.0, 719.0, 200.0, 24.0),
                          10.5,
                          NSFontWeightSemibold,
                          mutedTextColor());
    _liveText.font = [NSFont monospacedSystemFontOfSize:10.5 weight:NSFontWeightSemibold];
    [content addSubview:_liveText];

    CircuitPedalPanelView* leftPanel = [[CircuitPedalPanelView alloc]
        initWithFrame:NSMakeRect(16.0, 90.0, 260.0, 596.0)];
    [content addSubview:leftPanel];

    [leftPanel addSubview:makeSectionLabel(@"PEDAL LIBRARY", NSMakeRect(16.0, 552.0, 220.0, 20.0))];
    _modelValue = makeLabel(@"Built-in Distortion+",
                            NSMakeRect(16.0, 510.0, 228.0, 38.0),
                            15.0,
                            NSFontWeightSemibold);
    _modelValue.usesSingleLineMode = NO;
    _modelValue.lineBreakMode = NSLineBreakByWordWrapping;
    [leftPanel addSubview:_modelValue];

    _circuitLibraryPopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(14.0, 466.0, 232.0, 32.0)
            pullsDown:NO];
    _circuitLibraryPopup.target = self;
    _circuitLibraryPopup.action = @selector(circuitLibraryChanged:);
    stylePopup(_circuitLibraryPopup);
    [leftPanel addSubview:_circuitLibraryPopup];

    _builtinButton = makeButton(@"Use Built-in Distortion+",
                                NSMakeRect(16.0, 424.0, 228.0, 32.0),
                                self,
                                @selector(useBuiltin:));
    [leftPanel addSubview:_builtinButton];

    [leftPanel addSubview:makeSectionLabel(@"AUDIO I/O", NSMakeRect(16.0, 374.0, 220.0, 20.0))];
    [leftPanel addSubview:makeLabel(@"Device", NSMakeRect(16.0, 348.0, 100.0, 18.0), 11.0, NSFontWeightMedium, mutedTextColor())];
    _devicePopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(14.0, 314.0, 232.0, 30.0)
            pullsDown:NO];
    _devicePopup.target = self;
    _devicePopup.action = @selector(deviceChanged:);
    stylePopup(_devicePopup);
    [leftPanel addSubview:_devicePopup];

    [leftPanel addSubview:makeLabel(@"Input Channel", NSMakeRect(16.0, 286.0, 120.0, 18.0), 11.0, NSFontWeightMedium, mutedTextColor())];
    _channelPopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(14.0, 252.0, 232.0, 30.0)
            pullsDown:NO];
    stylePopup(_channelPopup);
    [leftPanel addSubview:_channelPopup];

    [leftPanel addSubview:makeLabel(@"Buffer", NSMakeRect(16.0, 224.0, 90.0, 18.0), 11.0, NSFontWeightMedium, mutedTextColor())];
    _bufferPopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(14.0, 190.0, 232.0, 30.0)
            pullsDown:NO];
    [_bufferPopup addItemsWithTitles:@[ @"64 samples", @"128 samples", @"256 samples" ]];
    [_bufferPopup selectItemAtIndex:0];
    stylePopup(_bufferPopup);
    [leftPanel addSubview:_bufferPopup];

    _startButton = makeButton(@"START AUDIO",
                              NSMakeRect(16.0, 132.0, 108.0, 38.0),
                              self,
                              @selector(startAudio:));
    _startButton.contentTintColor = liveColor();
    _stopButton = makeButton(@"STOP",
                             NSMakeRect(136.0, 132.0, 108.0, 38.0),
                             self,
                             @selector(stopAudio:));
    [leftPanel addSubview:_startButton];
    [leftPanel addSubview:_stopButton];

    NSTextField* warning = makeLabel(@"Start with interface / amp volume low.",
                                     NSMakeRect(16.0, 82.0, 228.0, 34.0),
                                     10.5,
                                     NSFontWeightRegular,
                                     cpColor(0.88, 0.46, 0.32));
    warning.usesSingleLineMode = NO;
    [leftPanel addSubview:warning];

    CircuitPedalPanelView* centerPanel = [[CircuitPedalPanelView alloc]
        initWithFrame:NSMakeRect(290.0, 90.0, 580.0, 596.0)];
    [content addSubview:centerPanel];

    _pedalTitle = makeLabel(@"Built-in Distortion+",
                            NSMakeRect(28.0, 548.0, 524.0, 28.0),
                            21.0,
                            NSFontWeightSemibold);
    _pedalTitle.alignment = NSTextAlignmentCenter;
    [centerPanel addSubview:_pedalTitle];
    _pedalSubtitle = makeLabel(@"REFERENCE DISTORTION MODEL",
                               NSMakeRect(28.0, 529.0, 524.0, 16.0),
                               9.5,
                               NSFontWeightMedium,
                               mutedTextColor());
    _pedalSubtitle.alignment = NSTextAlignmentCenter;
    _pedalSubtitle.font = [NSFont monospacedSystemFontOfSize:9.5 weight:NSFontWeightMedium];
    [centerPanel addSubview:_pedalSubtitle];

    CircuitPedalFaceView* pedalFace = [[CircuitPedalFaceView alloc]
        initWithFrame:NSMakeRect(20.0, 20.0, 540.0, 496.0)];
    [centerPanel addSubview:pedalFace];

    _diodeLabel = makeSectionLabel(@"CLIPPING DIODES", NSMakeRect(36.0, 428.0, 150.0, 18.0));
    [pedalFace addSubview:_diodeLabel];
    _diodePopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(32.0, 392.0, 476.0, 30.0)
            pullsDown:NO];
    [_diodePopup addItemsWithTitles:@[
        @"Reference germanium (V0.2)",
        @"Silicon-like (experimental)",
        @"LED-like (experimental)",
        @"No clipping diodes"
    ]];
    [_diodePopup selectItemAtIndex:0];
    stylePopup(_diodePopup);
    [pedalFace addSubview:_diodePopup];

    _distortionLabel = makeLabel(@"DISTORTION",
                                 NSMakeRect(92.0, 338.0, 130.0, 20.0),
                                 11.0,
                                 NSFontWeightSemibold,
                                 textColor());
    _distortionLabel.alignment = NSTextAlignmentCenter;
    [pedalFace addSubview:_distortionLabel];

    _distortionSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(96.0, 205.0, 122.0, 122.0)];
    _distortionSlider.doubleValue = 65.0;
    _distortionSlider.target = self;
    _distortionSlider.action = @selector(distortionSliderChanged:);
    styleRotarySlider(_distortionSlider);
    [pedalFace addSubview:_distortionSlider];
    _distortionValue = makeLabel(@"65%",
                                 NSMakeRect(92.0, 180.0, 130.0, 22.0),
                                 14.0,
                                 NSFontWeightMedium,
                                 accentColor());
    _distortionValue.alignment = NSTextAlignmentCenter;
    [pedalFace addSubview:_distortionValue];

    _outputControlLabel = makeLabel(@"OUTPUT",
                                    NSMakeRect(318.0, 338.0, 130.0, 20.0),
                                    11.0,
                                    NSFontWeightSemibold,
                                    textColor());
    _outputControlLabel.alignment = NSTextAlignmentCenter;
    [pedalFace addSubview:_outputControlLabel];
    _outputSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(322.0, 205.0, 122.0, 122.0)];
    _outputSlider.doubleValue = 70.0;
    _outputSlider.target = self;
    _outputSlider.action = @selector(outputSliderChanged:);
    styleRotarySlider(_outputSlider);
    [pedalFace addSubview:_outputSlider];
    _outputValue = makeLabel(@"70%",
                             NSMakeRect(318.0, 180.0, 130.0, 22.0),
                             14.0,
                             NSFontWeightMedium,
                             accentColor());
    _outputValue.alignment = NSTextAlignmentCenter;
    [pedalFace addSubview:_outputValue];

    _circuitScrollView = [[NSScrollView alloc]
        initWithFrame:NSMakeRect(22.0, 110.0, 496.0, 320.0)];
    _circuitScrollView.hasVerticalScroller = YES;
    _circuitScrollView.hasHorizontalScroller = NO;
    _circuitScrollView.autohidesScrollers = YES;
    _circuitScrollView.borderType = NSNoBorder;
    _circuitScrollView.drawsBackground = NO;

    _circuitDocumentView = [[CircuitPedalFlippedView alloc]
        initWithFrame:NSMakeRect(0.0, 0.0, 476.0, 320.0)];
    _circuitScrollView.documentView = _circuitDocumentView;
    [pedalFace addSubview:_circuitScrollView];

    for (NSInteger i = 0; i < 16; ++i)
    {
        const NSInteger column = i % 4;
        const NSInteger row = i / 4;
        const double x = 6.0 + static_cast<double>(column) * 118.0;
        const double y = 8.0 + static_cast<double>(row) * 132.0;

        _circuitLabels[i] = makeLabel(@"CONTROL",
                                      NSMakeRect(x, y, 110.0, 18.0),
                                      10.0,
                                      NSFontWeightSemibold,
                                      textColor());
        _circuitLabels[i].alignment = NSTextAlignmentCenter;
        [_circuitDocumentView addSubview:_circuitLabels[i]];

        _circuitSliders[i] = [[NSSlider alloc]
            initWithFrame:NSMakeRect(x + 18.0, y + 24.0, 74.0, 74.0)];
        _circuitSliders[i].doubleValue = 50.0;
        _circuitSliders[i].target = self;
        _circuitSliders[i].action = @selector(circuitSliderChanged:);
        _circuitSliders[i].tag = i;
        styleRotarySlider(_circuitSliders[i]);
        [_circuitDocumentView addSubview:_circuitSliders[i]];

        _circuitSwitchPopups[i] = [[NSPopUpButton alloc]
            initWithFrame:NSMakeRect(x, y + 46.0, 110.0, 30.0)
                pullsDown:NO];
        _circuitSwitchPopups[i].target = self;
        _circuitSwitchPopups[i].action = @selector(circuitSwitchChanged:);
        _circuitSwitchPopups[i].tag = i;
        stylePopup(_circuitSwitchPopups[i]);
        _circuitSwitchPopups[i].hidden = YES;
        [_circuitDocumentView addSubview:_circuitSwitchPopups[i]];

        _circuitValues[i] = makeLabel(@"50%",
                                      NSMakeRect(x, y + 101.0, 110.0, 18.0),
                                      11.0,
                                      NSFontWeightMedium,
                                      accentColor());
        _circuitValues[i].alignment = NSTextAlignmentCenter;
        [_circuitDocumentView addSubview:_circuitValues[i]];
    }

    _bypassButton = makeButton(@"ACTIVE",
                               NSMakeRect(170.0, 54.0, 200.0, 48.0),
                               self,
                               @selector(bypassChanged:));
    _bypassButton.buttonType = NSButtonTypePushOnPushOff;
    _bypassButton.contentTintColor = liveColor();
    [pedalFace addSubview:_bypassButton];
    NSTextField* bypassCaption = makeSectionLabel(@"FOOTSWITCH / BYPASS",
                                                  NSMakeRect(170.0, 30.0, 200.0, 18.0));
    bypassCaption.alignment = NSTextAlignmentCenter;
    [pedalFace addSubview:bypassCaption];

    CircuitPedalPanelView* rightPanel = [[CircuitPedalPanelView alloc]
        initWithFrame:NSMakeRect(884.0, 90.0, 280.0, 596.0)];
    [content addSubview:rightPanel];

    [rightPanel addSubview:makeSectionLabel(@"SIGNAL", NSMakeRect(16.0, 552.0, 120.0, 20.0))];
    [rightPanel addSubview:makeLabel(@"INPUT", NSMakeRect(16.0, 516.0, 70.0, 18.0), 10.5, NSFontWeightSemibold, mutedTextColor())];
    _inputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(178.0, 516.0, 84.0, 18.0), 10.5, NSFontWeightMedium, textColor());
    _inputDbLabel.alignment = NSTextAlignmentRight;
    [rightPanel addSubview:_inputDbLabel];
    _inputMeter = [[CircuitPedalMeterView alloc]
        initWithFrame:NSMakeRect(16.0, 490.0, 246.0, 18.0)];
    [rightPanel addSubview:_inputMeter];

    [rightPanel addSubview:makeLabel(@"OUTPUT", NSMakeRect(16.0, 452.0, 70.0, 18.0), 10.5, NSFontWeightSemibold, mutedTextColor())];
    _outputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(178.0, 452.0, 84.0, 18.0), 10.5, NSFontWeightMedium, textColor());
    _outputDbLabel.alignment = NSTextAlignmentRight;
    [rightPanel addSubview:_outputDbLabel];
    _outputMeter = [[CircuitPedalMeterView alloc]
        initWithFrame:NSMakeRect(16.0, 426.0, 246.0, 18.0)];
    [rightPanel addSubview:_outputMeter];

    [rightPanel addSubview:makeSectionLabel(@"ENGINE STATUS", NSMakeRect(16.0, 374.0, 180.0, 20.0))];
    _statusLabel = makeLabel(@"Audio stopped.\n\nChoose a model and audio device, then press Start Audio.",
                             NSMakeRect(16.0, 132.0, 246.0, 232.0),
                             11.0,
                             NSFontWeightRegular,
                             cpColor(0.72, 0.75, 0.78));
    _statusLabel.font = [NSFont monospacedSystemFontOfSize:11.0 weight:NSFontWeightRegular];
    _statusLabel.usesSingleLineMode = NO;
    _statusLabel.lineBreakMode = NSLineBreakByWordWrapping;
    [rightPanel addSubview:_statusLabel];

    NSTextField* stablePath = makeLabel(@"LIVE GENERIC PATH",
                                        NSMakeRect(16.0, 82.0, 130.0, 18.0),
                                        9.5,
                                        NSFontWeightSemibold,
                                        mutedTextColor());
    stablePath.font = [NSFont monospacedSystemFontOfSize:9.5 weight:NSFontWeightSemibold];
    [rightPanel addSubview:stablePath];
    NSTextField* stableValue = makeLabel(@"1×  •  STABLE",
                                         NSMakeRect(146.0, 82.0, 116.0, 18.0),
                                         10.5,
                                         NSFontWeightSemibold,
                                         liveColor());
    stableValue.alignment = NSTextAlignmentRight;
    [rightPanel addSubview:stableValue];

    CircuitPedalPanelView* footer = [[CircuitPedalPanelView alloc]
        initWithFrame:NSMakeRect(16.0, 16.0, 1148.0, 58.0)];
    [content addSubview:footer];
    _errorLabel = makeLabel(@"Ready.",
                            NSMakeRect(16.0, 12.0, 1116.0, 34.0),
                            11.0,
                            NSFontWeightRegular,
                            mutedTextColor());
    _errorLabel.usesSingleLineMode = NO;
    _errorLabel.lineBreakMode = NSLineBreakByWordWrapping;
    [footer addSubview:_errorLabel];

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

    NSMutableArray<NSURL*>* urls = [NSMutableArray array];
    NSArray<NSURL*>* bundled =
        [[NSBundle mainBundle] URLsForResourcesWithExtension:@"cpedal"
                                                subdirectory:@"circuits"];
    if (bundled != nil)
        [urls addObjectsFromArray:bundled];

    struct LibraryEntry {
        std::string name;
        std::string path;
    };
    std::vector<LibraryEntry> entries;
    entries.reserve(urls.count);

    for (NSURL* url in urls)
    {
        const char* utf8Path = url.path.UTF8String;
        if (utf8Path == nullptr)
            continue;

        circuitpedal::CircuitFileDocument document;
        std::string error;
        if (!circuitpedal::loadCircuitFile(utf8Path, document, error))
            continue;

        entries.push_back({ document.name, utf8Path });
    }

    std::sort(entries.begin(), entries.end(),
              [](const LibraryEntry& a, const LibraryEntry& b) {
                  return a.name < b.name;
              });

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
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue = nsString(error);
        [_circuitLibraryPopup selectItemAtIndex:0];
        NSBeep();
        return;
    }

    _bypassButton.state = NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Circuit selected. Start Audio when ready.";
    _statusLabel.stringValue =
        @"Circuit selected from the built-in library.\n\nStart Audio to compile its operating point and begin real-time processing.";
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
        NSString* title = [NSString stringWithFormat:@"%@ — %u in / %u out",
                           nsString(device.name),
                           device.inputChannels,
                           device.outputChannels];
        [_devicePopup addItemWithTitle:title];
    }

    if (!error.empty())
    {
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue = nsString(error);
    }
    else if (_devices.empty())
    {
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue =
            @"No duplex audio device was found. Connect an interface with both input and output, then relaunch CircuitPedal.";
    }
    else
    {
        std::string defaultError;
        const std::uint32_t defaultId =
            circuitpedal::MacAudioEngine::defaultOutputDeviceId(defaultError);
        const auto preferred = std::find_if(
            _devices.begin(),
            _devices.end(),
            [defaultId](const circuitpedal::AudioDeviceInfo& device) {
                return device.id == defaultId;
            });
        const NSInteger row = preferred == _devices.end()
            ? 0
            : static_cast<NSInteger>(std::distance(_devices.begin(), preferred));
        [_devicePopup selectItemAtIndex:row];
        _errorLabel.textColor = mutedTextColor();
        _errorLabel.stringValue = @"Ready. Select a model and start audio.";
    }

    [self updateInputChannels];
}

- (void)updateInputChannels
{
    [_channelPopup removeAllItems];
    const NSInteger row = _devicePopup.indexOfSelectedItem;
    if (row < 0 || static_cast<std::size_t>(row) >= _devices.size())
        return;

    const std::uint32_t channelCount =
        _devices[static_cast<std::size_t>(row)].inputChannels;
    for (std::uint32_t channel = 1; channel <= channelCount; ++channel)
    {
        [_channelPopup addItemWithTitle:
            [NSString stringWithFormat:@"Input %u", channel]];
    }
    if (channelCount > 0)
        [_channelPopup selectItemAtIndex:0];
}

- (void)deviceChanged:(id)sender
{
    (void)sender;
    [self updateInputChannels];
}

- (void)refreshModelControls
{
    const BOOL generic = _engine->usingCircuitFile();
    NSString* activeName = nsString(_engine->activeModelName());
    _modelValue.stringValue = activeName;
    _pedalTitle.stringValue = activeName;
    _pedalSubtitle.stringValue = generic
        ? @"GENERIC CIRCUIT MODEL  •  1× LIVE PATH"
        : @"REFERENCE DISTORTION MODEL";

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

    const std::size_t visibleControlCount =
        std::min<std::size_t>(_circuitControls.size(), 16);
    const std::size_t rows = (visibleControlCount + 3U) / 4U;
    const double documentHeight =
        std::max(320.0, 12.0 + 132.0 * static_cast<double>(rows));
    _circuitDocumentView.frame =
        NSMakeRect(0.0, 0.0, 476.0, documentHeight);

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

            const std::uint32_t count =
                std::max<std::uint32_t>(2U, control.switchPositionCount);
            const double normalized = static_cast<double>(_engine->circuitControl(i));
            const NSInteger position = static_cast<NSInteger>(
                std::llround(normalized * static_cast<double>(count - 1U)));
            if (_circuitSwitchPopups[i].numberOfItems > 0)
            {
                [_circuitSwitchPopups[i] selectItemAtIndex:
                    std::clamp<NSInteger>(position,
                                          0,
                                          _circuitSwitchPopups[i].numberOfItems - 1)];
            }
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

    _startButton.enabled =
        !running && !_devices.empty() && _channelPopup.numberOfItems > 0;
    _stopButton.enabled = running;
    _liveDot.active = running;
    _liveText.stringValue = running ? @"LIVE AUDIO" : @"AUDIO STOPPED";
    _liveText.textColor = running ? liveColor() : mutedTextColor();
}

- (void)refreshBypassAppearance
{
    const BOOL bypassed = _bypassButton.state == NSControlStateValueOn;
    _bypassButton.title = bypassed ? @"BYPASSED" : @"ACTIVE";
    _bypassButton.contentTintColor = bypassed ? accentColor() : liveColor();
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
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue = @"Could not read the selected file path.";
        return;
    }

    std::string error;
    if (!_engine->loadCircuitFile(utf8Path, error))
    {
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue = nsString(error);
        NSBeep();
        return;
    }

    [_circuitLibraryPopup selectItemAtIndex:0];
    _bypassButton.state = NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"External circuit loaded. Start Audio when ready.";
    _statusLabel.stringValue =
        @"External circuit loaded.\n\nStart Audio to compile its DC operating point and begin real-time processing.";
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
    _bypassButton.state =
        _engine->bypassed() ? NSControlStateValueOn : NSControlStateValueOff;
    _errorLabel.textColor = mutedTextColor();
    _errorLabel.stringValue = @"Built-in Distortion+ selected.";
    _statusLabel.stringValue = @"Built-in Distortion+ selected.\n\nStart Audio when ready.";
    [self refreshModelControls];
    [self setRunningControls:NO];
    [self refreshBypassAppearance];
}

- (void)startAudio:(id)sender
{
    (void)sender;
    const NSInteger deviceRow = _devicePopup.indexOfSelectedItem;
    const NSInteger channelRow = _channelPopup.indexOfSelectedItem;
    if (deviceRow < 0
        || static_cast<std::size_t>(deviceRow) >= _devices.size()
        || channelRow < 0)
    {
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
        _errorLabel.stringValue = @"Select a duplex device and an input channel before starting.";
        return;
    }

    circuitpedal::AudioStartConfiguration configuration;
    configuration.deviceId = _devices[static_cast<std::size_t>(deviceRow)].id;
    configuration.inputChannel = static_cast<std::uint32_t>(channelRow);
    const NSInteger bufferIndex = std::max<NSInteger>(0, _bufferPopup.indexOfSelectedItem);
    const std::uint32_t bufferValues[] = { 64U, 128U, 256U };
    configuration.requestedBufferFrames =
        bufferValues[std::min<NSInteger>(bufferIndex, 2)];

    if (!_engine->usingCircuitFile())
    {
        _engine->setDistortion(static_cast<float>(_distortionSlider.doubleValue / 100.0));
        _engine->setOutput(static_cast<float>(_outputSlider.doubleValue / 100.0));
        const NSInteger diodeRow = _diodePopup.indexOfSelectedItem;
        const auto diodePreset = static_cast<circuitpedal::ClippingDiodePreset>(
            diodeRow >= 0 ? static_cast<std::uint32_t>(diodeRow) : 0U);
        _engine->setClippingDiodePreset(diodePreset);
    }
    _engine->setBypass(_bypassButton.state == NSControlStateValueOn);

    std::string error;
    if (!_engine->start(configuration, error))
    {
        _errorLabel.textColor = cpColor(0.95, 0.38, 0.32);
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
    _statusLabel.stringValue = @"Audio stopped.\n\nSettings are unlocked.";
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
    (void)_engine->setCircuitControl(static_cast<std::size_t>(index),
                                     static_cast<float>(value / 100.0));
}

- (void)circuitSwitchChanged:(id)sender
{
    NSPopUpButton* popup = (NSPopUpButton*)sender;
    const NSInteger index = popup.tag;
    if (index < 0
        || index >= 16
        || static_cast<std::size_t>(index) >= _circuitControls.size())
    {
        return;
    }

    const auto& control = _circuitControls[static_cast<std::size_t>(index)];
    const std::uint32_t count = std::max<std::uint32_t>(2U, control.switchPositionCount);
    const NSInteger selected = popup.indexOfSelectedItem;
    if (selected < 0)
        return;

    const float normalized = static_cast<float>(selected)
        / static_cast<float>(count - 1U);
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
}

- (void)updateRunningStatus
{
    const circuitpedal::AudioRuntimeInfo info = _engine->runtimeInfo();
    if (_engine->usingCircuitFile())
    {
        _statusLabel.stringValue = [NSString stringWithFormat:
            @"LIVE AUDIO\n\n%@\n\n"
             "Sample rate   %.1f kHz\n"
             "Buffer        %u frames\n"
             "Buffer time   %.2f ms\n"
             "Latency sum   %.2f ms\n\n"
             "Circuit path  1× stable live",
            nsString(_engine->activeModelName()),
            info.sampleRate / 1000.0,
            info.actualBufferFrames,
            info.bufferDurationMilliseconds(),
            info.reportedLatencyMilliseconds()];
    }
    else
    {
        _statusLabel.stringValue = [NSString stringWithFormat:
            @"LIVE AUDIO\n\nBuilt-in Distortion+\n\n"
             "Diodes        %s\n"
             "Sample rate   %.1f kHz\n"
             "Buffer        %u frames\n"
             "Buffer time   %.2f ms\n"
             "Latency sum   %.2f ms",
            circuitpedal::clippingDiodePresetName(_engine->clippingDiodePreset()),
            info.sampleRate / 1000.0,
            info.actualBufferFrames,
            info.bufferDurationMilliseconds(),
            info.reportedLatencyMilliseconds()];
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
        NSMenuItem* quitItem = [[NSMenuItem alloc]
            initWithTitle:@"Quit CircuitPedal"
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
