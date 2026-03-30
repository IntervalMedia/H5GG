#import "ContextHostManager.h"

SBApplication *applicationForID(NSString *applicationID) {
    id controller = [objc_getClass("SBApplicationController") sharedInstance];
    
    if ([controller respondsToSelector:@selector(applicationWithDisplayIdentifier:)]) {
        return [controller applicationWithDisplayIdentifier:applicationID];
    } else {
        return [controller applicationWithBundleIdentifier:applicationID];
    }
}

@implementation ContextHostManager

+ (id)sharedInstance{
    static dispatch_once_t onceToken;
    static ContextHostManager *sharedInstance = nil;
    dispatch_once(&onceToken, ^{
        sharedInstance = [ContextHostManager new];
    });
    return sharedInstance;
}

-(UIView *)hostViewForBundleID:(NSString *)bundleId{
    if (@available(iOS 15.0, *)){
        return [self iOS15HostViewForBundleId:bundleId];
    } else {
        return [self hostViewForApplicationWithBundleID:bundleId];
    }
}

-(void)stopHostingView:(__weak UIView *)view forBundleId:(NSString *)bundleId{
    if (@available(iOS 15.0, *)){
        [self iOS15StopHostingForBundleId:bundleId view:view];
    } else {
        [self stopHostingForBundleID:bundleId];
    }
}

- (BOOL)isHostViewHosting:(UIView *)hostView {
    if (@available(iOS 15.0, *)){
        return (hostView && [hostView isKindOfClass:NSClassFromString(@"SBDeviceApplicationSceneView")]);
    } else if (@available(iOS 13, *)){
        return (hostView != nil);
    } else {
        if (hostView && [[hostView subviews] count] >= 1)
            return [(UIView *)[hostView subviews][0] respondsToSelector:@selector(isHosting)] ? [[(id)[hostView subviews][0] valueForKey:@"isHosting"] boolValue] : NO;
    }
    return NO;
}

#pragma mark - iOS 15+ Implementation

-(UIView *)iOS15HostViewForBundleId:(NSString *)bundleId {
    SBApplication *app = applicationForID(bundleId);
    if (!app) return nil;

    [self iOS15ForceApplicationForegroundForBundleId:bundleId];

    if ([app respondsToSelector:@selector(mainSceneHandle)]) {
        SBDeviceApplicationSceneHandle *sceneHandle = [app mainSceneHandle];
        if (sceneHandle) {
            CGSize refSize = [UIScreen mainScreen].bounds.size;
            UIWindowScene *activeScene = nil;
            
            for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
                if (scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass:[UIWindowScene class]]) {
                    activeScene = (UIWindowScene *)scene;
                    refSize = activeScene.screen.bounds.size;
                    break;
                }
            }
            
            long long orientation = activeScene ? activeScene.interfaceOrientation : 1; // 1 = Portrait
            
            Class sceneViewClass = NSClassFromString(@"SBDeviceApplicationSceneView");
            if (sceneViewClass) {
                SBDeviceApplicationSceneView *hostView = [[sceneViewClass alloc] initWithSceneHandle:sceneHandle referenceSize:refSize orientation:orientation];
                
                // Prioritize the scene to ensure it renders over Jetsam suspensions
                if ([hostView respondsToSelector:@selector(setHostingPriority:)]) {
                    hostView.hostingPriority = 1; 
                }
                
                return hostView;
            }
        }
    }
    return nil;
}

-(void)iOS15ForceApplicationForegroundForBundleId:(NSString *)bundleId {
    SBApplication *app = applicationForID(bundleId);
    if (!app) return;
    
    FBScene *scene = [app respondsToSelector:@selector(mainScene)] ? [app performSelector:@selector(mainScene)] : nil;
    if (scene && [scene respondsToSelector:@selector(settings)]) {
        FBSMutableSceneSettings *settings = [[scene settings] mutableCopy];
        
        if ([settings respondsToSelector:@selector(setForeground:)]) [settings setForeground:YES];
        if ([settings respondsToSelector:@selector(setBackgrounded:)]) [settings setBackgrounded:NO];
        if ([settings respondsToSelector:@selector(setLevel:)]) [settings setLevel:1.0];
        
        if ([scene respondsToSelector:@selector(updateSettings:withTransitionContext:)]) {
            [scene updateSettings:settings withTransitionContext:nil];
        }
    }
}

-(void)iOS15StopHostingForBundleId:(NSString *)bundleId view:(__weak UIView *)view {
    if (view && [view isKindOfClass:NSClassFromString(@"SBDeviceApplicationSceneView")]) {
        if ([view respondsToSelector:@selector(invalidate)]) {
            [(SBDeviceApplicationSceneView *)view invalidate];
        }
        [view removeFromSuperview];
    }
    
    // Release active level bounds
    SBApplication *app = applicationForID(bundleId);
    if (!app) return;
    FBScene *scene = [app respondsToSelector:@selector(mainScene)] ? [app performSelector:@selector(mainScene)] : nil;
    if (scene && [scene respondsToSelector:@selector(settings)]) {
        FBSMutableSceneSettings *settings = [[scene settings] mutableCopy];
        if ([settings respondsToSelector:@selector(setForeground:)]) [settings setForeground:NO];
        if ([scene respondsToSelector:@selector(updateSettings:withTransitionContext:)]) {
            [scene updateSettings:settings withTransitionContext:nil];
        }
    }
}


#pragma mark - Pre 13 implementation

- (UIView *)hostViewForApplication:(id)sbapplication {
    [self launchSuspendedApplicationWithBundleID:[(SBApplication *)sbapplication bundleIdentifier]];
    [self enableBackgroundingForApplication:sbapplication];
    [[self contextManagerForApplication:sbapplication] enableHostingForRequester:[(SBApplication *)sbapplication bundleIdentifier] orderFront:YES];
    id hostView = [[self contextManagerForApplication:sbapplication] hostViewForRequester:[(SBApplication *)sbapplication bundleIdentifier] enableAndOrderFront:YES];
    return hostView;
}

- (UIView *)hostViewForApplicationWithBundleID:(NSString *)bundleID {
    SBApplication *appToHost = applicationForID(bundleID);
    return [self hostViewForApplication:appToHost];
}

- (void)disableBackgroundingForApplication:(id)sbapplication {
    FBSMutableSceneSettings *sceneSettings = [self sceneSettingsForApplication:sbapplication];
    [sceneSettings setBackgrounded:YES];
    if (IS_IOS11orHIGHER) {
        [[self FBSceneForApplication:sbapplication] updateSettings:sceneSettings withTransitionContext:nil];
    }else{
        [[self FBSceneForApplication:sbapplication] _applyMutableSettings:sceneSettings withTransitionContext:nil completion:nil];
    }
}

- (void)enableBackgroundingForApplication:(id)sbapplication {
    FBSMutableSceneSettings *sceneSettings = [self sceneSettingsForApplication:sbapplication];
    [sceneSettings setBackgrounded:NO];
    if (IS_IOS11orHIGHER) {
        [[self FBSceneForApplication:sbapplication] updateSettings:sceneSettings withTransitionContext:nil];
    }else{
        [[self FBSceneForApplication:sbapplication] _applyMutableSettings:sceneSettings withTransitionContext:nil completion:nil];
    }
}

- (FBScene *)FBSceneForApplication:(id)sbapplication {
    FBScene* mainScene =  [(SBApplication *)sbapplication mainScene];
    return mainScene;
}

- (FBWindowContextHostManager *)contextManagerForApplication:(id)sbapplication {
    id contextHostManager = [[self FBSceneForApplication:sbapplication] hostManager];
    return contextHostManager;
}

- (FBSMutableSceneSettings *)sceneSettingsForApplication:(id)sbapplication {
    return [[[self FBSceneForApplication:sbapplication] mutableSettings] mutableCopy];
}

- (void)stopHostingForBundleID:(NSString *)bundleID {
    SBApplication *appToHost = [[NSClassFromString(@"SBApplicationController") sharedInstance] applicationWithBundleIdentifier:bundleID];
    [self disableBackgroundingForApplication:appToHost];
    FBWindowContextHostManager *contextManager = [self contextManagerForApplication:appToHost];
    [contextManager disableHostingForRequester:bundleID];
}

- (void)launchSuspendedApplicationWithBundleID:(NSString *)bundleID {
    [[UIApplication sharedApplication] launchApplicationWithIdentifier:bundleID suspended:YES];
}

@end