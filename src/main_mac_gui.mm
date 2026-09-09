#import <Cocoa/Cocoa.h>

#include "MacAudioEngine.h"

#include <algorithm>
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

NSTextField* makeLabel(NSString* text, NSRect frame)
{
    NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text;
    label.editable = NO;
    label.selectable = NO;
    label.bezeled = NO;
    label.drawsBackground = NO;
    return label;
}

NSButton* makeButton(NSString* title, NSRect frame, id target, SEL action)
{
    NSButton* button = [[NSButton alloc] initWithFrame:frame];
    button.title = title;
    button.bezelStyle = NSBezelStyleRounded;
    button.target = target;
    button.action = action;
    return button;
}

} // namespace

@interface CircuitPedalAppDelegate : NSObject <NSApplicationDelegate> {
@private
    std::unique_ptr<circuitpedal::MacAudioEngine> _engine;
    std::vector<circuitpedal::AudioDeviceInfo> _devices;
    NSWindow* _window;
    NSPopUpButton* _devicePopup;
    NSPopUpButton* _channelPopup;
    NSPopUpButton* _bufferPopup;
    NSButton* _startButton;
    NSButton* _stopButton;
    NSSlider* _distortionSlider;
    NSSlider* _outputSlider;
    NSTextField* _distortionValue;
    NSTextField* _outputValue;
    NSButton* _bypassButton;
    NSProgressIndicator* _inputMeter;
    NSProgressIndicator* _outputMeter;
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

    const NSRect windowRect = NSMakeRect(0.0, 0.0, 640.0, 590.0);
    const NSWindowStyleMask style = NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    _window = [[NSWindow alloc] initWithContentRect:windowRect
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"CircuitPedal V0.3";
    _window.releasedWhenClosed = NO;
    NSView* content = _window.contentView;

    NSTextField* title = makeLabel(@"CircuitPedal V0.3 — macOS Test GUI",
                                   NSMakeRect(24.0, 542.0, 590.0, 28.0));
    title.font = [NSFont systemFontOfSize:20.0 weight:NSFontWeightSemibold];
    [content addSubview:title];

    NSTextField* warning = makeLabel(@"Start with your interface, headphones or amplifier volume low.",
                                     NSMakeRect(24.0, 516.0, 590.0, 20.0));
    warning.textColor = [NSColor systemRedColor];
    [content addSubview:warning];

    [content addSubview:makeLabel(@"Audio Device", NSMakeRect(24.0, 476.0, 110.0, 22.0))];
    _devicePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(140.0, 472.0, 472.0, 28.0)
                                              pullsDown:NO];
    _devicePopup.target = self;
    _devicePopup.action = @selector(deviceChanged:);
    [content addSubview:_devicePopup];

    [content addSubview:makeLabel(@"Input Channel", NSMakeRect(24.0, 434.0, 110.0, 22.0))];
    _channelPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(140.0, 430.0, 170.0, 28.0)
                                               pullsDown:NO];
    [content addSubview:_channelPopup];

    [content addSubview:makeLabel(@"Buffer Size", NSMakeRect(336.0, 434.0, 90.0, 22.0))];
    _bufferPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(430.0, 430.0, 182.0, 28.0)
                                              pullsDown:NO];
    [_bufferPopup addItemsWithTitles:@[ @"64", @"128", @"256" ]];
    [_bufferPopup selectItemWithTitle:@"64"];
    [content addSubview:_bufferPopup];

    _startButton = makeButton(@"Start Audio", NSMakeRect(140.0, 380.0, 150.0, 32.0),
                              self, @selector(startAudio:));
    _stopButton = makeButton(@"Stop Audio", NSMakeRect(320.0, 380.0, 150.0, 32.0),
                             self, @selector(stopAudio:));
    [content addSubview:_startButton];
    [content addSubview:_stopButton];

    [content addSubview:makeLabel(@"Distortion", NSMakeRect(24.0, 329.0, 100.0, 22.0))];
    _distortionSlider = [NSSlider sliderWithValue:65.0
                                            minValue:0.0
                                            maxValue:100.0
                                               target:self
                                               action:@selector(distortionSliderChanged:)];
    _distortionSlider.frame = NSMakeRect(140.0, 325.0, 390.0, 28.0);
    _distortionSlider.continuous = YES;
    [content addSubview:_distortionSlider];
    _distortionValue = makeLabel(@"65%", NSMakeRect(548.0, 329.0, 64.0, 22.0));
    _distortionValue.alignment = NSTextAlignmentRight;
    [content addSubview:_distortionValue];

    [content addSubview:makeLabel(@"Output", NSMakeRect(24.0, 287.0, 100.0, 22.0))];
    _outputSlider = [NSSlider sliderWithValue:70.0
                                    minValue:0.0
                                    maxValue:100.0
                                       target:self
                                       action:@selector(outputSliderChanged:)];
    _outputSlider.frame = NSMakeRect(140.0, 283.0, 390.0, 28.0);
    _outputSlider.continuous = YES;
    [content addSubview:_outputSlider];
    _outputValue = makeLabel(@"70%", NSMakeRect(548.0, 287.0, 64.0, 22.0));
    _outputValue.alignment = NSTextAlignmentRight;
    [content addSubview:_outputValue];

    _bypassButton = [NSButton checkboxWithTitle:@"Bypass"
                                         target:self
                                         action:@selector(bypassChanged:)];
    _bypassButton.frame = NSMakeRect(140.0, 242.0, 150.0, 28.0);
    [content addSubview:_bypassButton];

    [content addSubview:makeLabel(@"Input Level", NSMakeRect(24.0, 200.0, 100.0, 22.0))];
    _inputMeter = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(140.0, 202.0, 472.0, 16.0)];
    _inputMeter.indeterminate = NO;
    _inputMeter.style = NSProgressIndicatorStyleBar;
    _inputMeter.minValue = 0.0;
    _inputMeter.maxValue = 1.0;
    [content addSubview:_inputMeter];

    [content addSubview:makeLabel(@"Output Level", NSMakeRect(24.0, 166.0, 100.0, 22.0))];
    _outputMeter = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(140.0, 168.0, 472.0, 16.0)];
    _outputMeter.indeterminate = NO;
    _outputMeter.style = NSProgressIndicatorStyleBar;
    _outputMeter.minValue = 0.0;
    _outputMeter.maxValue = 1.0;
    [content addSubview:_outputMeter];

    _statusLabel = makeLabel(@"Audio Stopped", NSMakeRect(24.0, 60.0, 588.0, 92.0));
    _statusLabel.font = [NSFont monospacedSystemFontOfSize:12.0 weight:NSFontWeightRegular];
    _statusLabel.usesSingleLineMode = NO;
    [content addSubview:_statusLabel];

    _errorLabel = makeLabel(@"", NSMakeRect(24.0, 16.0, 588.0, 42.0));
    _errorLabel.textColor = [NSColor systemRedColor];
    _errorLabel.usesSingleLineMode = NO;
    [content addSubview:_errorLabel];

    [self populateDevices];
    [self setRunningControls:NO];
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
                           nsString(device.name), device.inputChannels, device.outputChannels];
        [_devicePopup addItemWithTitle:title];
    }

    if (!error.empty())
    {
        _errorLabel.stringValue = nsString(error);
    }
    else if (_devices.empty())
    {
        _errorLabel.stringValue = @"No duplex audio device was found. Connect an interface with both input and output, then relaunch the test GUI.";
    }
    else
    {
        std::string defaultError;
        const std::uint32_t defaultId =
            circuitpedal::MacAudioEngine::defaultOutputDeviceId(defaultError);
        const auto preferred = std::find_if(_devices.begin(), _devices.end(),
            [defaultId](const circuitpedal::AudioDeviceInfo& device) {
                return device.id == defaultId;
            });
        const NSInteger row = preferred == _devices.end()
            ? 0
            : static_cast<NSInteger>(std::distance(_devices.begin(), preferred));
        [_devicePopup selectItemAtIndex:row];
        _errorLabel.stringValue = @"";
    }

    [self updateInputChannels];
}

- (void)updateInputChannels
{
    [_channelPopup removeAllItems];
    const NSInteger row = _devicePopup.indexOfSelectedItem;
    if (row < 0 || static_cast<std::size_t>(row) >= _devices.size())
        return;

    const std::uint32_t channelCount = _devices[static_cast<std::size_t>(row)].inputChannels;
    for (std::uint32_t channel = 1; channel <= channelCount; ++channel)
    {
        [_channelPopup addItemWithTitle:[NSString stringWithFormat:@"Input %u", channel]];
    }
    if (channelCount > 0)
        [_channelPopup selectItemAtIndex:0];
}

- (void)deviceChanged:(id)sender
{
    (void)sender;
    [self updateInputChannels];
}

- (void)setRunningControls:(BOOL)running
{
    _devicePopup.enabled = !running && !_devices.empty();
    _channelPopup.enabled = !running && _channelPopup.numberOfItems > 0;
    _bufferPopup.enabled = !running;
    _startButton.enabled = !running && !_devices.empty() && _channelPopup.numberOfItems > 0;
    _stopButton.enabled = running;
}

- (void)startAudio:(id)sender
{
    (void)sender;
    const NSInteger deviceRow = _devicePopup.indexOfSelectedItem;
    const NSInteger channelRow = _channelPopup.indexOfSelectedItem;
    if (deviceRow < 0 || static_cast<std::size_t>(deviceRow) >= _devices.size()
        || channelRow < 0)
    {
        _errorLabel.stringValue = @"Select a duplex device and an input channel before starting.";
        return;
    }

    circuitpedal::AudioStartConfiguration configuration;
    configuration.deviceId = _devices[static_cast<std::size_t>(deviceRow)].id;
    configuration.inputChannel = static_cast<std::uint32_t>(channelRow);
    configuration.requestedBufferFrames =
        static_cast<std::uint32_t>(_bufferPopup.titleOfSelectedItem.intValue);

    _engine->setDistortion(static_cast<float>(_distortionSlider.doubleValue / 100.0));
    _engine->setOutput(static_cast<float>(_outputSlider.doubleValue / 100.0));
    _engine->setBypass(_bypassButton.state == NSControlStateValueOn);

    std::string error;
    if (!_engine->start(configuration, error))
    {
        _errorLabel.stringValue = nsString(error);
        _statusLabel.stringValue = @"Audio Stopped";
        [self setRunningControls:NO];
        NSBeep();
        return;
    }

    _errorLabel.stringValue = @"";
    [self setRunningControls:YES];
    [self updateRunningStatus];
}

- (void)stopAudio:(id)sender
{
    (void)sender;
    _engine->stop();
    _inputMeter.doubleValue = 0.0;
    _outputMeter.doubleValue = 0.0;
    _statusLabel.stringValue = @"Audio Stopped";
    _errorLabel.stringValue = @"";
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

- (void)bypassChanged:(id)sender
{
    (void)sender;
    _engine->setBypass(_bypassButton.state == NSControlStateValueOn);
}

- (void)updateMeters:(NSTimer*)timer
{
    (void)timer;
    if (!_engine->isRunning())
        return;
    _inputMeter.doubleValue = std::clamp(static_cast<double>(_engine->inputPeak()), 0.0, 1.0);
    _outputMeter.doubleValue = std::clamp(static_cast<double>(_engine->outputPeak()), 0.0, 1.0);
}

- (void)updateRunningStatus
{
    const circuitpedal::AudioRuntimeInfo info = _engine->runtimeInfo();
    _statusLabel.stringValue = [NSString stringWithFormat:
        @"Audio Running @ %.1f kHz\nRequested buffer: %u | Actual: %u (%.2f ms)\nInput latency: %u + %u safety frames | Output: %u + %u safety frames\nDSP FIR delay: %u frames | Reported component sum: %.2f ms",
        info.sampleRate / 1000.0,
        info.requestedBufferFrames,
        info.actualBufferFrames,
        info.bufferDurationMilliseconds(),
        info.inputLatencyFrames,
        info.inputSafetyOffsetFrames,
        info.outputLatencyFrames,
        info.outputSafetyOffsetFrames,
        info.dspDelayFrames,
        info.reportedLatencyMilliseconds()];
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
