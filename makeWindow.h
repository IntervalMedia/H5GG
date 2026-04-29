//
//  makeWindow.h
//  h5gg
//
//  Created by admin on 24/4/2022.
//

#ifndef makeWindow_h
#define makeWindow_h

#import <UIKit/UIKit.h>

#pragma GCC diagnostic ignored "-Wnullability-completeness"

static inline BOOL H5GGWindowIsUsable(UIWindow *window) {
    return window && !window.hidden && window.alpha > 0.0;
}

static inline UIWindowScene *H5GGPreferredWindowScene(UIWindow *preferredWindow) API_AVAILABLE(ios(13.0));
static inline UIWindowScene *H5GGPreferredWindowScene(UIWindow *preferredWindow) {
    if (@available(iOS 13.0, *)) {
        if (preferredWindow.windowScene) {
            return preferredWindow.windowScene;
        }

        UIWindowScene *inactiveScene = nil;
        UIWindowScene *fallbackScene = nil;

        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:[UIWindowScene class]]) {
                continue;
            }

            UIWindowScene *windowScene = (UIWindowScene *)scene;
            if (!fallbackScene) {
                fallbackScene = windowScene;
            }

            if (windowScene.activationState == UISceneActivationStateForegroundActive) {
                return windowScene;
            }

            if (!inactiveScene && windowScene.activationState == UISceneActivationStateForegroundInactive) {
                inactiveScene = windowScene;
            }
        }

        return inactiveScene ?: fallbackScene;
    }

    return nil;
}

static inline UIWindow *H5GGPreferredWindow(UIWindow *preferredWindow) {
    if (H5GGWindowIsUsable(preferredWindow)) {
        return preferredWindow;
    }

    if (@available(iOS 13.0, *)) {
        NSMutableArray<UIWindowScene *> *candidateScenes = [NSMutableArray array];
        UIWindowScene *preferredScene = H5GGPreferredWindowScene(preferredWindow);
        if (preferredScene) {
            [candidateScenes addObject:preferredScene];
        }

        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if (![scene isKindOfClass:[UIWindowScene class]]) {
                continue;
            }
            UIWindowScene *windowScene = (UIWindowScene *)scene;
            if (![candidateScenes containsObject:windowScene]) {
                [candidateScenes addObject:windowScene];
            }
        }

        for (UIWindowScene *scene in candidateScenes) {
            for (UIWindow *window in scene.windows) {
                if (window.isKeyWindow && H5GGWindowIsUsable(window)) {
                    return window;
                }
            }
        }

        for (UIWindowScene *scene in candidateScenes) {
            for (UIWindow *window in scene.windows) {
                if (H5GGWindowIsUsable(window) && window.windowLevel == UIWindowLevelNormal) {
                    return window;
                }
            }
        }

        for (UIWindowScene *scene in candidateScenes) {
            for (UIWindow *window in scene.windows) {
                if (H5GGWindowIsUsable(window)) {
                    return window;
                }
            }
        }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([UIApplication sharedApplication].keyWindow) {
        return [UIApplication sharedApplication].keyWindow;
    }
    for (UIWindow *window in [UIApplication sharedApplication].windows) {
        if (H5GGWindowIsUsable(window)) {
            return window;
        }
    }
#pragma clang diagnostic pop

    return nil;
}

static inline NSArray<UIWindow *> *H5GGAllWindows(void) {
    NSMutableArray<UIWindow *> *windows = [NSMutableArray array];

    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                [windows addObjectsFromArray:((UIWindowScene *)scene).windows];
            }
        }
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [windows addObjectsFromArray:UIApplication.sharedApplication.windows];
#pragma clang diagnostic pop
    }

    return windows;
}

static inline UIViewController *H5GGVisibleViewController(UIViewController *controller) {
    UIViewController *visible = controller;

    while (visible.presentedViewController && visible.presentedViewController != visible) {
        visible = visible.presentedViewController;
    }

    if ([visible isKindOfClass:[UINavigationController class]]) {
        UINavigationController *navigationController = (UINavigationController *)visible;
        return H5GGVisibleViewController(navigationController.visibleViewController ?: navigationController.topViewController);
    }

    if ([visible isKindOfClass:[UITabBarController class]]) {
        UITabBarController *tabBarController = (UITabBarController *)visible;
        return H5GGVisibleViewController(tabBarController.selectedViewController ?: visible);
    }

    return visible;
}

static inline UIViewController *H5GGPresenterForWindow(UIWindow *preferredWindow) {
    UIWindow *window = H5GGPreferredWindow(preferredWindow);
    UIViewController *rootViewController = window.rootViewController;
    return rootViewController ? H5GGVisibleViewController(rootViewController) : nil;
}

static inline UIInterfaceOrientationMask H5GGOrientationMaskForOrientation(UIInterfaceOrientation orientation) {
    switch (orientation) {
        case UIInterfaceOrientationLandscapeLeft:
            return UIInterfaceOrientationMaskLandscapeLeft;
        case UIInterfaceOrientationLandscapeRight:
            return UIInterfaceOrientationMaskLandscapeRight;
        case UIInterfaceOrientationPortraitUpsideDown:
            return UIInterfaceOrientationMaskPortraitUpsideDown;
        case UIInterfaceOrientationPortrait:
        default:
            return UIInterfaceOrientationMaskPortrait;
    }
}

static inline UIInterfaceOrientation H5GGCurrentInterfaceOrientation(UIWindow *preferredWindow) {
    if (@available(iOS 13.0, *)) {
        UIWindowScene *scene = H5GGPreferredWindowScene(preferredWindow);
        if (scene) {
            return scene.interfaceOrientation;
        }
    }

    return UIInterfaceOrientationPortrait;
}

static inline UIInterfaceOrientationMask H5GGCurrentInterfaceOrientationMask(UIWindow *preferredWindow) {
    UIViewController *presenter = H5GGPresenterForWindow(preferredWindow);
    if (presenter) {
        return presenter.supportedInterfaceOrientations;
    }
    return H5GGOrientationMaskForOrientation(H5GGCurrentInterfaceOrientation(preferredWindow));
}

UIWindow* makeWindow(NSString* clazz)
{
    UIWindow* w = nil;
    
    if (@available(iOS 13.0, *)) {
        UIWindowScene *theScene = H5GGPreferredWindowScene(nil);
        if (theScene) {
            NSLog(@"makeWindow scene=%@ %@ state=%ld", theScene, theScene.windows, (long)theScene.activationState);
            w = [[NSClassFromString(clazz) alloc] initWithWindowScene:theScene];
        } else {
            CGRect frame = [UIScreen mainScreen].bounds;
            w = [[NSClassFromString(clazz) alloc] initWithFrame:frame];
        }
    } else {
        CGRect frame = [UIScreen mainScreen].bounds; //在iPad分屏或浮动模式下, 后面会被resize成真实尺寸
        w = [[NSClassFromString(clazz) alloc] initWithFrame:frame];
        NSLog(@"makeWindow=frame=%@", NSStringFromCGRect(w.frame));
    }
    
    return w;
}



@implementation UIWindow(GVWindow)

//这个在ipad支持悬浮模式的app中会触发两次viewWillTransitionToSize
- (void)private_updateToInterfaceOrientation:(UIInterfaceOrientation)orientation animated:(BOOL)animated
{
    NSLog(@"private_updateToInterfaceOrientation=%ld %d %@", (long)orientation, animated, self);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
    SEL mySelector = @selector(_updateToInterfaceOrientation:animated:);
#pragma clang diagnostic pop
    if (![self respondsToSelector:mySelector]) {
        NSLog(@"private_updateToInterfaceOrientation skipped: selector unavailable");
        return;
    }
    NSMethodSignature * sig = [[self class] instanceMethodSignatureForSelector:mySelector];
    if (!sig) {
        NSLog(@"private_updateToInterfaceOrientation skipped: no method signature");
        return;
    }
    NSInvocation * myInvocation = [NSInvocation invocationWithMethodSignature: sig];
    [myInvocation setTarget:self];
    [myInvocation setSelector: mySelector];
    [myInvocation setArgument:&orientation atIndex: 2];
    [myInvocation setArgument:&animated atIndex: 3];
    [myInvocation retainArguments];
    [myInvocation invoke];
}

@end

#endif /* makeWindow_h */
