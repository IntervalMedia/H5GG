//
//  FloatMenu.h
//  menutweak
//
//  Created by admin on 4/3/2022.
//

#undef WEBVIEW_HOOK

#ifndef FloatMenu_h
#define FloatMenu_h

#include "TopShow.h"
#import <JavaScriptCore/JavaScriptCore.h>
#include <objc/runtime.h>
#include "ModalShow.h"
#include "version.h"
#import <WebKit/WebKit.h>
#include "crossproc.h"
#include <libgen.h>
#include <sys/stat.h>

INCTXT(INITIAL_JS, "initial.js");

static NSHashTable* g_webViews = nil;

typedef id (*objc_method_pointer)(id,SEL,...);
objc_method_pointer g_orig_didCreateJavaScriptContext=NULL;

#ifdef __OBJC__
#include "h5gg.h"
#endif

//
// WKWebView bridge architecture:
//
// The original UIWebView version hooked NSObject's webView:didCreateJavaScriptContext:forFrame:
// to inject native objects (h5ggEngine via JSExport) directly into the JSContext. This gave
// synchronous access to all h5gg methods from JavaScript.
//
// WKWebView runs JS in a separate process, so direct JSContext injection is impossible.
// Instead we use window.prompt() as a synchronous bridge channel:
//   - JS calls: var result = window.prompt("__h5gg__:methodName", JSON.stringify(args))
//   - Native runJavaScriptTextInputPanelWithPrompt: intercepts the __h5gg__ prefix
//   - Native executes the method synchronously and returns JSON via completionHandler
//   - JS receives the return value synchronously
//
// This preserves the original synchronous API that the HTML files depend on.
// For async operations (pickScriptFile), we use postMessage + evaluateJavaScript callbacks.
// For callback registrations (setLayoutAction, setButtonAction), JS stores the callback
// in a global variable and native invokes it later via evaluateJavaScript.
//

@interface FloatMenu : UIView <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler>
@property (nonatomic, strong) WKWebView *webView;
@property (nonatomic, strong) h5ggEngine *h5ggInstance;
@property NSTimer* frontTimer;

@property BOOL touchableAll;
@property CGRect touchableRect;

@property CGRect dragableRect;
@property CGPoint startLocation;

@property NSMutableDictionary* actions;

@property BOOL usingCustomDialog;
@property void(^reloadAction)(void);

// Flags for JS callback registrations (setLayoutAction/setButtonAction)
@property BOOL layoutCallbackRegistered;
@property BOOL buttonCallbackRegistered;

-(void)setAction:(NSString*)name callback:(id)block;
-(void)setH5ggEngine:(h5ggEngine*)engine;
- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler;

// Alert/confirm/prompt for native-side use (called from h5gg.h via H5Alerting protocol)
-(void)alert:(NSString*)message;
-(BOOL)confirm:(NSString*)message;
-(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText;

// Invoke JS callbacks stored in the page (for setLayoutAction/setButtonAction)
-(void)invokeLayoutCallback:(CGFloat)width height:(CGFloat)height;
-(void)invokeButtonCallback;

@end
//@interface CALayer()
//@property BOOL allowsHitTesting;
//@end
@implementation FloatMenu
-(instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
        WKUserContentController *userContentController = [[WKUserContentController alloc] init];

        // 1) Inject initial.js error handler at document start (before any page JS runs)
        NSString *initialJS = [NSString stringWithUTF8String:gINITIAL_JSData];
        WKUserScript *errorScript = [[WKUserScript alloc] initWithSource:initialJS
                                                           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                        forMainFrameOnly:YES];
        [userContentController addUserScript:errorScript];

        // 2) Inject h5gg_internel_version at document start
        NSString *versionJS = [NSString stringWithFormat:@"var h5gg_internel_version = '%g';", H5GG_VERSION];
        WKUserScript *versionScript = [[WKUserScript alloc] initWithSource:versionJS
                                                             injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                          forMainFrameOnly:YES];
        [userContentController addUserScript:versionScript];

        // 3) Register async message handler for pickScriptFile callbacks
        [userContentController addScriptMessageHandler:self name:@"__h5gg_async__"];

        config.userContentController = userContentController;

        self.webView = [[WKWebView alloc] initWithFrame:self.bounds configuration:config];
        self.webView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.webView.navigationDelegate = self;
        self.webView.UIDelegate = self;
        self.webView.opaque = NO;
        self.webView.backgroundColor = [UIColor clearColor];
        [self addSubview:self.webView];

        float version = [UIDevice currentDevice].systemVersion.floatValue;
        self.usingCustomDialog = version > 13.0 && version < 13.4;
        self.touchableAll = YES;

        UIPanGestureRecognizer *drag = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragMe:)];
        [self addGestureRecognizer:drag];

        self.webView.scrollView.bounces = NO;
        self.webView.scrollView.scrollEnabled = NO;
        [self.webView.scrollView setShowsVerticalScrollIndicator:NO];
        [self.webView.scrollView setShowsHorizontalScrollIndicator:NO];
        if (@available(iOS 11.0, *)) {
            [self.webView.scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
        }

        self.actions = [[NSMutableDictionary alloc] init];
        self.layoutCallbackRegistered = NO;
        self.buttonCallbackRegistered = NO;
    }
    return self;
}

-(nullable UIView *)hitTest:(CGPoint)point withEvent:(nullable UIEvent *)event
{
    UIView* v = [super hitTest:point withEvent:event];
    //NSLog(@"touchtest webview hitTest=%@, %@\n%@", NSStringFromCGPoint(point), event, v);
    return v;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(nullable UIEvent *)event;
{
    //NSLog(@"touchtest webview pointInside=%@, %@", NSStringFromCGPoint(point), event);
    
    if(self.touchableAll || CGRectContainsPoint(self.touchableRect, point))
        return [super pointInside:point withEvent:event];
    else
        return NO;
}

-(void)setDragRect:(CGRect)rect {
    self.dragableRect = rect;
}

-(void)dragMe:(UIPanGestureRecognizer *)sender {
   //NSLog(@"drag FloatMenu! %@", gestureRecognizer);
    
    CGPoint translation = [sender translationInView:sender.view];
      
    //相对有手势父视图的坐标点(注意如果父视图是scrollView,locationPoint.x可能会大于视图的width)
    CGPoint locationPoint = [sender locationInView:sender.view];

    
    if(sender.state==UIGestureRecognizerStateBegan) {
        NSLog(@"drag start from %f, %f", locationPoint.x, locationPoint.y);
        self.startLocation = locationPoint;
    }
    
    if(sender.state==UIGestureRecognizerStateChanged) {
        
        if(!CGRectContainsPoint(self.dragableRect, self.startLocation))
            return;
        
        CGPoint pt = locationPoint;
        float dx = pt.x - self.startLocation.x;
        float dy = pt.y - self.startLocation.y;
        
        CGPoint newcenter = CGPointMake(self.center.x + dx, self.center.y + dy);

//        float halfx = CGRectGetMidX(self.bounds);
//        newcenter.x = MAX(halfx, newcenter.x);
//        newcenter.x = MIN(self.superview.bounds.size.width - halfx, newcenter.x);

        float halfy = CGRectGetMidY(self.bounds);
        newcenter.y = MAX(halfy, newcenter.y);
        //newcenter.y = MIN(self.superview.bounds.size.height - halfy, newcenter.y);
        
        self.center = newcenter;
        
        PGVSharedData->floatMenuRect = self.frame;
    }
}

// Register an action block, also inject a JS wrapper function as a WKUserScript.
// The wrapper uses window.prompt("__h5action__:name", args) for the synchronous bridge.
// For setLayoutAction/setButtonAction, the JS wrapper additionally stores the JS callback
// in a window global so native can invoke it later via evaluateJavaScript.
-(void)setAction:(NSString*)name callback:(id)block
{
    [self.actions setValue:block forKey:name];

    // Generate JS wrapper injected at document start (available before DOMContentLoaded)
    NSString *jsWrapper;

    if ([name isEqualToString:@"setLayoutAction"]) {
        jsWrapper = @"window.setLayoutAction = function(callback) {"
                    @"  window.__h5gg_layout_fn = callback;"
                    @"  window.prompt('__h5action__:setLayoutAction', '[]');"
                    @"};";
    } else if ([name isEqualToString:@"setButtonAction"]) {
        jsWrapper = @"window.setButtonAction = function(callback) {"
                    @"  window.__h5gg_button_fn = callback;"
                    @"  window.prompt('__h5action__:setButtonAction', '[]');"
                    @"};";
    } else {
        // Generic action: pass all arguments via prompt, parse optional return value
        jsWrapper = [NSString stringWithFormat:
            @"window.%@ = function() {"
            @"  var args = Array.prototype.slice.call(arguments);"
            @"  var r = window.prompt('__h5action__:%@', JSON.stringify(args));"
            @"  if (r !== null && r !== '' && r !== 'null') try { return JSON.parse(r); } catch(e) { return r; }"
            @"  return undefined;"
            @"};", name, name];
    }

    WKUserScript *script = [[WKUserScript alloc] initWithSource:jsWrapper
                                                  injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                               forMainFrameOnly:YES];
    [self.webView.configuration.userContentController addUserScript:script];
}

- (void)setH5ggEngine:(h5ggEngine*)engine {
    self.h5ggInstance = engine;

    // Inject h5gg synchronous bridge object at document start via WKUserScript.
    // All methods use window.prompt("__h5gg__:method", JSON.stringify(args)) which is
    // intercepted by runJavaScriptTextInputPanelWithPrompt: for synchronous execution.
    // pickScriptFile is async and uses postMessage instead.
    NSString *h5ggBridge =
        @"(function() {"
        @"  function h5sync(m, a) {"
        @"    var r = window.prompt('__h5gg__:' + m, JSON.stringify(a || []));"
        @"    if (r === null || r === '') return null;"
        @"    try { return JSON.parse(r); } catch(e) { return r; }"
        @"  }"
        @"  window.h5gg = {"
        @"    require: function(v) { return h5sync('require', [v]); },"
        @"    setFloatTolerance: function(v) { h5sync('setFloatTolerance', [v]); },"
        @"    searchNumber: function(v,t,f,to) { h5sync('searchNumber', [v,t,f,to]); },"
        @"    searchNearby: function(v,t,r) { h5sync('searchNearby', [v,t,r]); },"
        @"    getValue: function(a,t) { return h5sync('getValue', [a,t]); },"
        @"    setValue: function(a,v,t) { return h5sync('setValue', [a,v,t]); },"
        @"    editAll: function(v,t) { return h5sync('editAll', [v,t]); },"
        @"    getResults: function(m,s) { return h5sync('getResults', [m,s]) || []; },"
        @"    getResultsCount: function() { return h5sync('getResultsCount', []) || 0; },"
        @"    clearResults: function() { h5sync('clearResults', []); },"
        @"    getLocalScripts: function() { return h5sync('getLocalScripts', []) || []; },"
        @"    pickScriptFile: function(cb, types) {"
        @"      window.__h5gg_pickCallback = cb;"
        @"      window.webkit.messageHandlers.__h5gg_async__.postMessage({method:'pickScriptFile', types:types||null});"
        @"    },"
        @"    getRangesList: function(f) { return h5sync('getRangesList', [typeof f==='undefined'?null:f]) || []; },"
        @"    getProcList: function(f) {"
        @"      var r = window.prompt('__h5gg__:getProcList', JSON.stringify([typeof f==='undefined'?null:f]));"
        @"      if (r === null || r === 'null') return null;"
        @"      try { return JSON.parse(r); } catch(e) { return null; }"
        @"    },"
        @"    setTargetProc: function(p) { return h5sync('setTargetProc', [p]); },"
        @"    loadPlugin: function(c,d) { return h5sync('loadPlugin', [c,d]); },"
        @"    makeTweak: function(i,h) { return h5sync('makeTweak', [i,h]); }"
        @"  };"
        @"})();";

    WKUserScript *bridgeScript = [[WKUserScript alloc] initWithSource:h5ggBridge
                                                        injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                     forMainFrameOnly:YES];
    [self.webView.configuration.userContentController addUserScript:bridgeScript];
}

// MARK: - Alert / Confirm / Prompt (native-side, called from h5gg.h)

-(void)alert:(NSString*)message
{
    dispatch_async(dispatch_get_main_queue(), ^{[self.superview sendSubviewToBack:self];});

    if(self.usingCustomDialog)
        [ModalShow alert:@"H5GG" message:message InWindow:self.window];
    else
        [ModalShow alert:@"H5GG" message:message InWindow:self.window];

    dispatch_async(dispatch_get_main_queue(), ^{[self.superview bringSubviewToFront:self];});
}

-(BOOL)confirm:(NSString*)message
{
    BOOL result;

    dispatch_async(dispatch_get_main_queue(), ^{[self.superview sendSubviewToBack:self];});

    result = [ModalShow confirm:message InWindow:self.window];

    dispatch_async(dispatch_get_main_queue(), ^{[self.superview bringSubviewToFront:self];});

    return result;
}

-(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText
{
    NSString* result;

    dispatch_async(dispatch_get_main_queue(), ^{[self.superview sendSubviewToBack:self];});

    result = [ModalShow prompt:text defaultText:defaultText InWindow:self.window];

    dispatch_async(dispatch_get_main_queue(), ^{[self.superview bringSubviewToFront:self];});
    return result;
}

// MARK: - Callback invocation (native → JS)

-(void)invokeLayoutCallback:(CGFloat)width height:(CGFloat)height {
    if (!self.layoutCallbackRegistered) return;
    NSString *js = [NSString stringWithFormat:@"if(window.__h5gg_layout_fn) window.__h5gg_layout_fn(%f, %f)", width, height];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.webView evaluateJavaScript:js completionHandler:nil];
    });
}

-(void)invokeButtonCallback {
    if (!self.buttonCallbackRegistered) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.webView evaluateJavaScript:@"if(window.__h5gg_button_fn) window.__h5gg_button_fn()" completionHandler:nil];
    });
}

// MARK: - Delegate forwarding

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler {
    [self.webView evaluateJavaScript:javaScriptString completionHandler:completionHandler];
}

- (void)loadRequest:(NSURLRequest *)request {
    [self.webView loadRequest:request];
}

- (void)loadHTMLString:(NSString *)string baseURL:(nullable NSURL *)baseURL {
    [self.webView loadHTMLString:string baseURL:baseURL];
}

// MARK: - JSON helpers

-(NSArray*)parseJSONArray:(NSString*)jsonString {
    if (!jsonString || jsonString.length == 0) return @[];
    NSData *data = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
    id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [obj isKindOfClass:[NSArray class]] ? obj : @[];
}

-(NSString*)jsonStringFromObject:(id)obj {
    if (!obj) return nil;
    // Wrap non-collection types for JSON serialization
    id toSerialize = obj;
    if ([obj isKindOfClass:[NSString class]]) {
        toSerialize = @[obj]; // wrap then unwrap
        NSData *data = [NSJSONSerialization dataWithJSONObject:toSerialize options:0 error:nil];
        if (!data) return nil;
        NSString *arr = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        // Strip enclosing [ ] to get just the quoted string
        if (arr.length > 2)
            return [arr substringWithRange:NSMakeRange(1, arr.length - 2)];
        return nil;
    }
    if ([obj isKindOfClass:[NSNumber class]]) {
        // Boolean or numeric
        if (strcmp([obj objCType], @encode(BOOL)) == 0)
            return [obj boolValue] ? @"true" : @"false";
        return [obj stringValue];
    }
    NSData *data = [NSJSONSerialization dataWithJSONObject:obj options:0 error:nil];
    if (!data) return nil;
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

// MARK: - Synchronous h5gg method dispatcher (called from prompt bridge)

-(NSString*)dispatchH5ggMethod:(NSString*)method args:(NSArray*)args {
    if (!self.h5ggInstance) return nil;

    @try {
        if ([method isEqualToString:@"require"]) {
            BOOL r = [self.h5ggInstance require:[args[0] doubleValue]];
            return r ? @"true" : @"false";
        }
        if ([method isEqualToString:@"setFloatTolerance"]) {
            [self.h5ggInstance setFloatTolerance:[args[0] description]];
            return nil;
        }
        if ([method isEqualToString:@"searchNumber"]) {
            [self.h5ggInstance searchNumber:[args[0] description]
                                    param2:[args[1] description]
                                    param3:[args[2] description]
                                    param4:[args[3] description]];
            return nil;
        }
        if ([method isEqualToString:@"searchNearby"]) {
            [self.h5ggInstance searchNearby:[args[0] description]
                                    param2:[args[1] description]
                                    param3:[args[2] description]];
            return nil;
        }
        if ([method isEqualToString:@"getValue"]) {
            NSString *result = [self.h5ggInstance getValue:[args[0] description] param2:[args[1] description]];
            return [self jsonStringFromObject:result];
        }
        if ([method isEqualToString:@"setValue"]) {
            BOOL r = [self.h5ggInstance setValue:[args[0] description] param2:[args[1] description] param3:[args[2] description]];
            return r ? @"true" : @"false";
        }
        if ([method isEqualToString:@"editAll"]) {
            int count = [self.h5ggInstance editAll:[args[0] description] param3:[args[1] description]];
            return [NSString stringWithFormat:@"%d", count];
        }
        if ([method isEqualToString:@"getResults"]) {
            int maxCount = [args[0] intValue];
            int skipCount = [args[1] intValue];
            NSArray *results = [self.h5ggInstance getResults:maxCount param1:skipCount];
            return [self jsonStringFromObject:results ?: @[]];
        }
        if ([method isEqualToString:@"getResultsCount"]) {
            long count = [self.h5ggInstance getResultsCount];
            return [NSString stringWithFormat:@"%ld", count];
        }
        if ([method isEqualToString:@"clearResults"]) {
            [self.h5ggInstance clearResults];
            return nil;
        }
        if ([method isEqualToString:@"getLocalScripts"]) {
            NSArray *scripts = [self.h5ggInstance getLocalScripts];
            return [self jsonStringFromObject:scripts ?: @[]];
        }
        if ([method isEqualToString:@"getRangesList"]) {
            // Create a temporary JSContext to pass filter as JSValue (h5gg.h expects JSValue*)
            JSContext *ctx = [[JSContext alloc] init];
            JSValue *filterVal;
            if (args.count > 0 && args[0] != [NSNull null]) {
                filterVal = [JSValue valueWithObject:args[0] inContext:ctx];
            } else {
                filterVal = [JSValue valueWithUndefinedInContext:ctx];
            }
            NSArray *result = [self.h5ggInstance getRangesList:filterVal];
            return [self jsonStringFromObject:result ?: @[]];
        }
        if ([method isEqualToString:@"getProcList"]) {
            // getProcList uses [JSContext currentContext] internally which won't work outside JSC.
            // Reimplement the filtering logic here directly.
            NSString *filter = (args.count > 0 && args[0] != [NSNull null]) ? [args[0] description] : nil;

            NSArray* allproc = getRunningProcess();
            if (!allproc) return @"null"; // null = no cross-process permission

            NSMutableArray* newarr = [[NSMutableArray alloc] init];
            for (NSDictionary* proc in allproc) {
                char path[PATH_MAX] = {0};
                if (!proc_pidpath([[proc valueForKey:@"pid"] intValue], path, sizeof(path)))
                    continue;
                if (!RBIsUserAppExecutablePath(path))
                    continue;
                if (!filter || [filter isEqualToString:@""] || [filter isEqualToString:[proc valueForKey:@"name"]])
                    [newarr addObject:proc];
            }
            return [self jsonStringFromObject:newarr];
        }
        if ([method isEqualToString:@"setTargetProc"]) {
            pid_t pid = [args[0] intValue];
            BOOL r = [self.h5ggInstance setTargetProc:pid];
            return r ? @"true" : @"false";
        }
        if ([method isEqualToString:@"loadPlugin"]) {
            NSObject *result = [self.h5ggInstance loadPlugin:[args[0] description] path:[args[1] description]];
            // Cannot bridge arbitrary ObjC objects to WKWebView JS; return description
            return result ? [self jsonStringFromObject:[result description]] : @"null";
        }
        if ([method isEqualToString:@"makeTweak"]) {
            NSString *result = [self.h5ggInstance makeTweak:[args[0] description] with:[args[1] description]];
            return [self jsonStringFromObject:result];
        }
    }
    @catch (NSException *e) {
        NSLog(@"h5gg dispatch error [%@]: %@", method, e);
    }
    return nil;
}

// MARK: - Synchronous action dispatcher (called from prompt bridge)

-(NSString*)dispatchAction:(NSString*)name args:(NSArray*)args {
    id block = self.actions[name];
    if (!block) return nil;

    // Special: callback registration actions (block is called, but JS callback is in window global)
    if ([name isEqualToString:@"setLayoutAction"]) {
        self.layoutCallbackRegistered = YES;
        ((void(^)(void))block)();
        return @"true";
    }
    if ([name isEqualToString:@"setButtonAction"]) {
        self.buttonCallbackRegistered = YES;
        ((void(^)(void))block)();
        return @"true";
    }

    // No-arg blocks
    if (!args || args.count == 0) {
        ((void(^)(void))block)();
        return nil;
    }

    // 1-arg: string (setButtonImage returns BOOL)
    if (args.count == 1) {
        id arg = args[0];
        if ([arg isKindOfClass:[NSString class]]) {
            BOOL result = ((BOOL(^)(NSString*))block)(arg);
            return result ? @"true" : @"false";
        }
        if ([arg isKindOfClass:[NSNumber class]]) {
            ((void(^)(BOOL))block)([arg boolValue]);
            return nil;
        }
    }

    // 4-arg: int,int,int,int (setWindowRect, setWindowDrag, setWindowTouch)
    if (args.count == 4) {
        ((void(^)(int,int,int,int))block)(
            [args[0] intValue], [args[1] intValue],
            [args[2] intValue], [args[3] intValue]);
        return nil;
    }

    return nil;
}

// MARK: - WKScriptMessageHandler (async operations)

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    if (![message.name isEqualToString:@"__h5gg_async__"]) return;

    NSDictionary *body = message.body;
    NSString *method = body[@"method"];

    if ([method isEqualToString:@"pickScriptFile"]) {
        NSArray *types = body[@"types"];
        if (!types || [types isKindOfClass:[NSNull class]])
            types = @[@"public.executable", @"public.html"];

        [TopShow filePicker:types callback:^(NSString* path) {
            // Safely encode the path as JSON to prevent injection
            NSString *pathArg;
            if (path) {
                NSData *jsonData = [NSJSONSerialization dataWithJSONObject:@[path] options:0 error:nil];
                NSString *jsonArr = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
                pathArg = [NSString stringWithFormat:@"%@[0]", jsonArr];
            } else {
                pathArg = @"null";
            }
            NSString *js = [NSString stringWithFormat:@"if(window.__h5gg_pickCallback) window.__h5gg_pickCallback(%@);", pathArg];
            dispatch_async(dispatch_get_main_queue(), ^{
                [self.webView evaluateJavaScript:js completionHandler:nil];
            });
        }];
    }
}

// MARK: - WKUIDelegate (alert/confirm/prompt from JS)

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler {
    // Use ModalShow which handles both main thread and web thread correctly via semaphore
    [ModalShow alert:Localized(@"提示") message:message InWindow:self.window];
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL))completionHandler {
    BOOL result = [ModalShow confirm:message InWindow:self.window];
    completionHandler(result);
}

// THE KEY BRIDGE METHOD: intercepts window.prompt() calls from JS.
// Prompts prefixed with "__h5gg__:" dispatch to h5gg methods synchronously.
// Prompts prefixed with "__h5action__:" dispatch to registered action blocks.
// All other prompts show a normal input dialog.
- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString * _Nullable))completionHandler {

    // Synchronous h5gg method bridge
    if ([prompt hasPrefix:@"__h5gg__:"]) {
        NSString *method = [prompt substringFromIndex:9];
        NSArray *args = [self parseJSONArray:defaultText];
        NSString *result = [self dispatchH5ggMethod:method args:args];
        completionHandler(result);
        return;
    }

    // Synchronous action bridge
    if ([prompt hasPrefix:@"__h5action__:"]) {
        NSString *name = [prompt substringFromIndex:13];
        NSArray *args = [self parseJSONArray:defaultText];
        NSString *result = [self dispatchAction:name args:args];
        completionHandler(result);
        return;
    }

    // Normal prompt dialog
    NSString *result = [ModalShow prompt:prompt defaultText:defaultText ?: @"" InWindow:self.window];
    completionHandler(result);
}

// MARK: - WKNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    NSLog(@"webView %@ decidePolicyForNavigationAction %@", webView, navigationAction.request);

    if ([navigationAction.request.URL.scheme isEqualToString:@"file"]) {
        NSError *error = nil;
        NSString *html = [NSString stringWithContentsOfURL:navigationAction.request.URL encoding:NSASCIIStringEncoding error:&error];
        if (html) {
            NSInteger CR_count = [html length] - [[html stringByReplacingOccurrencesOfString:@"\r" withString:@""] length];
            NSInteger CRLF_count = ([html length] - [[html stringByReplacingOccurrencesOfString:@"\r\n" withString:@""] length]) / 2;
            if (CR_count > 0 && CR_count != CRLF_count) {
                [TopShow alert:Localized(@"提示") message:Localized(@"该页面为CR换行符格式, 请修改为LF或CRLF换行符格式, 否则JS错误提示无法显示准确的行数!")];
            }
        }
    }

    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    NSLog(@"webView %@ didFailNavigationWithError %@", webView, error);
    NSString *scheme = [[error.userInfo[NSURLErrorFailingURLErrorKey] scheme] lowercaseString];
    if ([scheme isEqualToString:@"file"] || [scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
        [TopShow alert:Localized(@"H5加载失败") message:[NSString stringWithFormat:@"%@", error]];
    }
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    NSLog(@"webView %@ didFailProvisionalNavigation %@", webView, error);
    NSString *scheme = [[error.userInfo[NSURLErrorFailingURLErrorKey] scheme] lowercaseString];
    if ([scheme isEqualToString:@"file"] || [scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
        [TopShow alert:Localized(@"H5加载失败") message:[NSString stringWithFormat:@"%@", error]];
    }
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    NSLog(@"webViewDidStartLoad=%@", webView);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSLog(@"webViewDidFinishLoad=%@", webView);

    // 禁止文本选择(matching original -webkit- style behavior)
    [self.webView evaluateJavaScript:@"document.body.style.webkitTouchCallout='none';" completionHandler:nil];
    [self.webView evaluateJavaScript:@"document.documentElement.style.webkitUserSelect='none';" completionHandler:nil];

    // Reset state on page load (matching original ts_didCreateJavaScriptContext behavior)
    self.dragableRect = CGRectZero;
    self.touchableAll = YES;
    self.touchableRect = CGRectZero;
    PGVSharedData->touchableAll = YES;
    PGVSharedData->touchableRect = CGRectZero;
    self.layoutCallbackRegistered = NO;
    self.buttonCallbackRegistered = NO;

    // Check for FastClick (causes UI freezes)
    [self.webView evaluateJavaScript:@"typeof FastClick" completionHandler:^(id result, NSError *error) {
        if (error == nil && [result isKindOfClass:[NSString class]] && ![result isEqualToString:@"undefined"]) {
            [self alert:Localized(@"发现FastClick模块!\n\n请将其从html中移除, 否则界面可能卡死!")];
        }
    }];

    if (self.reloadAction) {
        self.reloadAction();
    }
}

@end

#endif /* FloatMenu_h */
