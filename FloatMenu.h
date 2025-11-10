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

INCTXT(INITIAL_JS, "initial.js");

static NSHashTable* g_webViews = nil;

typedef id (*objc_method_pointer)(id,SEL,...);
objc_method_pointer g_orig_didCreateJavaScriptContext=NULL;

@interface FloatMenu : UIView <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler>
@property (nonatomic, strong) WKWebView *webView;
@property (nonatomic, strong) JSContext *jscontext;
@property NSTimer* frontTimer;

@property BOOL touchableAll;
@property CGRect touchableRect;

@property CGRect dragableRect;
@property CGPoint startLocation;

@property NSMutableDictionary* actions;

@property BOOL usingCustomDialog;
@property void(^reloadAction)(void);

-(void)setAction:(NSString*)name callback:(id)block;
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
    [self.actions setValue:block forKey:name];
    
    // Register script message handler for WKWebView
    [self.webView.configuration.userContentController addScriptMessageHandler:self name:name];
    
    // Also set in jscontext if available (for compatibility)
    if(self.jscontext) self.jscontext[name] = block;
}

- (void)alert:(NSString*)message {
    [TopShow alert:Localized(@"提示") message:message];
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
    id callback = self.actions[message.name];
    if (callback) {
        ((void (^)(id))callback)(message.body);
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

    // Inject JavaScript bridge for registered actions
    // Note: This creates wrapper functions that use WKScriptMessageHandler
    // For complex objects like h5gg, JSContext integration may still be needed
    for (NSString *actionName in self.actions) {
        NSString *jsCode = [NSString stringWithFormat:
            @"if (typeof %@ === 'undefined' && window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.%@) {"
            @"  window.%@ = function(arg) {"
            @"    window.webkit.messageHandlers.%@.postMessage(arg);"
            @"  };"
            @"}", actionName, actionName, actionName, actionName];
        [self.webView evaluateJavaScript:jsCode completionHandler:nil];
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
