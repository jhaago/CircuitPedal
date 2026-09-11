from pathlib import Path
import re


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"missing anchor: {label}")
    return text.replace(old, new, 1)

# ---- Real master I/O gain support -----------------------------------------
h = Path('src/MacAudioEngine.h')
s = h.read_text()
s = replace_once(s,
'''    void setOutput(float normalized) noexcept;\n    void setBypass(bool bypassed) noexcept;''',
'''    void setOutput(float normalized) noexcept;\n    void setInputTrimDb(float decibels) noexcept;\n    void setMasterOutputDb(float decibels) noexcept;\n    void setBypass(bool bypassed) noexcept;''',
'engine gain setters')
s = replace_once(s,
'''    float output() const noexcept;\n    bool bypassed() const noexcept;''',
'''    float output() const noexcept;\n    float inputTrimDb() const noexcept;\n    float masterOutputDb() const noexcept;\n    bool bypassed() const noexcept;''',
'engine gain getters')
h.write_text(s)

cpp = Path('src/MacAudioEngine.cpp')
s = cpp.read_text()
s = replace_once(s,
'''    std::atomic<float> inputPeak { 0.0f };\n    std::atomic<float> outputPeak { 0.0f };\n    std::atomic<bool> running { false };''',
'''    std::atomic<float> inputPeak { 0.0f };\n    std::atomic<float> outputPeak { 0.0f };\n    std::atomic<float> inputTrimLinear { 1.0f };\n    std::atomic<float> masterOutputLinear { 1.0f };\n    std::atomic<float> inputTrimDecibels { 0.0f };\n    std::atomic<float> masterOutputDecibels { 0.0f };\n    std::atomic<bool> running { false };''',
'engine gain atomics')
s = replace_once(s,
'''            const float input = state->inputScratch[frame];\n            float output = 0.0f;''',
'''            const float rawInput = state->inputScratch[frame];\n            const float input = rawInput * state->inputTrimLinear.load(std::memory_order_relaxed);\n            float output = 0.0f;''',
'input trim in callback')
s = replace_once(s,
'''            if (std::isfinite(input))\n                blockInputPeak = std::max(blockInputPeak, std::abs(input));\n            blockOutputPeak = std::max(blockOutputPeak, std::abs(output));''',
'''            output = std::clamp(output * state->masterOutputLinear.load(std::memory_order_relaxed), -1.0f, 1.0f);\n            if (std::isfinite(input))\n                blockInputPeak = std::max(blockInputPeak, std::abs(input));\n            blockOutputPeak = std::max(blockOutputPeak, std::abs(output));''',
'output level in callback')
# Insert methods before setBypass implementation.
anchor = 'void MacAudioEngine::setBypass(bool bypassed) noexcept\n'
idx = s.find(anchor)
if idx < 0:
    raise SystemExit('missing anchor: setBypass implementation')
methods = '''void MacAudioEngine::setInputTrimDb(float decibels) noexcept\n{\n    const float db = std::clamp(decibels, -18.0f, 18.0f);\n    impl_->inputTrimDecibels.store(db, std::memory_order_relaxed);\n    impl_->inputTrimLinear.store(std::pow(10.0f, db / 20.0f), std::memory_order_relaxed);\n}\n\nvoid MacAudioEngine::setMasterOutputDb(float decibels) noexcept\n{\n    const float db = std::clamp(decibels, -18.0f, 6.0f);\n    impl_->masterOutputDecibels.store(db, std::memory_order_relaxed);\n    impl_->masterOutputLinear.store(std::pow(10.0f, db / 20.0f), std::memory_order_relaxed);\n}\n\n'''
s = s[:idx] + methods + s[idx:]
# Insert getters before bypassed getter.
anchor = 'bool MacAudioEngine::bypassed() const noexcept\n'
idx = s.find(anchor)
if idx < 0:
    raise SystemExit('missing anchor: bypassed getter')
methods = '''float MacAudioEngine::inputTrimDb() const noexcept\n{\n    return impl_->inputTrimDecibels.load(std::memory_order_relaxed);\n}\n\nfloat MacAudioEngine::masterOutputDb() const noexcept\n{\n    return impl_->masterOutputDecibels.load(std::memory_order_relaxed);\n}\n\n'''
s = s[:idx] + methods + s[idx:]
cpp.write_text(s)

# ---- Native UI -------------------------------------------------------------
p = Path('src/main_mac_gui.mm')
s = p.read_text()

# Richer hero treatment.
s = replace_once(s,
'''    [super drawRect:dirtyRect];\n\n    const CGFloat enclosureWidth = std::min<CGFloat>(300.0, NSWidth(self.bounds) * 0.46);''',
'''    [super drawRect:dirtyRect];\n\n    // Atmospheric pedal-specific stage behind the enclosure, matching the approved UI direction.\n    NSGradient* stageGradient = [[NSGradient alloc] initWithStartingColor:cpColor(0.12, 0.045, 0.038)\n                                                             endingColor:cpColor(0.025, 0.030, 0.036)];\n    [stageGradient drawInRect:NSInsetRect(self.bounds, 1.0, 1.0) angle:18.0];\n    [[warmAccentColor() colorWithAlphaComponent:0.10] setFill];\n    for (NSInteger i = 0; i < 18; ++i)\n    {\n        const CGFloat x = NSWidth(self.bounds) * (0.46 + 0.03 * static_cast<CGFloat>(i % 7));\n        const CGFloat y = NSHeight(self.bounds) * (0.18 + 0.08 * static_cast<CGFloat>((i * 3) % 8));\n        const CGFloat r = 7.0 + static_cast<CGFloat>((i * 5) % 13);\n        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, y, r, r)] fill];\n    }\n\n    const CGFloat enclosureWidth = std::min<CGFloat>(330.0, NSWidth(self.bounds) * 0.48);''',
'hero atmosphere')
s = s.replace('const CGFloat enclosureHeight = std::min<CGFloat>(380.0, NSHeight(self.bounds) - 80.0);',
              'const CGFloat enclosureHeight = std::min<CGFloat>(410.0, NSHeight(self.bounds) - 72.0);', 1)
s = s.replace('[cpColor(0.09, 0.10, 0.11) setFill];', '[cpColor(0.24, 0.075, 0.060) setFill];', 1)

# Add EQ and routing-editor custom views before flipped view.
insert_anchor = '@interface CircuitPedalFlippedView : NSView\n@end\n'
if insert_anchor not in s:
    raise SystemExit('missing anchor: flipped view')
custom_views = r'''
@interface CircuitPedalEQPreviewView : NSView
@end
@implementation CircuitPedalEQPreviewView
- (BOOL)isOpaque { return NO; }
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    [[NSColor colorWithWhite:0.02 alpha:0.65] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:8.0 yRadius:8.0] fill];
    [[NSColor colorWithWhite:1.0 alpha:0.055] setStroke];
    for (NSInteger i = 1; i < 7; ++i) {
        CGFloat x = NSWidth(self.bounds) * i / 7.0;
        NSBezierPath* g = [NSBezierPath bezierPath]; [g moveToPoint:NSMakePoint(x, 0)]; [g lineToPoint:NSMakePoint(x, NSHeight(self.bounds))]; [g stroke];
    }
    for (NSInteger i = 1; i < 5; ++i) {
        CGFloat y = NSHeight(self.bounds) * i / 5.0;
        NSBezierPath* g = [NSBezierPath bezierPath]; [g moveToPoint:NSMakePoint(0, y)]; [g lineToPoint:NSMakePoint(NSWidth(self.bounds), y)]; [g stroke];
    }
    NSBezierPath* response = [NSBezierPath bezierPath];
    for (NSInteger i = 0; i < 120; ++i) {
        CGFloat t = static_cast<CGFloat>(i) / 119.0;
        CGFloat x = t * NSWidth(self.bounds);
        CGFloat y = NSHeight(self.bounds) * (0.58 - 0.18 * std::sin(t * 2.7) + 0.04 * std::sin(t * 13.0));
        if (i == 0) [response moveToPoint:NSMakePoint(x, y)]; else [response lineToPoint:NSMakePoint(x, y)];
    }
    [accentColor() setStroke]; response.lineWidth = 2.0; [response stroke];
}
@end

@interface CircuitPedalRoutingEditorView : NSView {
@private
    NSPoint _nodes[7];
    NSInteger _dragNode;
    NSInteger _pendingSource;
    NSMutableArray<NSArray<NSNumber*>*>* _edges;
}
@end
@implementation CircuitPedalRoutingEditorView
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        _dragNode = -1; _pendingSource = -1;
        _nodes[0] = NSMakePoint(0.08, 0.50); _nodes[1] = NSMakePoint(0.23, 0.50);
        _nodes[2] = NSMakePoint(0.42, 0.68); _nodes[3] = NSMakePoint(0.42, 0.32);
        _nodes[4] = NSMakePoint(0.62, 0.50); _nodes[5] = NSMakePoint(0.78, 0.50); _nodes[6] = NSMakePoint(0.93, 0.50);
        _edges = [NSMutableArray arrayWithArray:@[ @[@0,@1], @[@1,@2], @[@1,@3], @[@2,@4], @[@3,@4], @[@4,@5], @[@5,@6] ]];
    }
    return self;
}
- (BOOL)isOpaque { return NO; }
- (NSPoint)pointForNode:(NSInteger)i { return NSMakePoint(_nodes[i].x * NSWidth(self.bounds), _nodes[i].y * NSHeight(self.bounds)); }
- (NSRect)rectForNode:(NSInteger)i
{
    NSPoint p = [self pointForNode:i];
    CGFloat w = (i == 0 || i == 6) ? 76.0 : 104.0;
    CGFloat h = (i == 0 || i == 6) ? 52.0 : 92.0;
    return NSMakeRect(p.x - w * 0.5, p.y - h * 0.5, w, h);
}
- (NSPoint)inputPort:(NSInteger)i { NSRect r=[self rectForNode:i]; return NSMakePoint(NSMinX(r), NSMidY(r)); }
- (NSPoint)outputPort:(NSInteger)i { NSRect r=[self rectForNode:i]; return NSMakePoint(NSMaxX(r), NSMidY(r)); }
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    [[NSColor colorWithSRGBRed:0.025 green:0.040 blue:0.055 alpha:1.0] setFill]; NSRectFill(self.bounds);
    [[NSColor colorWithWhite:1.0 alpha:0.035] setStroke];
    for (CGFloat x=0; x<NSWidth(self.bounds); x+=24.0) { NSBezierPath* p=[NSBezierPath bezierPath]; [p moveToPoint:NSMakePoint(x,0)]; [p lineToPoint:NSMakePoint(x,NSHeight(self.bounds))]; [p stroke]; }
    for (CGFloat y=0; y<NSHeight(self.bounds); y+=24.0) { NSBezierPath* p=[NSBezierPath bezierPath]; [p moveToPoint:NSMakePoint(0,y)]; [p lineToPoint:NSMakePoint(NSWidth(self.bounds),y)]; [p stroke]; }
    [cpColor(0.35,0.58,0.70) setStroke];
    for (NSArray<NSNumber*>* e in _edges) {
        NSInteger a=e[0].integerValue,b=e[1].integerValue; NSPoint p1=[self outputPort:a],p2=[self inputPort:b];
        CGFloat dx=std::max<CGFloat>(38.0,std::abs(p2.x-p1.x)*0.42);
        NSBezierPath* cable=[NSBezierPath bezierPath]; [cable moveToPoint:p1];
        [cable curveToPoint:p2 controlPoint1:NSMakePoint(p1.x+dx,p1.y) controlPoint2:NSMakePoint(p2.x-dx,p2.y)]; cable.lineWidth=3.0; [cable stroke];
    }
    NSArray<NSString*>* names=@[@"INPUT",@"COMP",@"FUZZ",@"MOD",@"DELAY",@"REVERB",@"OUTPUT"];
    for (NSInteger i=0;i<7;++i) {
        NSRect r=[self rectForNode:i]; NSBezierPath* body=[NSBezierPath bezierPathWithRoundedRect:r xRadius:(i==0||i==6?18.0:10.0) yRadius:(i==0||i==6?18.0:10.0)];
        [(i==2 ? cpColor(0.27,0.08,0.065) : cpColor(0.075,0.095,0.115)) setFill]; [body fill];
        [(i==2 ? warmAccentColor() : borderColor()) setStroke]; body.lineWidth=(i==2?1.8:1.0); [body stroke];
        NSDictionary* attrs=@{NSFontAttributeName:[NSFont systemFontOfSize:10.0 weight:NSFontWeightSemibold],NSForegroundColorAttributeName:textColor()}; NSSize z=[names[i] sizeWithAttributes:attrs];
        [names[i] drawAtPoint:NSMakePoint(NSMidX(r)-z.width*0.5,NSMidY(r)-5.0) withAttributes:attrs];
        if (i>0) { NSPoint p=[self inputPort:i]; NSBezierPath* port=[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(p.x-6,p.y-6,12,12)]; [cpColor(0.10,0.13,0.16) setFill]; [port fill]; [accentColor() setStroke]; [port stroke]; }
        if (i<6) { NSPoint p=[self outputPort:i]; NSBezierPath* port=[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(p.x-6,p.y-6,12,12)]; [(_pendingSource==i?warmAccentColor():cpColor(0.10,0.13,0.16)) setFill]; [port fill]; [accentColor() setStroke]; [port stroke]; }
    }
    NSDictionary* hint=@{NSFontAttributeName:[NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightRegular],NSForegroundColorAttributeName:mutedTextColor()};
    [@"ROUTING DESIGN — drag nodes; click OUT then IN to draw a cable. Current live DSP still processes the selected model only." drawAtPoint:NSMakePoint(14.0,12.0) withAttributes:hint];
}
- (void)mouseDown:(NSEvent*)event
{
    NSPoint p=[self convertPoint:event.locationInWindow fromView:nil]; _dragNode=-1;
    for (NSInteger i=0;i<7;++i) {
        if (i<6 && NSPointInRect(p,NSInsetRect(NSMakeRect([self outputPort:i].x-8,[self outputPort:i].y-8,16,16),-2,-2))) { _pendingSource=i; self.needsDisplay=YES; return; }
        if (i>0 && NSPointInRect(p,NSMakeRect([self inputPort:i].x-10,[self inputPort:i].y-10,20,20)) && _pendingSource>=0 && _pendingSource!=i) {
            BOOL exists=NO; for (NSArray<NSNumber*>* e in _edges) if (e[0].integerValue==_pendingSource && e[1].integerValue==i) exists=YES;
            if (!exists) [_edges addObject:@[@(_pendingSource),@(i)]]; _pendingSource=-1; self.needsDisplay=YES; return;
        }
        if (NSPointInRect(p,[self rectForNode:i])) { _dragNode=i; return; }
    }
}
- (void)mouseDragged:(NSEvent*)event
{
    if (_dragNode<0) return; NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
    _nodes[_dragNode].x=std::clamp(p.x/std::max<CGFloat>(1.0,NSWidth(self.bounds)),0.05,0.95);
    _nodes[_dragNode].y=std::clamp(p.y/std::max<CGFloat>(1.0,NSHeight(self.bounds)),0.10,0.90); self.needsDisplay=YES;
}
- (void)mouseUp:(NSEvent*)event { (void)event; _dragNode=-1; }
@end

'''
s = s.replace(insert_anchor, custom_views + insert_anchor, 1)

# New app delegate ivars.
s = replace_once(s,
'''    CircuitPedalHistoryView* _historyView;\n\n    NSTextField* _topModelLabel;''',
'''    CircuitPedalHistoryView* _historyView;\n    CircuitPedalRoutingEditorView* _routingEditorView;\n    CircuitPedalEQPreviewView* _eqPreviewView;\n    NSButton* _editChainButton;\n    NSButton* _inspectorTabButtons[4];\n    NSTextField* _inspectorInfoLabel;\n    NSInteger _inspectorTabIndex;\n    NSSlider* _inputTrimSlider;\n    NSTextField* _inputTrimValue;\n    NSSlider* _masterOutputSlider;\n    NSTextField* _masterOutputValue;\n\n    NSTextField* _topModelLabel;''',
'new ui ivars')

# Top branding phrase.
s = s.replace('@"TONE LIVES HERE"', '@"REAL CIRCUITS. REAL TONE."', 1)

# Signal chain header: replace phase shell label with real edit button.
old = '''    NSTextField* chainMode = makeLabel(@"CHAIN VIEW  •  Phase 1 shell",\n                                       NSMakeRect(450.0, 574.0, 270.0, 18.0),\n                                       9.0,\n                                       NSFontWeightMedium,\n                                       mutedTextColor());\n    chainMode.alignment = NSTextAlignmentRight;\n    chainMode.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;\n    [_workspacePanel addSubview:chainMode];\n'''
new = '''    _editChainButton = makeButton(@"EDIT SIGNAL CHAIN", NSMakeRect(532.0, 568.0, 192.0, 28.0), self, @selector(toggleSignalChainEditor:));\n    _editChainButton.contentTintColor = accentColor();\n    _editChainButton.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;\n    [_workspacePanel addSubview:_editChainButton];\n'''
s = replace_once(s, old, new, 'chain editor button')

# Routing editor layered over normal workspace, hidden initially.
anchor = '''    _heroView = [[CircuitPedalHeroView alloc] initWithFrame:NSMakeRect(16.0, 16.0, 710.0, 442.0)];\n    _heroView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;\n    [_workspacePanel addSubview:_heroView];\n'''
addition = anchor + '''\n    _routingEditorView = [[CircuitPedalRoutingEditorView alloc] initWithFrame:NSMakeRect(16.0, 16.0, 710.0, 542.0)];\n    _routingEditorView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;\n    _routingEditorView.hidden = YES;\n    [_workspacePanel addSubview:_routingEditorView];\n'''
s = replace_once(s, anchor, addition, 'routing editor insertion')

# Replace inspector header with real tabs.
old = '''    NSTextField* inspectorTitle = makeSectionLabel(@"INSPECTOR", NSMakeRect(16.0, 574.0, 120.0, 18.0));\n    inspectorTitle.autoresizingMask = NSViewMinYMargin;\n    [_rightPanel addSubview:inspectorTitle];\n    NSTextField* parameterTab = makeLabel(@"Parameters", NSMakeRect(16.0, 542.0, 86.0, 22.0), 12.0, NSFontWeightSemibold, accentColor());\n    parameterTab.autoresizingMask = NSViewMinYMargin;\n    [_rightPanel addSubview:parameterTab];\n    NSTextField* futureTabs = makeLabel(@"Circuit    EQ    Settings", NSMakeRect(112.0, 542.0, 180.0, 22.0), 10.0, NSFontWeightMedium, mutedTextColor());\n    futureTabs.alignment = NSTextAlignmentRight;\n    futureTabs.autoresizingMask = NSViewMinYMargin;\n    [_rightPanel addSubview:futureTabs];\n'''
new = '''    NSTextField* inspectorTitle = makeSectionLabel(@"INSPECTOR", NSMakeRect(16.0, 574.0, 120.0, 18.0));\n    inspectorTitle.autoresizingMask = NSViewMinYMargin;\n    [_rightPanel addSubview:inspectorTitle];\n    NSArray<NSString*>* inspectorTabs = @[ @"Parameters", @"Circuit", @"EQ", @"Settings" ];\n    for (NSInteger i = 0; i < 4; ++i)\n    {\n        _inspectorTabButtons[i] = makeButton(inspectorTabs[i], NSMakeRect(12.0 + i * 72.0, 540.0, 68.0, 28.0), self, @selector(inspectorTabChanged:));\n        _inspectorTabButtons[i].tag = i;\n        _inspectorTabButtons[i].contentTintColor = i == 0 ? accentColor() : mutedTextColor();\n        _inspectorTabButtons[i].autoresizingMask = NSViewMinYMargin;\n        [_rightPanel addSubview:_inspectorTabButtons[i]];\n    }\n    _inspectorTabIndex = 0;\n'''
s = replace_once(s, old, new, 'inspector tabs')

# Add inspector placeholder + EQ preview before hint.
anchor = '''    NSTextField* inspectorHint = makeLabel(@"Controls are generated from the loaded circuit definition.",'''
idx = s.find(anchor)
if idx < 0: raise SystemExit('missing anchor: inspector hint')
extra = '''    _inspectorInfoLabel = makeLabel(@"", NSMakeRect(20.0, 150.0, 270.0, 350.0), 11.0, NSFontWeightRegular, cpColor(0.72, 0.76, 0.80));\n    _inspectorInfoLabel.usesSingleLineMode = NO;\n    _inspectorInfoLabel.lineBreakMode = NSLineBreakByWordWrapping;\n    _inspectorInfoLabel.hidden = YES;\n    [_rightPanel addSubview:_inspectorInfoLabel];\n\n    _eqPreviewView = [[CircuitPedalEQPreviewView alloc] initWithFrame:NSMakeRect(18.0, 180.0, 274.0, 300.0)];\n    _eqPreviewView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;\n    _eqPreviewView.hidden = YES;\n    [_rightPanel addSubview:_eqPreviewView];\n\n'''
s = s[:idx] + extra + s[idx:]

# Replace bottom live monitor with real master I/O controls.
old = '''    [ _bottomPanel addSubview:makeSectionLabel(@"INPUT", NSMakeRect(14.0, 69.0, 60.0, 16.0)) ];\n    _inputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(166.0, 69.0, 74.0, 16.0), 9.5, NSFontWeightMedium, textColor());\n    _inputDbLabel.alignment = NSTextAlignmentRight;\n    [_bottomPanel addSubview:_inputDbLabel];\n    _inputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(14.0, 48.0, 226.0, 16.0)];\n    [_bottomPanel addSubview:_inputMeter];\n\n    _historyView = [[CircuitPedalHistoryView alloc] initWithFrame:NSMakeRect(260.0, 20.0, 330.0, 62.0)];\n    _historyView.autoresizingMask = NSViewWidthSizable;\n    [_bottomPanel addSubview:_historyView];\n    NSTextField* monitorLabel = makeSectionLabel(@"LIVE MONITOR", NSMakeRect(270.0, 69.0, 110.0, 16.0));\n    [_bottomPanel addSubview:monitorLabel];\n'''
new = '''    [ _bottomPanel addSubview:makeSectionLabel(@"INPUT", NSMakeRect(72.0, 69.0, 60.0, 16.0)) ];\n    _inputTrimSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(14.0, 18.0, 56.0, 56.0)];\n    styleRotarySlider(_inputTrimSlider); _inputTrimSlider.minValue = -18.0; _inputTrimSlider.maxValue = 18.0; _inputTrimSlider.doubleValue = 0.0;\n    _inputTrimSlider.target = self; _inputTrimSlider.action = @selector(inputTrimChanged:); [_bottomPanel addSubview:_inputTrimSlider];\n    _inputTrimValue = makeLabel(@"0.0 dB", NSMakeRect(6.0, 3.0, 72.0, 16.0), 8.5, NSFontWeightMedium, accentColor()); _inputTrimValue.alignment = NSTextAlignmentCenter; [_bottomPanel addSubview:_inputTrimValue];\n    _inputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(206.0, 69.0, 70.0, 16.0), 9.5, NSFontWeightMedium, textColor()); _inputDbLabel.alignment = NSTextAlignmentRight; [_bottomPanel addSubview:_inputDbLabel];\n    _inputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(78.0, 48.0, 198.0, 16.0)]; [_bottomPanel addSubview:_inputMeter];\n    NSTextField* trimLabel = makeSectionLabel(@"TRIM", NSMakeRect(16.0, 78.0, 54.0, 14.0)); trimLabel.alignment = NSTextAlignmentCenter; [_bottomPanel addSubview:trimLabel];\n'''
s = replace_once(s, old, new, 'bottom input/monitor')

# Replace output block with output master knob.
old = '''    [ _bottomPanel addSubview:makeSectionLabel(@"OUTPUT", NSMakeRect(1112.0, 69.0, 70.0, 16.0)) ];\n    _outputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(1260.0, 69.0, 62.0, 16.0), 9.5, NSFontWeightMedium, textColor());\n    _outputDbLabel.alignment = NSTextAlignmentRight;\n    _outputDbLabel.autoresizingMask = NSViewMinXMargin;\n    [_bottomPanel addSubview:_outputDbLabel];\n    _outputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(1112.0, 48.0, 210.0, 16.0)];\n    _outputMeter.autoresizingMask = NSViewMinXMargin;\n    [_bottomPanel addSubview:_outputMeter];\n'''
new = '''    [ _bottomPanel addSubview:makeSectionLabel(@"OUTPUT", NSMakeRect(1060.0, 69.0, 70.0, 16.0)) ];\n    _outputDbLabel = makeLabel(@"−∞ dB", NSMakeRect(1194.0, 69.0, 62.0, 16.0), 9.5, NSFontWeightMedium, textColor()); _outputDbLabel.alignment = NSTextAlignmentRight; _outputDbLabel.autoresizingMask = NSViewMinXMargin; [_bottomPanel addSubview:_outputDbLabel];\n    _outputMeter = [[CircuitPedalMeterView alloc] initWithFrame:NSMakeRect(1060.0, 48.0, 196.0, 16.0)]; _outputMeter.autoresizingMask = NSViewMinXMargin; [_bottomPanel addSubview:_outputMeter];\n    _masterOutputSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(1264.0, 18.0, 56.0, 56.0)]; styleRotarySlider(_masterOutputSlider); _masterOutputSlider.minValue = -18.0; _masterOutputSlider.maxValue = 6.0; _masterOutputSlider.doubleValue = 0.0; _masterOutputSlider.target = self; _masterOutputSlider.action = @selector(masterOutputChanged:); _masterOutputSlider.autoresizingMask = NSViewMinXMargin; [_bottomPanel addSubview:_masterOutputSlider];\n    _masterOutputValue = makeLabel(@"0.0 dB", NSMakeRect(1256.0, 3.0, 72.0, 16.0), 8.5, NSFontWeightMedium, accentColor()); _masterOutputValue.alignment = NSTextAlignmentCenter; _masterOutputValue.autoresizingMask = NSViewMinXMargin; [_bottomPanel addSubview:_masterOutputValue];\n    NSTextField* levelLabel = makeSectionLabel(@"LEVEL", NSMakeRect(1264.0, 78.0, 56.0, 14.0)); levelLabel.alignment = NSTextAlignmentCenter; levelLabel.autoresizingMask = NSViewMinXMargin; [_bottomPanel addSubview:levelLabel];\n'''
s = replace_once(s, old, new, 'bottom output')

# Compact status area in bottom middle.
s = s.replace('NSMakeRect(752.0, 48.0, 340.0, 34.0)', 'NSMakeRect(760.0, 48.0, 270.0, 34.0)', 1)
s = s.replace('NSMakeRect(752.0, 18.0, 340.0, 26.0)', 'NSMakeRect(760.0, 18.0, 270.0, 26.0)', 1)

# Do not push history now.
s = s.replace('    [_historyView pushLevel:output];\n', '', 1)

# Refresh model also refreshes inspector content.
old = '''    if (generic)\n        [_circuitDocumentView scrollPoint:NSMakePoint(0.0, 0.0)];\n}\n'''
new = '''    if (generic)\n        [_circuitDocumentView scrollPoint:NSMakePoint(0.0, 0.0)];\n    if (_inspectorTabIndex != 0)\n        [self inspectorTabChanged:_inspectorTabButtons[_inspectorTabIndex]];\n}\n'''
s = replace_once(s, old, new, 'refresh inspector hook')

# Insert new action methods before loadCircuit.
anchor = '- (void)loadCircuit:(id)sender\n'
idx = s.find(anchor)
if idx < 0: raise SystemExit('missing anchor: loadCircuit')
methods = r'''- (void)toggleSignalChainEditor:(id)sender
{
    (void)sender;
    const BOOL showing = !_routingEditorView.hidden;
    _routingEditorView.hidden = showing;
    _chainView.hidden = !showing;
    _heroView.hidden = !showing;
    _editChainButton.title = showing ? @"EDIT SIGNAL CHAIN" : @"DONE";
    _editChainButton.contentTintColor = showing ? accentColor() : warmAccentColor();
}

- (void)inspectorTabChanged:(id)sender
{
    NSButton* button = (NSButton*)sender;
    _inspectorTabIndex = std::clamp<NSInteger>(button.tag, 0, 3);
    for (NSInteger i=0;i<4;++i)
        _inspectorTabButtons[i].contentTintColor = i == _inspectorTabIndex ? accentColor() : mutedTextColor();
    const BOOL parameters = _inspectorTabIndex == 0;
    _diodeLabel.hidden = !parameters || _engine->usingCircuitFile();
    _diodePopup.hidden = !parameters || _engine->usingCircuitFile();
    _distortionLabel.hidden = !parameters || _engine->usingCircuitFile();
    _distortionSlider.hidden = !parameters || _engine->usingCircuitFile();
    _distortionValue.hidden = !parameters || _engine->usingCircuitFile();
    _outputControlLabel.hidden = !parameters || _engine->usingCircuitFile();
    _outputSlider.hidden = !parameters || _engine->usingCircuitFile();
    _outputValue.hidden = !parameters || _engine->usingCircuitFile();
    _circuitScrollView.hidden = !parameters || !_engine->usingCircuitFile();
    _eqPreviewView.hidden = _inspectorTabIndex != 2;
    _inspectorInfoLabel.hidden = parameters || _inspectorTabIndex == 2;
    if (_inspectorTabIndex == 1)
        _inspectorInfoLabel.stringValue = [NSString stringWithFormat:@"%@\n\nCircuit model\n%zu exposed controls\n48 kHz / 1× live path\n\nThe detailed schematic view will attach to package/model metadata in a later pass.", nsString(_engine->activeModelName()), _engine->circuitControls().size()];
    else if (_inspectorTabIndex == 3)
        _inspectorInfoLabel.stringValue = @"Audio settings remain live and functional in the left panel for this native pass.\n\nDevice, input channel and buffer selection still use the existing engine path.\n\nUI scale and package preferences will move here later.";
}

- (void)inputTrimChanged:(id)sender
{
    (void)sender;
    const float db = static_cast<float>(_inputTrimSlider.doubleValue);
    _engine->setInputTrimDb(db);
    _inputTrimValue.stringValue = [NSString stringWithFormat:@"%+.1f dB", db];
}

- (void)masterOutputChanged:(id)sender
{
    (void)sender;
    const float db = static_cast<float>(_masterOutputSlider.doubleValue);
    _engine->setMasterOutputDb(db);
    _masterOutputValue.stringValue = [NSString stringWithFormat:@"%+.1f dB", db];
}

'''
s = s[:idx] + methods + s[idx:]

# Initialise gain values on app launch before showing window.
anchor = '    [self refreshBypassAppearance];\n\n    _meterTimer ='
s = replace_once(s, anchor, '    [self refreshBypassAppearance];\n    _engine->setInputTrimDb(0.0f);\n    _engine->setMasterOutputDb(0.0f);\n\n    _meterTimer =', 'gain init')

p.write_text(s)

# Documentation for the native approved layout pass.
doc = Path('docs/ui_approved_layout_native.md')
doc.write_text('''# CircuitPedal native approved-layout pass\n\nThis branch ports the tablet-reviewed HTML design direction into the real macOS Circuit Lab application.\n\nImplemented in the native app:\n\n- approved top/left/centre/right/bottom visual hierarchy;\n- richer atmospheric selected-pedal hero treatment;\n- compact signal-chain overview with an **Edit Signal Chain** mode;\n- interactive native routing-design canvas supporting draggable nodes and split/recombine cable creation;\n- permanent inspector tabs for **Parameters / Circuit / EQ / Settings**;\n- real master **Input Trim** and **Output Level** gain stages in the macOS audio callback;\n- live input/output metering retained;\n- current device/channel/buffer, circuit loading, bypass and parameter-control paths preserved.\n\n## Important routing boundary\n\nThe routing editor is currently a native interaction/design layer only. The live engine still processes the single selected circuit/model. This is shown explicitly in the routing view so the UI does not imply that multi-effect split/merge DSP is already active.\n\nThe master input/output controls are real audio gain stages. Input Trim is limited to -18 dB to +18 dB and Output Level to -18 dB to +6 dB.\n\nNo oversampling changes were made; the stable live path remains 48 kHz / 1x.\n''')

print('native approved UI patch applied')
