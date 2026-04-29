#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wincomplete-implementation"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-W#warnings"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wnullability-completeness"

#import <UIKit/UIKit.h>
#import <pthread.h>
#include <dlfcn.h>
#include <libgen.h>

#include "ContextHostManager.h"
#include "globalview.h"
#include "libAPAppView.h"
#include "../FloatButton.h"
#include "../makeWindow.h"
#include "../incbin.h"
#import "../RootBridge/Headers/RootBridge.h"

FILE* logger = NULL;
#define LOGGER(...) if(logger){fprintf(logger, __VA_ARGS__);fprintf(logger,"\r\n");fflush(logger);}


bool g_dismiss_on_switchapp = true;
bool g_dismiss_on_backtohome = true;

//嵌入图标文件
INCBIN(Icon, "../icon.png");

NSString* g_pinnedBundleId = nil;
UIImage* g_pinnedBundleIcon = nil;

GVData GVSharedData = GVDataDefault;

FloatButton* floatBtn;
APAppView* appView=nil;
UIView* hostView = nil;
UIWindow* GlobalView=nil;

UIViewController *getViewControllerWithView(UIView *view){
    UIResponder *responder = view;
    while ((responder = [responder nextResponder]))
        if ([responder isKindOfClass: [UIViewController class]])
            return (UIViewController *)responder;
    return nil;
}

void alphaHostView(UIView* view)
{
    if(view.opaque) {
        view.opaque = NO;
        view.backgroundColor=[UIColor clearColor];
    }
    for(UIView* subview in view.subviews)
    {
        alphaHostView(subview);
    }
}

void handleHostView(UIView* view, CGRect newFrame)
{
    UIViewController* vc = getViewControllerWithView(view);
    NSLog(@"GlobalView=handleHostView=%@(%@) : %@",
            NSStringFromClass(view.class), vc?NSStringFromClass(vc.class):@"", NSStringFromClass(view.class));

    if([NSStringFromClass(view.class) isEqualToString:@"SBHomeGrabberView"]) {
        view.alpha=0; //works fine
        return;
    }

    // Adjust frame based on safeAreaInsets if on iOS 15+ to prevent Home Grabber overlap issues
    CGRect targetFrame = newFrame;
    if (@available(iOS 15.0, *)) {
        UIWindow *keyWindow = H5GGPreferredWindow(nil);
        if (keyWindow) {
            targetFrame = UIEdgeInsetsInsetRect(newFrame, keyWindow.safeAreaInsets);
        }
    }

    if(view.frame.origin.x==newFrame.origin.x
    &&view.frame.origin.y==newFrame.origin.y
    && view.frame.size.width==newFrame.size.height
    && view.frame.size.height==newFrame.size.width
    ) {
        NSLog(@"GlobalView=forceRotation=%@", view);
        view.frame = targetFrame;
    }

    for(UIView* subview in view.subviews)
    {
        handleHostView(subview, targetFrame);
    }
}


@interface GVWindow : UIWindow
@end

@implementation GVWindow
- (BOOL)pointInside:(CGPoint)point withEvent:(nullable UIEvent *)event;
{
    int count = (int)self.subviews.count;
    for (int i = count - 1; i >= 0;i-- ) {
        UIView *childV = self.subviews[i];
        CGPoint childP = [self convertPoint:point toView:childV];
        UIView *fitView = [childV hitTest:childP withEvent:event];
        if(fitView) {
            if(childV==hostView) {
                if(GVSharedData.touchableAll) {
                    return CGRectContainsPoint(GVSharedData.floatMenuRect, childP);
                } else {
                    return CGRectContainsPoint(GVSharedData.touchableRect, childP);
                }
            }
            return YES;
        }
    }
    return NO;
}

-(void)setHidden:(BOOL)hidden
{
    [super setHidden:hidden];
    NSLog(@"FloatWindow setHidden=%d", hidden);

    if(hidden==NO)
    {
        UIView* superview = self.rootViewController.view;
        while(superview && ![superview isKindOfClass:UIWindow.class])
        {
            [superview setHidden:YES];
            superview = superview.superview;
        }
    }
}

@end

@interface GVController : UIViewController
@end

void dumpview(int i, UIView* view)
{
   for(int j=0; j<view.subviews.count; j++)
   {
       UIView* subView = view.subviews[j];
       NSString* tag=@"";
       for(int a=0;a<i;a++) tag = [tag stringByAppendingString:@"--"];
        NSLog(@"GlobalView=dumpview=%@ %d, %@:%@",tag, subView.isHidden, NSStringFromCGRect(subView.frame), NSStringFromClass(subView.class));
   }
}

@interface SBApplicationProcessState
@property(readonly, nonatomic) int pid;
@end
@interface SBApplication()
 @property SBApplicationProcessState* processState;
@end


@implementation GVController
- (BOOL)shouldAutorotate {
    NSString *deviceType = [UIDevice currentDevice].model;
    if([deviceType isEqualToString:@"iPad"]) return YES;
    return UIDevice.currentDevice.orientation==UIDeviceOrientationPortraitUpsideDown ? NO:YES;
}

// iOS 15 Modernization: Transitioning away from SB activeInterfaceOrientation
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    if (@available(iOS 15.0, *)) {
        UIWindowScene *windowScene = self.view.window.windowScene;
        if (windowScene) {
            return (UIInterfaceOrientationMask)(1 << windowScene.interfaceOrientation);
        }
    }
    SpringBoard* sbapp = (SpringBoard*)[UIApplication sharedApplication];
    return (UIInterfaceOrientationMask)(1<<sbapp.activeInterfaceOrientation);
}

-(UIInterfaceOrientation) preferredInterfaceOrientationForPresentation {
    if (@available(iOS 15.0, *)) {
        UIWindowScene *windowScene = self.view.window.windowScene;
        if (windowScene) {
            return windowScene.interfaceOrientation;
        }
    }
    SpringBoard* sbapp = (SpringBoard*)[UIApplication sharedApplication];
    return (UIInterfaceOrientation)sbapp.activeInterfaceOrientation;
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id <UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    
    // iOS 15 Modernization: UIWindowScene Orientation logic
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        if (hostView) {
            hostView.frame = CGRectMake(0, 0, size.width, size.height);
        }
    } completion:nil];
}
@end

Class AXBackgrounderManager;
@interface AXBackgrounderManagerClass : NSObject
+(void)setForeground:(id)app WithBool:(BOOL)enable;
+(BOOL)isForeground:(id)app;
+(void)setForegroundSceneID:(id)app WithBool:(BOOL)enable;
+(void)setDictionary:(NSString*)bundleId WithBool:(BOOL)enable;
@end

void toggleGlobalView()
{
    static UIAlertController *alertloading = nil;

    static NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*t) {
                
        if(hostView) {
            GVSharedData.viewHosted = YES;
            alphaHostView(hostView);

            if(!CGAffineTransformIsIdentity(hostView.transform)) {
                hostView.transform = CGAffineTransformIdentity;
            }
            if(hostView.frame.origin.x!=0 || hostView.frame.origin.y!=0) {
                hostView.frame = CGRectMake(0, 0, hostView.frame.size.width, hostView.frame.size.height);
            }

            SBApplication *appToHost = applicationForID(g_pinnedBundleId);
            bool running = appToHost && appToHost.processState;

            if(!running)
            {
                NSLog(@"GlobalView=monitor=%d", running);
                GVSharedData = GVDataDefault;
                [hostView removeFromSuperview];

                if (@available(iOS 15.0, *)) {
                    [ContextHostManager.sharedInstance stopHostingView:hostView forBundleId:g_pinnedBundleId];
                } else if (@available(iOS 13, *)) {
                    [appView stopAppView];
                } else {
                    [ContextHostManager.sharedInstance stopHostingView:hostView forBundleId:g_pinnedBundleId];
                }

                hostView = nil;
            }
            else if(!hostView.superview && GVSharedData.enable && GVSharedData.appLoaded) {
                NSLog(@"GlobalView=apploaded");
                if(alertloading) {
                    [GlobalView.rootViewController dismissViewControllerAnimated:YES completion:nil];
                    alertloading = nil;
                }
                [GlobalView addSubview:hostView];
            }

            if(GVSharedData.setWindowVisible) {
                [hostView setHidden:!GVSharedData.windowVisibleState];
                GVSharedData.setWindowVisible = NO;
            }
        }
    }];

    if(hostView==nil)
    {
        BOOL running = GVSharedData.enable;
        static NSTimer* timer2 = nil;

        if (@available(iOS 15.0, *)) {
            GVSharedData.followCurrentOrientation = YES;
            
            hostView = [ContextHostManager.sharedInstance hostViewForBundleID:g_pinnedBundleId];
            if (hostView) {
                handleHostView(hostView, GlobalView.frame);
                NSLog(@"GlobalView=iOS15SceneView=%@", hostView);
                [AXBackgrounderManager setForeground:g_pinnedBundleId WithBool:YES];
            } else {
                running = false;
            }
            
        } else if (@available(iOS 13, *)) {
            GVSharedData.followCurrentOrientation = YES;

            hostView = [appView viewForBundleID:g_pinnedBundleId];
            handleHostView(hostView, GlobalView.frame);
            NSLog(@"GlobalView=APAppView=%@", hostView);
            
            [AXBackgrounderManager setForeground:g_pinnedBundleId WithBool:YES];
        } else {
            hostView = [ContextHostManager.sharedInstance hostViewForBundleID:g_pinnedBundleId];
            
            if(!hostView) running = false;

            timer2 = [NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*t) {
                hostView = [ContextHostManager.sharedInstance hostViewForBundleID:g_pinnedBundleId];
                if(hostView) {
                    NSLog(@"GlobalView=gothostview=%@", hostView);
                    [AXBackgrounderManager setDictionary:g_pinnedBundleId WithBool:YES];
                    [timer2 invalidate];
                    timer2 = nil;
                }
            }];
        }

        if(running)
        {
            [GlobalView addSubview:hostView];
        }
        else
        {
            alertloading = [UIAlertController alertControllerWithTitle:@"正在启动\nLoading" message:@"" preferredStyle:UIAlertControllerStyleAlert];
            [alertloading addAction:[UIAlertAction actionWithTitle:@". . ." style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
                alertloading = nil;
                if(timer2) {
                    [timer2 invalidate];
                    timer2 = nil;
                }
            }]];
            [GlobalView.rootViewController presentViewController:alertloading animated:YES completion:nil];
        }
        return;
    }

    if(hostView.superview)
        [hostView setHidden:!hostView.isHidden];
}

void initload()
{
    LOGGER("run in initload");
    
    void* backgrounder=NULL;
    if (@available(iOS 13, *)) {
        const char *pathB = [[RootBridge getJBPath:@"/Library/MobileSubstrate/DynamicLibraries/libH5GG.B.dylib"] fileSystemRepresentation];
        backgrounder = dlopen(pathB, RTLD_NOW);
        const char *pathA = [[RootBridge getJBPath:@"/Library/MobileSubstrate/DynamicLibraries/libH5GG.A.dylib"] fileSystemRepresentation];
        dlopen(pathA, RTLD_NOW);
        Class APAppViewClass = NSClassFromString(@"APAppView");
        appView = [[APAppViewClass alloc] init];
    } else {
        const char *pathB12 = [[RootBridge getJBPath:@"/Library/MobileSubstrate/DynamicLibraries/libH5GG.B12.dylib"] fileSystemRepresentation];
        backgrounder = dlopen(pathB12, RTLD_NOW);
    }

    AXBackgrounderManager = NSClassFromString(@"AXBackgrounderManager");
    NSLog(@"GlobalView=backgrounder=%p, %@", backgrounder, AXBackgrounderManager);

    GlobalView = makeWindow(NSStringFromClass(GVWindow.class));
    
    if (@available(iOS 13.0, *)) {
        UIWindowScene *scene = H5GGPreferredWindowScene(nil);
        if (scene) {
            GlobalView.windowScene = scene;
        }
    }
    
    GlobalView.layer.masksToBounds = YES;
    GlobalView.backgroundColor=[UIColor clearColor];
    GlobalView.windowLevel = UIWindowLevelStatusBar + 1;
    GlobalView.rootViewController = [[GVController alloc] init];

    floatBtn = [[FloatButton alloc] init];
    floatBtn.keepWindow = YES;

    if(!g_pinnedBundleIcon) {
        NSData* iconData = [[NSData alloc] initWithBytes:gIconData length:gIconSize];
        g_pinnedBundleIcon = [[UIImage alloc] initWithData:iconData];
    }
    [floatBtn setIcon:g_pinnedBundleIcon];

    [floatBtn setAction:^(void) {
        NSLog(@"GlobalView=clickbutton");
        if(GVSharedData.enable && GVSharedData.customButtonAction)
        {
            if(!hostView) {
                toggleGlobalView();
                return;
            }

            if(hostView.isHidden) {
                [hostView setHidden:NO];
                return;
            }

            GVSharedData.floatBtnClick = YES;

        } else {
            toggleGlobalView();
        }
    }];

    [GlobalView addSubview:floatBtn]; 
    [GlobalView setHidden:NO];

    static NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*t){
        SpringBoard* sbapp = (SpringBoard*)[UIApplication sharedApplication];

        static id lastApp = sbapp._accessibilityFrontMostApplication;
        if(lastApp!=sbapp._accessibilityFrontMostApplication) {
            NSLog(@"GlobalView appchange=%@ => %@", lastApp, sbapp._accessibilityFrontMostApplication);
            if(hostView && !hostView.isHidden) {
                if( (g_dismiss_on_switchapp && lastApp && sbapp._accessibilityFrontMostApplication)
                || (g_dismiss_on_backtohome && !sbapp._accessibilityFrontMostApplication))
                    toggleGlobalView();
            }
            lastApp = sbapp._accessibilityFrontMostApplication;
        }

        static long long lastOrientation=sbapp.activeInterfaceOrientation;
        GVSharedData.curOrientation = (UIInterfaceOrientation)sbapp.activeInterfaceOrientation;
        
        if(lastOrientation!=sbapp.activeInterfaceOrientation) {
            lastOrientation=sbapp.activeInterfaceOrientation;
            if (@available(iOS 15.0, *)) {
                 // Do not force private rotation on modern iOS if not explicitly required, handled natively
            } else {
                [GlobalView performSelector:@selector(private_updateToInterfaceOrientation:animated:) withObject:@(sbapp.activeInterfaceOrientation) withObject:@(YES)];
            }
        }

        if(GVSharedData.buttonImageSize) {
            NSData* iconData = [[NSData alloc] initWithBytes:GVSharedData.buttonImageData length:GVSharedData.buttonImageSize];
            g_pinnedBundleIcon = [[UIImage alloc] initWithData:iconData];
            if(g_pinnedBundleIcon) [floatBtn setIcon:g_pinnedBundleIcon];
            GVSharedData.buttonImageSize = 0;
        }

        SBApplication *appToHost = applicationForID(g_pinnedBundleId);
        bool running = appToHost && appToHost.processState;
        if(floatBtn.isHidden!=!running) {
            LOGGER("state change %d", running);
            [floatBtn setHidden:!running];
        }
    }];
}

static void* thread_running(void* arg)
{
    LOGGER("run in newthread");
    sleep(2);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        LOGGER("run in main");
        __block NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:1 repeats:YES block:^(NSTimer*t){
        LOGGER("run in timer");
            if(UIApplication.sharedApplication && H5GGPreferredWindow(nil)) {
                LOGGER("run in appdone");
                [timer invalidate];
                initload();
            }
        }];
    });
    return 0;
}

static void __attribute__((constructor)) _init_()
{
    struct dl_info di={0};
    dladdr((void*)_init_, &di);

    LOGGER("run in %s", NSBundle.mainBundle.bundleIdentifier.UTF8String);

    if([NSBundle.mainBundle.bundleIdentifier isEqualToString:@"com.apple.springboard"])
    {
        NSString* plistPath = [NSString stringWithUTF8String:di.dli_fname];
        char* p = (char*)plistPath.UTF8String + strlen(di.dli_fname) - 5;
        strcpy(p, "plist");
        
        NSDictionary* plist = [[NSDictionary alloc] initWithContentsOfFile:plistPath];
        NSLog(@"plist=%@\n%@\n%@\n%@", plistPath, plist, plist[@"Filter"], plist[@"Filter"][@"Bundles"]);
        if(plist && [plist[@"Filter"][@"Bundles"] count]<=2) {
            for(NSString* bundleId in plist[@"Filter"][@"Bundles"]) {
                if([bundleId isEqualToString:@"com.apple.springboard"]) 
                {
                    pthread_t thread;
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_create(&thread, &attr, thread_running, nil);
                } else {
                    g_pinnedBundleId = bundleId;
                }
            }
        }
    } else {
        void (*SetGlobalView)(char* dylib, UInt64 GVDataOffset);
        *(void**)&SetGlobalView = dlsym(RTLD_DEFAULT, "SetGlobalView");
        SetGlobalView((char*)di.dli_fname, (UInt64)&GVSharedData-(UInt64)di.dli_fbase);
    }
}
