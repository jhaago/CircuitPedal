#import <Cocoa/Cocoa.h>

#include "GenericCircuitProcessor.h"

#include <cstdint>

namespace {

NSString* modeTitle(circuitpedal::GenericProcessingMode mode)
{
    return mode == circuitpedal::GenericProcessingMode::FourX
        ? @"4× oversampled"
        : @"1× pre-V0.8 path";
}

NSTextField* diagnosticLabel(NSString* text, NSRect frame)
{
    NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text;
    label.editable = NO;
    label.selectable = YES;
    label.bezeled = NO;
    label.drawsBackground = NO;
    return label;
}

} // namespace

@interface CircuitPedalDiagnosticOverlay : NSObject {
@private
    NSPanel* _panel;
    NSPopUpButton* _modePopup;
    NSTextField* _instructionLabel;
    NSTextField* _diagnosticLabel;
    NSTimer* _timer;
}
- (void)show;
@end

@implementation CircuitPedalDiagnosticOverlay

+ (void)load
{
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(circuitPedalApplicationDidFinishLaunching:)
               name:NSApplicationDidFinishLaunchingNotification
             object:nil];
}

+ (void)circuitPedalApplicationDidFinishLaunching:(NSNotification*)notification
{
    (void)notification;
    static CircuitPedalDiagnosticOverlay* overlay = nil;
    if (overlay == nil)
        overlay = [[CircuitPedalDiagnosticOverlay alloc] init];
    [overlay show];
}

- (instancetype)init
{
    self = [super init];
    if (self == nil)
        return nil;

    // Start this diagnostic build in the known-good 1x-style processing mode.
    circuitpedal::setGenericProcessingModeForDiagnostics(
        circuitpedal::GenericProcessingMode::OneX);
    circuitpedal::resetGenericProcessorDiagnostics();

    _panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(0.0, 0.0, 520.0, 290.0)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    _panel.title = @"CircuitPedal Runtime Diagnostics";
    _panel.releasedWhenClosed = NO;
    _panel.floatingPanel = YES;

    NSView* content = _panel.contentView;

    NSTextField* heading = diagnosticLabel(
        @"Generic circuit processing mode",
        NSMakeRect(20.0, 246.0, 230.0, 22.0));
    heading.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
    [content addSubview:heading];

    _modePopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(260.0, 240.0, 238.0, 30.0)
            pullsDown:NO];
    [_modePopup addItemsWithTitles:@[
        @"1× — pre-V0.8 live path",
        @"4× — oversampled path"
    ]];
    [_modePopup selectItemAtIndex:0];
    _modePopup.target = self;
    _modePopup.action = @selector(modeChanged:);
    [content addSubview:_modePopup];

    _instructionLabel = diagnosticLabel(
        @"Change mode only while audio is stopped, then press Start Audio. Counters reset on each start.",
        NSMakeRect(20.0, 205.0, 478.0, 38.0));
    _instructionLabel.usesSingleLineMode = NO;
    _instructionLabel.textColor = [NSColor secondaryLabelColor];
    [content addSubview:_instructionLabel];

    _diagnosticLabel = diagnosticLabel(@"Waiting for audio start…",
                                       NSMakeRect(20.0, 58.0, 478.0, 142.0));
    _diagnosticLabel.font =
        [NSFont monospacedSystemFontOfSize:12.0 weight:NSFontWeightRegular];
    _diagnosticLabel.usesSingleLineMode = NO;
    [content addSubview:_diagnosticLabel];

    NSButton* resetButton = [[NSButton alloc]
        initWithFrame:NSMakeRect(348.0, 16.0, 150.0, 30.0)];
    resetButton.title = @"Reset Counters";
    resetButton.bezelStyle = NSBezelStyleRounded;
    resetButton.target = self;
    resetButton.action = @selector(resetCounters:);
    [content addSubview:resetButton];

    _timer = [NSTimer scheduledTimerWithTimeInterval:0.20
                                               target:self
                                             selector:@selector(refreshDiagnostics:)
                                             userInfo:nil
                                              repeats:YES];

    return self;
}

- (void)dealloc
{
    [_timer invalidate];
}

- (void)show
{
    [_panel center];
    [_panel orderFront:nil];
}

- (void)modeChanged:(id)sender
{
    (void)sender;
    const auto mode = _modePopup.indexOfSelectedItem == 1
        ? circuitpedal::GenericProcessingMode::FourX
        : circuitpedal::GenericProcessingMode::OneX;
    circuitpedal::setGenericProcessingModeForDiagnostics(mode);
    circuitpedal::resetGenericProcessorDiagnostics();
    _instructionLabel.stringValue =
        @"Mode selected. Stop/Start Audio in the main window for it to take effect.";
}

- (void)resetCounters:(id)sender
{
    (void)sender;
    circuitpedal::resetGenericProcessorDiagnostics();
}

- (void)refreshDiagnostics:(NSTimer*)timer
{
    (void)timer;
    const auto diagnostics = circuitpedal::genericProcessorDiagnostics();
    const BOOL pending = diagnostics.requestedMode != diagnostics.activeMode;

    NSString* pendingText = pending ? @"  (takes effect next Start Audio)" : @"";
    _diagnosticLabel.stringValue = [NSString stringWithFormat:
        @"Requested: %@%@\n"
         "Active:    %@\n"
         "Host samples: %llu\n"
         "Host solve failures: %llu\n"
         "Failed sub-solves:   %llu\n"
         "Timing samples: %llu (1 of every 64 host samples)\n"
         "DSP budget misses: %llu\n"
         "Max measured sample: %.1f µs  |  Budget: %.1f µs",
        modeTitle(diagnostics.requestedMode),
        pendingText,
        modeTitle(diagnostics.activeMode),
        static_cast<unsigned long long>(diagnostics.hostSamplesProcessed),
        static_cast<unsigned long long>(diagnostics.hostSolveFailures),
        static_cast<unsigned long long>(diagnostics.subSolveFailures),
        static_cast<unsigned long long>(diagnostics.timedSamples),
        static_cast<unsigned long long>(diagnostics.sampleBudgetMisses),
        diagnostics.maximumProcessMicroseconds,
        diagnostics.sampleBudgetMicroseconds];
}

@end
