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
    std::vector<circuitpedal::CircuitFileControl> _circuitControls;

    NSWindow* _window;
    NSPopUpButton* _devicePopup;
    NSPopUpButton* _channelPopup;
    NSPopUpButton* _bufferPopup;

    NSTextField* _modelValue;
    NSButton* _loadCircuitButton;
    NSButton* _builtinButton;

    NSTextField* _diodeLabel;
    NSPopUpButton* _diodePopup;
    NSTextField* _distortionLabel;
    NSSlider* _distortionSlider;
    NSTextField* _distortionValue;
    NSTextField* _outputControlLabel;
    NSSlider* _outputSlider;
    NSTextField* _outputValue;

    NSTextField* _circuitLabels[4];
    NSSlider* _circuitSliders[4];
    NSTextField* _circuitValues[4];

    NSButton* _startButton;
    NSButton* _stopButton;
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

    const NSRect windowRect = NSMakeRect(0.0, 0.0, 720.0, 840.0);
    const NSWindowStyleMask style = NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    _window = [[NSWindow alloc] initWithContentRect:windowRect
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"CircuitPedal V0.9";
    _window.releasedWhenClosed = NO;
    NSView* content = _window.contentView;

    NSTextField* title = makeLabel(@"CircuitPedal V0.9 — Circuit File Lab",
                                   NSMakeRect(24.0, 796.0, 672.0, 28.0));
    title.font = [NSFont systemFontOfSize:20.0 weight:NSFontWeightSemibold];
    [content addSubview:title];

    NSTextField* warning = makeLabel(
        @"Start with your interface, headphones or amplifier volume low.",
        NSMakeRect(24.0, 770.0, 672.0, 20.0));
    warning.textColor = [NSColor systemRedColor];
    [content addSubview:warning];

    [content addSubview:makeLabel(@"Effect Model", NSMakeRect(24.0, 730.0, 110.0, 22.0))];
    _modelValue = makeLabel(@"Built-in Distortion+",
                            NSMakeRect(140.0, 730.0, 280.0, 22.0));
    _modelValue.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium];
    _modelValue.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [content addSubview:_modelValue];

    _loadCircuitButton = makeButton(@"Load .cpedal…",
                                    NSMakeRect(430.0, 724.0, 126.0, 30.0),
                                    self,
                                    @selector(loadCircuit:));
    _builtinButton = makeButton(@"Use Distortion+",
                                NSMakeRect(566.0, 724.0, 130.0, 30.0),
                                self,
                                @selector(useBuiltin:));
    [content addSubview:_loadCircuitButton];
    [content addSubview:_builtinButton];

    [content addSubview:makeLabel(@"Audio Device", NSMakeRect(24.0, 682.0, 110.0, 22.0))];
    _devicePopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(140.0, 678.0, 556.0, 28.0)
            pullsDown:NO];
    _devicePopup.target = self;
    _devicePopup.action = @selector(deviceChanged:);
    [content addSubview:_devicePopup];

    [content addSubview:makeLabel(@"Input Channel", NSMakeRect(24.0, 640.0, 110.0, 22.0))];
    _channelPopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(140.0, 636.0, 200.0, 28.0)
            pullsDown:NO];
    [content addSubview:_channelPopup];

    [content addSubview:makeLabel(@"Buffer Size", NSMakeRect(376.0, 640.0, 90.0, 22.0))];
    _bufferPopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(470.0, 636.0, 226.0, 28.0)
            pullsDown:NO];
    [_bufferPopup addItemsWithTitles:@[ @"64", @"128", @"256" ]];
    [_bufferPopup selectItemWithTitle:@"64"];
    [content addSubview:_bufferPopup];

    _startButton = makeButton(@"Start Audio",
                              NSMakeRect(190.0, 586.0, 150.0, 32.0),
                              self,
                              @selector(startAudio:));
    _stopButton = makeButton(@"Stop Audio",
                             NSMakeRect(380.0, 586.0, 150.0, 32.0),
                             self,
                             @selector(stopAudio:));
    [content addSubview:_startButton];
    [content addSubview:_stopButton];

    _diodeLabel = makeLabel(@"Clipping Diodes", NSMakeRect(24.0, 538.0, 110.0, 22.0));
    [content addSubview:_diodeLabel];
    _diodePopup = [[NSPopUpButton alloc]
        initWithFrame:NSMakeRect(140.0, 534.0, 556.0, 28.0)
            pullsDown:NO];
    [_diodePopup addItemsWithTitles:@[
        @"Reference germanium (V0.2)",
        @"Silicon-like (experimental)",
        @"LED-like (experimental)",
        @"No clipping diodes"
    ]];
    [_diodePopup selectItemAtIndex:0];
    [content addSubview:_diodePopup];

    _distortionLabel = makeLabel(@"Distortion", NSMakeRect(24.0, 492.0, 100.0, 22.0));
    [content addSubview:_distortionLabel];
    _distortionSlider = [NSSlider sliderWithValue:65.0
                                            minValue:0.0
                                            maxValue:100.0
                                               target:self
                                               action:@selector(distortionSliderChanged:)];
    _distortionSlider.frame = NSMakeRect(140.0, 488.0, 470.0, 28.0);
    _distortionSlider.continuous = YES;
    [content addSubview:_distortionSlider];
    _distortionValue = makeLabel(@"65%", NSMakeRect(626.0, 492.0, 70.0, 22.0));
    _distortionValue.alignment = NSTextAlignmentRight;
    [content addSubview:_distortionValue];

    _outputControlLabel = makeLabel(@"Output", NSMakeRect(24.0, 450.0, 100.0, 22.0));
    [content addSubview:_outputControlLabel];
    _outputSlider = [NSSlider sliderWithValue:70.0
                                    minValue:0.0
                                    maxValue:100.0
                                       target:self
                                       action:@selector(outputSliderChanged:)];
    _outputSlider.frame = NSMakeRect(140.0, 446.0, 470.0, 28.0);
    _outputSlider.continuous = YES;
    [content addSubview:_outputSlider];
    _outputValue = makeLabel(@"70%", NSMakeRect(626.0, 450.0, 70.0, 22.0));
    _outputValue.alignment = NSTextAlignmentRight;
    [content addSubview:_outputValue];

    const double circuitY[4] = { 538.0, 496.0, 454.0, 412.0 };
    for (NSInteger i = 0; i < 4; ++i)
    {
        _circuitLabels[i] = makeLabel(@"Control",
                                      NSMakeRect(24.0, circuitY[i], 110.0, 22.0));
        [content addSubview:_circuitLabels[i]];

        _circuitSliders[i] = [NSSlider sliderWithValue:50.0
                                              minValue:0.0
                                              maxValue:100.0
                                                 target:self
                                                 action:@selector(circuitSliderChanged:)];
        _circuitSliders[i].frame =
            NSMakeRect(140.0, circuitY[i] - 4.0, 470.0, 28.0);
        _circuitSliders[i].continuous = YES;
        _circuitSliders[i].tag = i;
        [content addSubview:_circuitSliders[i]];

        _circuitValues[i] = makeLabel(@"50%",
                                      NSMakeRect(626.0, circuitY[i], 70.0, 22.0));
        _circuitValues[i].alignment = NSTextAlignmentRight;
        [content addSubview:_circuitValues[i]];
    }

    _bypassButton = [NSButton checkboxWithTitle:@"Bypass"
                                         target:self
                                         action:@selector(bypassChanged:)];
    _bypassButton.frame = NSMakeRect(140.0, 374.0, 150.0, 28.0);
    [content addSubview:_bypassButton];

    [content addSubview:makeLabel(@"Input Level", NSMakeRect(24.0, 330.0, 100.0, 22.0))];
    _inputMeter = [[NSProgressIndicator alloc]
        initWithFrame:NSMakeRect(140.0, 332.0, 556.0, 16.0)];
    _inputMeter.indeterminate = NO;
    _inputMeter.style = NSProgressIndicatorStyleBar;
    _inputMeter.minValue = 0.0;
    _inputMeter.maxValue = 1.0;
    [content addSubview:_inputMeter];

    [content addSubview:makeLabel(@"Output Level", NSMakeRect(24.0, 296.0, 100.0, 22.0))];
    _outputMeter = [[NSProgressIndicator alloc]
        initWithFrame:NSMakeRect(140.0, 298.0, 556.0, 16.0)];
    _outputMeter.indeterminate = NO;
    _outputMeter.style = NSProgressIndicatorStyleBar;
    _outputMeter.minValue = 0.0;
    _outputMeter.maxValue = 1.0;
    [content addSubview:_outputMeter];

    _statusLabel = makeLabel(@"Audio Stopped",
                             NSMakeRect(24.0, 112.0, 672.0, 166.0));
    _statusLabel.font =
        [NSFont monospacedSystemFontOfSize:12.0 weight:NSFontWeightRegular];
    _statusLabel.usesSingleLineMode = NO;
    [content addSubview:_statusLabel];

    _errorLabel = makeLabel(@"", NSMakeRect(24.0, 28.0, 672.0, 76.0));
    _errorLabel.textColor = [NSColor systemRedColor];
    _errorLabel.usesSingleLineMode = NO;
    [content addSubview:_errorLabel];

    [self populateDevices];
    [self refreshModelControls];
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
                           nsString(device.name),
                           device.inputChannels,
                           device.outputChannels];
        [_devicePopup addItemWithTitle:title];
    }

    if (!error.empty())
    {
        _errorLabel.stringValue = nsString(error);
    }
    else if (_devices.empty())
    {
        _errorLabel.stringValue =
            @"No duplex audio device was found. Connect an interface with both input "
             "and output, then relaunch CircuitPedal.";
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
    _modelValue.stringValue = nsString(_engine->activeModelName());

    _diodeLabel.hidden = generic;
    _diodePopup.hidden = generic;
    _distortionLabel.hidden = generic;
    _distortionSlider.hidden = generic;
    _distortionValue.hidden = generic;
    _outputControlLabel.hidden = generic;
    _outputSlider.hidden = generic;
    _outputValue.hidden = generic;

    _circuitControls = _engine->circuitControls();
    for (std::size_t i = 0; i < 4; ++i)
    {
        const BOOL visible = generic && i < _circuitControls.size();
        _circuitLabels[i].hidden = !visible;
        _circuitSliders[i].hidden = !visible;
        _circuitValues[i].hidden = !visible;
        if (visible)
        {
            _circuitLabels[i].stringValue = nsString(_circuitControls[i].name);
            const double value = 100.0 * static_cast<double>(_engine->circuitControl(i));
            _circuitSliders[i].doubleValue = value;
            _circuitValues[i].stringValue =
                [NSString stringWithFormat:@"%.0f%%", value];
        }
    }

    if (generic && _circuitControls.size() > 4)
    {
        _errorLabel.stringValue =
            @"This V0.8 GUI displays the first four circuit controls. "
             "The circuit engine supports up to sixteen.";
    }
}

- (void)setRunningControls:(BOOL)running
{
    _devicePopup.enabled = !running && !_devices.empty();
    _channelPopup.enabled = !running && _channelPopup.numberOfItems > 0;
    _bufferPopup.enabled = !running;
    _loadCircuitButton.enabled = !running;
    _builtinButton.enabled = !running && _engine->usingCircuitFile();

    _diodePopup.enabled = !running && !_engine->usingCircuitFile();
    _distortionSlider.enabled = !_engine->usingCircuitFile();
    _outputSlider.enabled = !_engine->usingCircuitFile();

    for (NSInteger i = 0; i < 4; ++i)
        _circuitSliders[i].enabled = _engine->usingCircuitFile();

    _startButton.enabled =
        !running && !_devices.empty() && _channelPopup.numberOfItems > 0;
    _stopButton.enabled = running;
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
        _errorLabel.stringValue = @"Could not read the selected file path.";
        return;
    }

    std::string error;
    if (!_engine->loadCircuitFile(utf8Path, error))
    {
        _errorLabel.stringValue = nsString(error);
        NSBeep();
        return;
    }

    _bypassButton.state = NSControlStateValueOff;
    _errorLabel.stringValue = @"";
    _statusLabel.stringValue =
        @"Circuit loaded. Start Audio to compile its DC operating point and "
         "begin real-time processing.";
    [self refreshModelControls];
    [self setRunningControls:NO];
}

- (void)useBuiltin:(id)sender
{
    (void)sender;
    if (!_engine->useBuiltInDistortionPlus())
        return;

    _bypassButton.state =
        _engine->bypassed() ? NSControlStateValueOn : NSControlStateValueOff;
    _errorLabel.stringValue = @"";
    _statusLabel.stringValue = @"Built-in Distortion+ selected.";
    [self refreshModelControls];
    [self setRunningControls:NO];
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
        _errorLabel.stringValue =
            @"Select a duplex device and an input channel before starting.";
        return;
    }

    circuitpedal::AudioStartConfiguration configuration;
    configuration.deviceId =
        _devices[static_cast<std::size_t>(deviceRow)].id;
    configuration.inputChannel = static_cast<std::uint32_t>(channelRow);
    configuration.requestedBufferFrames =
        static_cast<std::uint32_t>(_bufferPopup.titleOfSelectedItem.intValue);

    if (!_engine->usingCircuitFile())
    {
        _engine->setDistortion(
            static_cast<float>(_distortionSlider.doubleValue / 100.0));
        _engine->setOutput(
            static_cast<float>(_outputSlider.doubleValue / 100.0));
        const NSInteger diodeRow = _diodePopup.indexOfSelectedItem;
        const auto diodePreset =
            static_cast<circuitpedal::ClippingDiodePreset>(
                diodeRow >= 0 ? static_cast<std::uint32_t>(diodeRow) : 0U);
        _engine->setClippingDiodePreset(diodePreset);
    }
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
    [self refreshModelControls];
    [self setRunningControls:NO];
}

- (void)distortionSliderChanged:(id)sender
{
    (void)sender;
    const double value = _distortionSlider.doubleValue;
    _distortionValue.stringValue =
        [NSString stringWithFormat:@"%.0f%%", value];
    _engine->setDistortion(static_cast<float>(value / 100.0));
}

- (void)outputSliderChanged:(id)sender
{
    (void)sender;
    const double value = _outputSlider.doubleValue;
    _outputValue.stringValue =
        [NSString stringWithFormat:@"%.0f%%", value];
    _engine->setOutput(static_cast<float>(value / 100.0));
}

- (void)circuitSliderChanged:(id)sender
{
    NSSlider* slider = (NSSlider*)sender;
    const NSInteger index = slider.tag;
    if (index < 0 || index >= 4)
        return;

    const double value = slider.doubleValue;
    _circuitValues[index].stringValue =
        [NSString stringWithFormat:@"%.0f%%", value];
    (void)_engine->setCircuitControl(
        static_cast<std::size_t>(index),
        static_cast<float>(value / 100.0));
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

    _inputMeter.doubleValue =
        std::clamp(static_cast<double>(_engine->inputPeak()), 0.0, 1.0);
    _outputMeter.doubleValue =
        std::clamp(static_cast<double>(_engine->outputPeak()), 0.0, 1.0);
}

- (void)updateRunningStatus
{
    const circuitpedal::AudioRuntimeInfo info = _engine->runtimeInfo();
    if (_engine->usingCircuitFile())
    {
        _statusLabel.stringValue = [NSString stringWithFormat:
            @"Audio Running @ %.1f kHz\n"
             "Model: %@ (.cpedal generic MNA)\n"
             "Requested buffer: %u | Actual: %u (%.2f ms)\n"
             "Input latency: %u + %u safety frames\n"
             "Output latency: %u + %u safety frames\n"
             "DSP FIR delay: %u frames | Reported component sum: %.2f ms\n"
             "Generic circuit oversampling: 4x nonlinear solve + FIR resampling",
            info.sampleRate / 1000.0,
            nsString(_engine->activeModelName()),
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
    else
    {
        _statusLabel.stringValue = [NSString stringWithFormat:
            @"Audio Running @ %.1f kHz\n"
             "Model: Built-in Distortion+\n"
             "Diodes: %s\n"
             "Requested buffer: %u | Actual: %u (%.2f ms)\n"
             "Input latency: %u + %u safety frames | Output: %u + %u safety frames\n"
             "DSP FIR delay: %u frames | Reported component sum: %.2f ms",
            info.sampleRate / 1000.0,
            circuitpedal::clippingDiodePresetName(_engine->clippingDiodePreset()),
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
