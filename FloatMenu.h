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

@class h5ggEngine;

@interface FloatMenu : UIView <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler>
@property (nonatomic, strong) WKWebView *webView;
@property (nonatomic, strong) JSContext *jscontext;
@property (nonatomic, strong) h5ggEngine *h5ggInstance;
@property NSTimer* frontTimer;

@property BOOL touchableAll;
@property CGRect touchableRect;

@property CGRect dragableRect;
@property CGPoint startLocation;

@property NSMutableDictionary* actions;

@property BOOL usingCustomDialog;
@property void(^reloadAction)(void);

-(void)setAction:(NSString*)name callback:(id)block;
-(void)setH5ggEngine:(h5ggEngine*)engine;
- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler;
- (void)alert:(NSString*)message;

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
        
        // Initialize jscontext to nil - will be set via JavaScriptCore bridge if needed
        self.jscontext = nil;
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

-(void)setAction:(NSString*)name callback:(id)block
{
    // Check if handler is already registered to avoid duplicate registration
    BOOL alreadyRegistered = (self.actions[name] != nil);
    
    [self.actions setValue:block forKey:name];
    
    // Register script message handler for WKWebView (only if not already registered)
    if (!alreadyRegistered) {
        [self.webView.configuration.userContentController addScriptMessageHandler:self name:name];
    }
    
    // Also set in jscontext if available (for compatibility with JSExport objects)
    if(self.jscontext) self.jscontext[name] = block;
}

- (void)alert:(NSString*)message {
    [TopShow alert:Localized(@"提示") message:message];
}

- (void)setH5ggEngine:(h5ggEngine*)engine {
    self.h5ggInstance = engine;
    
    // Register all h5gg method handlers
    NSArray *h5ggMethods = @[
        @"h5gg_require",
        @"h5gg_setFloatTolerance", 
        @"h5gg_searchNumber",
        @"h5gg_searchNearby",
        @"h5gg_getValue",
        @"h5gg_setValue",
        @"h5gg_editAll",
        @"h5gg_getResults",
        @"h5gg_getResultsCount",
        @"h5gg_clearResults",
        @"h5gg_getLocalScripts",
        @"h5gg_pickScriptFile",
        @"h5gg_getRangesList",
        @"h5gg_getProcList",
        @"h5gg_setTargetProc",
        @"h5gg_loadPlugin",
        @"h5gg_makeTweak"
    ];
    
    for (NSString *methodName in h5ggMethods) {
        [self.webView.configuration.userContentController addScriptMessageHandler:self name:methodName];
    }
}

- (void)evaluateJavaScript:(NSString *)javaScriptString completionHandler:(void (^)(id, NSError *))completionHandler {
    [self.webView evaluateJavaScript:javaScriptString completionHandler:completionHandler];
}

- (void)loadRequest:(NSURLRequest *)request {
    [self.webView loadRequest:request];
}

- (void)loadHTMLString:(NSString *)string baseURL:(nullable NSURL *)baseURL {
    [self.webView loadHTMLString:string baseURL:baseURL];
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    // Handle h5gg method calls
    if ([message.name hasPrefix:@"h5gg_"] && self.h5ggInstance) {
        [self handleH5ggMessage:message];
        return;
    }
    
    // Handle regular action callbacks
    id callback = self.actions[message.name];
    if (callback) {
        // Check if callback is a block or an object
        if ([callback isKindOfClass:NSClassFromString(@"NSBlock")]) {
            // It's a block, call it directly
            ((void (^)(id))callback)(message.body);
        } else {
            // It's an object (like h5ggEngine) - this requires JSContext integration
            // WKWebView doesn't natively support JSExport objects
            // The object was likely meant to be exposed via JSContext
            NSLog(@"Warning: Received message for object '%@' but WKWebView cannot directly expose JSExport objects. JSContext integration required.", message.name);
            
            // Store in jscontext if available for backwards compatibility
            if (self.jscontext) {
                self.jscontext[message.name] = callback;
            }
        }
    }
}

- (void)handleH5ggMessage:(WKScriptMessage *)message {
    NSDictionary *body = message.body;
    NSString *method = [message.name substringFromIndex:5]; // Remove "h5gg_" prefix
    NSArray *args = body[@"args"];
    NSString *callbackId = body[@"callbackId"];
    
    id result = nil;
    NSError *error = nil;
    BOOL isAsync = NO;
    
    @try {
        if ([method isEqualToString:@"require"]) {
            double minver = [args[0] doubleValue];
            result = @([self.h5ggInstance require:minver]);
        }
        else if ([method isEqualToString:@"setFloatTolerance"]) {
            [self.h5ggInstance setFloatTolerance:args[0]];
        }
        else if ([method isEqualToString:@"searchNumber"]) {
            [self.h5ggInstance searchNumber:args[0] param2:args[1] param3:args[2] param4:args[3]];
        }
        else if ([method isEqualToString:@"searchNearby"]) {
            [self.h5ggInstance searchNearby:args[0] param2:args[1] param3:args[2]];
        }
        else if ([method isEqualToString:@"getValue"]) {
            result = [self.h5ggInstance getValue:args[0] param2:args[1]];
        }
        else if ([method isEqualToString:@"setValue"]) {
            result = @([self.h5ggInstance setValue:args[0] param2:args[1] param3:args[2]]);
        }
        else if ([method isEqualToString:@"editAll"]) {
            result = @([self.h5ggInstance editAll:args[0] param3:args[1]]);
        }
        else if ([method isEqualToString:@"getResults"]) {
            int maxCount = [args[0] intValue];
            int skipCount = [args[1] intValue];
            result = [self.h5ggInstance getResults:maxCount param1:skipCount];
        }
        else if ([method isEqualToString:@"getResultsCount"]) {
            result = @([self.h5ggInstance getResultsCount]);
        }
        else if ([method isEqualToString:@"clearResults"]) {
            [self.h5ggInstance clearResults];
        }
        else if ([method isEqualToString:@"getLocalScripts"]) {
            result = [self.h5ggInstance getLocalScripts];
        }
        else if ([method isEqualToString:@"pickScriptFile"]) {
            // Handle pickScriptFile with callback
            isAsync = YES;
            NSArray *types = (args.count > 0 && args[0] != [NSNull null]) ? args[0] : nil;
            
            [TopShow filePicker:types ?: @[@"public.executable", @"public.html"] callback:^(NSString* path) {
                NSString *pathJSON = path ? [NSString stringWithFormat:@"\"%@\"", path] : @"null";
                NSString *jsCallback = [NSString stringWithFormat:@"window.__h5gg_callbacks['%@'](%@);delete window.__h5gg_callbacks['%@'];",
                                       callbackId, pathJSON, callbackId];
                dispatch_async(dispatch_get_main_queue(), ^{
                    [self.webView evaluateJavaScript:jsCallback completionHandler:nil];
                });
            }];
        }
        else if ([method isEqualToString:@"getRangesList"]) {
            // Convert filter string to nil if undefined/null
            NSString *filter = (args.count > 0 && args[0] != [NSNull null]) ? args[0] : nil;
            
            // Call getRangesList with nil since we can't create JSValue here
            // The implementation checks for undefined/toString, so we'll pass nil for undefined
            result = [self.h5ggInstance getRangesList:nil];
            
            // If filter is provided, we need to filter the results ourselves
            if (filter && ![filter isEqualToString:@""]) {
                NSMutableArray *filtered = [[NSMutableArray alloc] init];
                for (NSDictionary *range in (NSArray*)result) {
                    NSString *name = range[@"name"];
                    if ([name rangeOfString:filter].location != NSNotFound || 
                        [filter isEqualToString:@"0"]) {
                        [filtered addObject:range];
                        if ([filter isEqualToString:@"0"]) break;
                    }
                }
                result = filtered;
            }
        }
        else if ([method isEqualToString:@"getProcList"]) {
            // Convert filter string to nil if undefined/null
            NSString *filter = (args.count > 0 && args[0] != [NSNull null]) ? args[0] : nil;
            
            // getProcList returns JSValue which we can't use here
            // Instead, we'll get the process list and filter it ourselves
            NSArray* allproc = getRunningProcess();
            if (allproc) {
                NSMutableArray* newarr = [[NSMutableArray alloc] init];
                
                for (NSDictionary* proc in allproc) {
                    char path[PATH_MAX] = {0};
                    
                    if (!proc_pidpath([[proc valueForKey:@"pid"] intValue], path, sizeof(path)))
                        continue;
                    
                    if (strstr(path, "/private/var/") != path && strstr(path, "/var/") != path)
                        continue;
                    
                    if (strstr(path, "/Application/") == NULL)
                        continue;
                    
                    if (!filter || [filter isEqualToString:@""] || 
                        [filter isEqualToString:[proc valueForKey:@"name"]]) {
                        [newarr addObject:proc];
                    }
                }
                result = newarr;
            } else {
                result = nil;
            }
        }
        else if ([method isEqualToString:@"setTargetProc"]) {
            pid_t pid = [args[0] intValue];
            result = @([self.h5ggInstance setTargetProc:pid]);
        }
        else if ([method isEqualToString:@"loadPlugin"]) {
            result = [self.h5ggInstance loadPlugin:args[0] path:args[1]];
        }
        else if ([method isEqualToString:@"makeTweak"]) {
            result = [self.h5ggInstance makeTweak:args[0] with:args[1]];
        }
    }
    @catch (NSException *exception) {
        error = [NSError errorWithDomain:@"h5gg" code:-1 userInfo:@{NSLocalizedDescriptionKey: exception.reason ?: @"Unknown error"}];
    }
    
    // Send result back to JavaScript (if not async)
    if (!isAsync && callbackId) {
        NSString *resultJSON = @"null";
        if (result) {
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:result ?: @"" options:0 error:nil];
            if (jsonData) {
                resultJSON = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
            }
        }
        
        NSString *jsCallback = [NSString stringWithFormat:@"window.__h5gg_callbacks['%@'](%@);delete window.__h5gg_callbacks['%@'];",
                               callbackId, resultJSON, callbackId];
        [self.webView evaluateJavaScript:jsCallback completionHandler:nil];
    }
}

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

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    NSLog(@"webViewDidStartLoad=%@", webView);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    NSLog(@"webViewDidFinishLoad=%@", webView);

    [self.webView evaluateJavaScript:@"document.body.style.webkitTouchCallout='none';" completionHandler:nil];
    [self.webView evaluateJavaScript:@"document.documentElement.style.webkitUserSelect='none';" completionHandler:nil];

    // Inject JavaScript bridge for h5gg if instance is set
    if (self.h5ggInstance) {
        NSString *h5ggBridge = @"(function() {"
            @"  if (typeof window.h5gg !== 'undefined') return;"
            @"  window.__h5gg_callbacks = {};"
            @"  window.__h5gg_callback_id = 0;"
            @"  "
            @"  function createH5ggMethod(methodName, argCount) {"
            @"    return function() {"
            @"      var args = Array.prototype.slice.call(arguments, 0, argCount);"
            @"      var callback = arguments[argCount];"
            @"      var callbackId = null;"
            @"      if (typeof callback === 'function') {"
            @"        callbackId = 'cb_' + (++window.__h5gg_callback_id);"
            @"        window.__h5gg_callbacks[callbackId] = callback;"
            @"      }"
            @"      window.webkit.messageHandlers['h5gg_' + methodName].postMessage({"
            @"        args: args,"
            @"        callbackId: callbackId"
            @"      });"
            @"    };"
            @"  }"
            @"  "
            @"  window.h5gg = {"
            @"    require: createH5ggMethod('require', 1),"
            @"    setFloatTolerance: createH5ggMethod('setFloatTolerance', 1),"
            @"    searchNumber: createH5ggMethod('searchNumber', 4),"
            @"    searchNearby: createH5ggMethod('searchNearby', 3),"
            @"    getValue: createH5ggMethod('getValue', 2),"
            @"    setValue: createH5ggMethod('setValue', 3),"
            @"    editAll: createH5ggMethod('editAll', 2),"
            @"    getResults: createH5ggMethod('getResults', 2),"
            @"    getResultsCount: createH5ggMethod('getResultsCount', 0),"
            @"    clearResults: createH5ggMethod('clearResults', 0),"
            @"    getLocalScripts: createH5ggMethod('getLocalScripts', 0),"
            @"    pickScriptFile: createH5ggMethod('pickScriptFile', 2),"
            @"    getRangesList: createH5ggMethod('getRangesList', 1),"
            @"    getProcList: createH5ggMethod('getProcList', 1),"
            @"    setTargetProc: createH5ggMethod('setTargetProc', 1),"
            @"    loadPlugin: createH5ggMethod('loadPlugin', 2),"
            @"    makeTweak: createH5ggMethod('makeTweak', 2)"
            @"  };"
            @"})();";
        [self.webView evaluateJavaScript:h5ggBridge completionHandler:nil];
    }

    // Inject JavaScript bridge for registered actions
    // Note: Simple function callbacks can use WKScriptMessageHandler
    for (NSString *actionName in self.actions) {
        id callback = self.actions[actionName];
        
        // Only create message handler wrappers for block callbacks
        // Skip h5gg as it's handled separately above
        if ([callback isKindOfClass:NSClassFromString(@"NSBlock")]) {
            NSString *jsCode = [NSString stringWithFormat:
                @"if (typeof %@ === 'undefined' && window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.%@) {"
                @"  window.%@ = function(arg) {"
                @"    window.webkit.messageHandlers.%@.postMessage(arg);"
                @"  };"
                @"}", actionName, actionName, actionName, actionName];
            [self.webView evaluateJavaScript:jsCode completionHandler:nil];
        } else if (![actionName isEqualToString:@"h5gg"]) {
            // Log warning only for non-h5gg objects
            NSLog(@"Warning: Action '%@' is an object that requires JSContext integration", actionName);
            if (self.jscontext) {
                self.jscontext[actionName] = callback;
            }
        }
    }

    // Check for FastClick
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
