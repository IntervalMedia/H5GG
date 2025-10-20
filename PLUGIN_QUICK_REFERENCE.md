# Quick Reference: H5GG Plugin Ideas by Use Case

## Finding Information

**Need to explore the app?**
- [Class Browser Plugin](PLUGIN_IDEAS.md#9-class-browser-plugin) - Browse all Objective-C classes and methods
- [Symbol Resolver Plugin](PLUGIN_IDEAS.md#25-symbol-resolver-plugin) - Resolve symbols and addresses
- [UI Inspector Plugin](PLUGIN_IDEAS.md#19-ui-inspector-plugin) - Inspect view hierarchy
- [String Dumper Plugin](PLUGIN_IDEAS.md#29-string-dumper-plugin) - Extract strings from memory

## Debugging Issues

**App crashes?**
- [Exception Handler Plugin](PLUGIN_IDEAS.md#3-exception-handler-plugin) - Catch and analyze crashes
- [Breakpoint Manager Plugin](PLUGIN_IDEAS.md#1-breakpoint-manager-plugin) - Debug without Xcode
- [Console Logger Plugin](PLUGIN_IDEAS.md#2-console-logger-plugin) - Advanced logging system

**Need to trace execution?**
- [Method Call Tracer Plugin](PLUGIN_IDEAS.md#11-method-call-tracer-plugin) - Trace method calls
- [Method Swizzler Plugin](PLUGIN_IDEAS.md#4-method-swizzler-plugin) - Hook and modify methods
- [ARM64 Disassembler Plugin](PLUGIN_IDEAS.md#27-arm64-disassembler-plugin) - Disassemble native code

## Memory Analysis

**Working with memory?**
- [Memory Hex Viewer Plugin](PLUGIN_IDEAS.md#5-memory-hex-viewer-plugin) - View and edit memory
- [Memory Diff Plugin](PLUGIN_IDEAS.md#6-memory-diff-plugin) - Compare memory snapshots
- [Pointer Chain Validator Plugin](PLUGIN_IDEAS.md#7-pointer-chain-validator-plugin) - Validate pointer chains
- [Pattern Scanner Plugin](PLUGIN_IDEAS.md#28-pattern-scanner-plugin) - Search for byte patterns
- [Memory Allocator Tracker Plugin](PLUGIN_IDEAS.md#8-memory-allocator-tracker-plugin) - Track allocations

## Network & Data

**Analyzing network traffic?**
- [Network Monitor Plugin](PLUGIN_IDEAS.md#13-network-monitor-plugin) - Intercept HTTP/HTTPS
- [WebSocket Monitor Plugin](PLUGIN_IDEAS.md#14-websocket-monitor-plugin) - Monitor WebSocket messages

**Need to access saved data?**
- [File System Browser Plugin](PLUGIN_IDEAS.md#15-file-system-browser-plugin) - Browse app files
- [SQLite Database Inspector Plugin](PLUGIN_IDEAS.md#16-sqlite-database-inspector-plugin) - View databases
- [UserDefaults Editor Plugin](PLUGIN_IDEAS.md#17-userdefaults-editor-plugin) - Edit preferences
- [Keychain Viewer Plugin](PLUGIN_IDEAS.md#18-keychain-viewer-plugin) - View keychain items

## Performance

**Performance problems?**
- [FPS Monitor Plugin](PLUGIN_IDEAS.md#22-fps-monitor-plugin) - Monitor frame rate
- [CPU Profiler Plugin](PLUGIN_IDEAS.md#23-cpu-profiler-plugin) - Profile CPU usage
- [Energy Usage Monitor Plugin](PLUGIN_IDEAS.md#24-energy-usage-monitor-plugin) - Monitor battery usage

## Reverse Engineering

**Bypassing protections?**
- [Anti-Debug Bypass Plugin](PLUGIN_IDEAS.md#26-anti-debug-bypass-plugin) - Bypass anti-debugging
- [Code Patcher Plugin](PLUGIN_IDEAS.md#30-code-patcher-plugin) - Apply binary patches

**Need runtime control?**
- [Instance Tracker Plugin](PLUGIN_IDEAS.md#10-instance-tracker-plugin) - Track object instances
- [Block Inspector Plugin](PLUGIN_IDEAS.md#12-block-inspector-plugin) - Inspect closures
- [Dynamic Library Injector Plugin](PLUGIN_IDEAS.md#31-dynamic-library-injector-plugin) - Inject libraries

## UI & Automation

**Working with UI?**
- [Gesture Recorder Plugin](PLUGIN_IDEAS.md#20-gesture-recorder-plugin) - Record and replay gestures
- [Screenshot & Video Recorder Plugin](PLUGIN_IDEAS.md#21-screenshot--video-recorder-plugin) - Capture screen

## Workflow Management

**Managing scripts?**
- [Script Manager Plugin](PLUGIN_IDEAS.md#32-script-manager-plugin) - Organize H5GG scripts

---

## Getting Started

1. Read the [Plugin Development Guide](pluginDemo/README.md)
2. Check out [existing plugin examples](pluginDemo/)
3. Browse [all 32 plugin ideas](PLUGIN_IDEAS.md)
4. Join the [H5GG Discord](https://discord.gg/FAs4MH7HMc) or [iosgods forum](https://iosgods.com/forum/595-h5gg-igamegod/)

## Plugin Development Steps

1. **Design**: Choose your plugin from the ideas or create your own
2. **Implement**: Follow the [Quick Start guide](pluginDemo/README.md#quick-start)
3. **Test**: Debug using Safari Web Inspector
4. **Share**: Contribute back to the community

---

**See also:**
- [Complete Plugin Ideas List](PLUGIN_IDEAS.md) - All 32 plugins with full details
- [Plugin Development Guide](pluginDemo/README.md) - Complete development reference
- [H5GG JavaScript API](h5gg-js-doc-en.js) - JavaScript engine documentation
