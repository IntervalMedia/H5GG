# H5GG Plugin Development Guide

This directory contains example plugins and serves as a starting point for developing your own H5GG plugins.

## Table of Contents
- [Existing Plugins](#existing-plugins)
- [Plugin Architecture](#plugin-architecture)
- [Quick Start](#quick-start)
- [Plugin Development Workflow](#plugin-development-workflow)
- [Best Practices](#best-practices)
- [Debugging Plugins](#debugging-plugins)
- [Plugin Ideas](#plugin-ideas)

---

## Existing Plugins

### 1. customAlert
A simple plugin demonstrating how to create custom UI alerts from JavaScript.

**Features**:
- Shows how to expose Objective-C methods to JavaScript
- Demonstrates parameter passing (0, 1, and 2+ parameters)
- Shows callback functions from native to JavaScript
- Handles main thread UI operations

**Files**:
- `Tweak.mm` - Objective-C implementation
- `customAlert.js` - JavaScript usage example
- `Makefile` - Build configuration

### 2. h5frida15.1.24
Advanced hooking and function calling plugin based on Frida.

**Features**:
- Call any C/C++/Objective-C function
- Hook methods and functions
- Code patching capabilities
- Works on both jailbroken and non-jailbroken devices

### 3. hookme-test
Simple test plugin for experimenting with hooks.

---

## Plugin Architecture

H5GG plugins are dynamic libraries (dylib) that:
1. Export an Objective-C class with `JSExport` protocol
2. Are loaded at runtime using `h5gg.loadPlugin()`
3. Can call back into JavaScript using JSValue
4. Run in the same process as the target app

### Plugin Flow

```
JavaScript (H5GG Script)
         ↓ [h5gg.loadPlugin()]
    Dylib Plugin Loading
         ↓
    Objective-C Class Instance
         ↓ [JSExport protocol]
    JavaScript API Available
         ↓
    Method Calls Between JS ↔ ObjC
```

---

## Quick Start

### Step 1: Set Up Build Environment

Install Theos if you haven't already:
```bash
# Set up Theos environment
export THEOS=/opt/theos
git clone --recursive https://github.com/theos/theos.git $THEOS
```

### Step 2: Copy Template

Start with the customAlert plugin as a template:
```bash
cp -r customAlert myNewPlugin
cd myNewPlugin
```

### Step 3: Modify Plugin Name

Edit `Makefile`:
```makefile
TWEAK_NAME = MyNewPlugin  # Change this
MyNewPlugin_FILES = Tweak.mm
```

Edit `control`:
```
Package: com.yourname.mynewplugin
Name: MyNewPlugin
```

### Step 4: Implement Your Plugin

Edit `Tweak.mm`:

```objective-c
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>

// Define JavaScript interface
@protocol MyPluginJSExport <JSExport>
-(void)myMethod;
-(NSString*)calculateSomething:(int)value;
@end

// Implement plugin class
@interface MyPlugin : NSObject <MyPluginJSExport>
@end

@implementation MyPlugin

-(void)myMethod {
    NSLog(@"MyPlugin: myMethod called");
}

-(NSString*)calculateSomething:(int)value {
    int result = value * 2;
    return [NSString stringWithFormat:@"Result: %d", result];
}

@end
```

### Step 5: Build Plugin

```bash
make clean
make
```

This produces `MyNewPlugin.dylib` in `obj/` directory.

### Step 6: Use Plugin in JavaScript

Create `test.js`:
```javascript
h5gg.require(7.8);

// Load your plugin
var myPlugin = h5gg.loadPlugin("MyPlugin", "MyNewPlugin.dylib");

if (!myPlugin) {
    throw "Plugin failed to load!";
}

// Use it
myPlugin.myMethod();
var result = myPlugin.calculateSomething(42);
alert(result); // Shows "Result: 84"
```

---

## Plugin Development Workflow

### 1. Design Phase
- Define what functionality you need
- Plan the JavaScript API interface
- Consider thread safety and performance

### 2. Implementation Phase
- Create Objective-C class with JSExport protocol
- Implement methods with proper error handling
- Test basic functionality

### 3. Testing Phase
- Test with various inputs
- Check memory leaks with Instruments
- Test on different iOS versions
- Verify thread safety

### 4. Documentation Phase
- Write usage examples
- Document parameters and return values
- List requirements and dependencies

### 5. Distribution Phase
- Package as .dylib with .js example
- Share on H5GG Discord or iosgods forum
- Consider open-sourcing on GitHub

---

## Best Practices

### Thread Safety

**Rule**: Always use main queue for UI operations

```objective-c
-(void)showAlert:(NSString*)message {
    dispatch_async(dispatch_get_main_queue(), ^{
        // UI code here
        UIAlertController *alert = [UIAlertController 
            alertControllerWithTitle:@"Alert" 
            message:message 
            preferredStyle:UIAlertControllerStyleAlert];
        // ...
    });
}
```

### Parameter Validation

```objective-c
-(NSString*)processData:(NSString*)input {
    if (!input || input.length == 0) {
        NSLog(@"MyPlugin: Invalid input");
        return @"Error: Invalid input";
    }
    // Process...
}
```

### Memory Management

Use `__weak` to avoid retain cycles with callbacks:

```objective-c
@interface MyPlugin : NSObject
@property (weak) JSValue* callback;
@end

-(void)setCallback:(JSValue*)cb {
    self.callback = cb;
}
```

### Error Handling

Return meaningful error messages:

```objective-c
-(NSString*)readFile:(NSString*)path {
    NSError *error;
    NSString *content = [NSString stringWithContentsOfFile:path 
        encoding:NSUTF8StringEncoding 
        error:&error];
    
    if (error) {
        return [NSString stringWithFormat:@"Error: %@", 
            error.localizedDescription];
    }
    return content;
}
```

### Exporting Complex Methods

For methods with 2+ parameters, use `JSExportAs`:

```objective-c
@protocol MyPluginJSExport <JSExport>

// One parameter - direct export
-(void)simpleMethod:(NSString*)param;

// Two+ parameters - use JSExportAs
JSExportAs(complexMethod, 
    -(void)methodWithParam1:(NSString*)p1 
                     param2:(int)p2 
                     param3:(BOOL)p3
);

@end
```

JavaScript usage:
```javascript
plugin.simpleMethod("test");
plugin.complexMethod("test", 42, true);
```

### Callbacks from Native to JavaScript

Store JavaScript callback and call from native code:

```objective-c
@interface MyPlugin : NSObject
@property JSValue* jsCallback;
@property NSThread* jsThread;
@end

-(void)setCallback:(JSValue*)callback {
    self.jsCallback = callback;
    self.jsThread = [NSThread currentThread]; // Remember JS thread
}

-(void)triggerCallback {
    // Call back on original JavaScript thread
    [self performSelector:@selector(doCallback) 
        onThread:self.jsThread 
        withObject:nil 
        waitUntilDone:NO];
}

-(void)doCallback {
    [self.jsCallback callWithArguments:@[@"result"]];
}
```

---

## Debugging Plugins

### Using NSLog

Add logging to your plugin:

```objective-c
NSLog(@"MyPlugin: Method called with param=%@", param);
```

View logs:
```bash
# On device with SSH
tail -f /var/log/syslog | grep MyPlugin

# Using Xcode
# Window → Devices → View Device Logs
```

### Using Safari Web Inspector

1. Enable Web Inspector in Safari (macOS)
2. Connect iOS device
3. In Safari: Develop → [Your Device] → [App]
4. Use console to test plugin calls

### Common Issues

**Plugin fails to load:**
- Check dylib is in correct location (.app directory)
- Verify class name matches first parameter of loadPlugin()
- Check build architecture is arm64
- Review console logs for loading errors

**Methods not callable from JS:**
- Ensure method is in `@protocol` with `<JSExport>`
- Check method signature matches JSExportAs declaration
- Verify plugin instance is not nil

**Crashes on method call:**
- Check for null pointer dereferences
- Verify thread safety (UI on main thread)
- Check memory management (weak references)
- Validate input parameters

---

## Plugin Ideas

For a comprehensive list of useful plugin ideas, see [PLUGIN_IDEAS.md](../PLUGIN_IDEAS.md) in the root directory.

**Quick Ideas to Get Started:**

1. **Simple Logger** - Log messages to a file with timestamps
2. **Toast Messages** - Show Android-style toast notifications
3. **Haptic Feedback** - Trigger haptic feedback from JS
4. **Clipboard Manager** - Read/write system clipboard
5. **Device Info** - Get device model, iOS version, etc.
6. **Math Helper** - Complex calculations in native code
7. **Color Picker** - Show native color picker UI
8. **Date/Time Formatter** - Advanced date formatting
9. **QR Code Generator** - Generate QR codes
10. **Sound Player** - Play system sounds or custom audio

---

## Advanced Topics

### Integrating Third-Party Libraries

Add frameworks in Makefile:
```makefile
MyPlugin_FRAMEWORKS = UIKit Foundation JavaScriptCore CoreLocation
MyPlugin_PRIVATE_FRAMEWORKS = AppSupport
```

Link against libraries:
```makefile
MyPlugin_LDFLAGS = -lsqlite3
```

### Using C++ in Plugins

Rename `Tweak.mm` to enable C++:
```makefile
MyPlugin_FILES = Tweak.mm  # .mm enables Objective-C++
```

Mix C++ and Objective-C:
```objective-c++
#import <vector>

@implementation MyPlugin

-(int)sumArray:(NSArray*)numbers {
    std::vector<int> vec;
    for (NSNumber *num in numbers) {
        vec.push_back([num intValue]);
    }
    
    int sum = 0;
    for (int n : vec) sum += n;
    return sum;
}

@end
```

### Hooking with Plugins

Use Frida or Dobby for hooking:

```objective-c
#import <substrate.h>

// Original function pointer
static int (*original_someFunction)(int param);

// Hook implementation
static int hooked_someFunction(int param) {
    NSLog(@"someFunction called with param=%d", param);
    return original_someFunction(param); // Call original
}

@implementation MyPlugin

-(instancetype)init {
    if (self = [super init]) {
        // Install hook
        MSHookFunction((void *)someFunction, 
                      (void *)hooked_someFunction,
                      (void **)&original_someFunction);
    }
    return self;
}

@end
```

---

## Resources

- **H5GG Discord**: https://discord.gg/FAs4MH7HMc
- **iosgods Forum**: https://iosgods.com/forum/595-h5gg-igamegod/
- **Theos Documentation**: https://theos.dev
- **iOS Runtime Headers**: https://developer.limneos.net
- **JavaScriptCore Guide**: https://developer.apple.com/documentation/javascriptcore

---

## Contributing

Share your plugins with the community:

1. Test thoroughly
2. Document clearly
3. Add usage examples
4. Post in Discord or forums
5. Consider open-sourcing

---

## License

Plugin examples in this directory are provided as educational resources for the H5GG community.

**Last Updated**: 2025-10-20
