#define __is__iOS9__ [[[UIDevice currentDevice] systemVersion] floatValue] >= 9.0

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

#import "headers.h"

@interface SBApplication (ContextHostManager)
@property NSString *bundleIdentifier;
@property NSString *displayIdentifier; 
@property NSString *displayName;
- (id)mainScene;
- (id)mainSceneHandle; // iOS 15+
@end

// iOS 15+ Scene Handling
@interface SBDeviceApplicationSceneHandle : NSObject
@property (nonatomic, readonly) NSString *sceneIdentifier;
@end

@interface SBSceneView : UIView
@property (nonatomic, readonly) long long displayMode;
- (void)invalidate;
@end

@interface SBDeviceApplicationSceneView : SBSceneView
@property (nonatomic, assign) NSInteger hostingPriority;
- (instancetype)initWithSceneHandle:(SBDeviceApplicationSceneHandle *)handle referenceSize:(CGSize)size orientation:(long long)orientation;
@end

@interface FBSceneLayerManager : NSObject
@property (nonatomic,readonly) NSOrderedSet * layers;
@end

@interface FBSceneHostManager : NSObject
-(void)setDefaultBackgroundColorWhileHosting:(UIColor *)arg1 ;
-(void)setDefaultBackgroundColorWhileNotHosting:(UIColor *)arg1 ;
-(id)hostViewForRequester:(id)arg1 enableAndOrderFront:(BOOL)arg2 ;
-(void)enableHostingForRequester:(id)arg1 orderFront:(BOOL)arg2 ;
-(void)disableHostingForRequester:(id)arg1 ;
-(id)initWithLayerManager:(id)arg1 scene:(id)arg2 ;
- (void)enableHostingForRequester:(id)arg1 priority:(int)arg2;
@end

@interface _UIExternalSceneLayerHostView : UIView 
-(id)initWithSceneLayer:(id)arg1 parentScene:(id)arg2 ;
@end

@interface _UIContextLayerHostView : UIView
-(id)initWithSceneLayer:(id)arg1 ;
@property (assign,nonatomic) unsigned long long renderingMode;
@end

@interface SBSceneManager
-(id)allScenes;
-(id)sceneIdentityForApplication:(id)arg1;
-(id)scenesMatchingPredicate:(id)arg1 ;
@end

@interface FBSceneLayer
-(NSString *)externalSceneID;
@end

@interface FBSMutableSceneSettings : NSObject
- (void)setBackgrounded:(bool)arg1; 
-(id)otherSettings;
@property (assign,getter=isForeground,nonatomic) BOOL foreground;
- (void)setLevel:(CGFloat)level; // iOS 15+
@end

@interface FBScene : NSObject
-(NSString *)identifier;
- (FBSceneHostManager *)hostManager;
- (id)mutableSettings;
-(id)settings; // iOS 15+
-(void)updateSettings:(id)arg1 withTransitionContext:(id)arg2 completion:(/*^block*/id)arg3 ;
- (void)_applyMutableSettings:(id)arg1 withTransitionContext:(id)arg2 completion:(id)arg3;
-(void)updateSettings:(id)arg1 withTransitionContext:(id)arg2 ;
-(void)setMutableSettings:(FBSMutableSceneSettings *)arg1 ;
@end

@interface FBSceneManager
+(id)sharedInstance;
-(id)sceneWithIdentifier:(id)arg1 ;
-(id)fbsSceneWithIdentifier:(id)arg1 ;
-(void)_startLayerHostingForScene:(id)arg1 ;
-(void)_stopLayerHostingForScene:(id)arg1 ;
-(id)_rootWindowForRootDisplayIdentity:(id)arg1 createIfNecessary:(BOOL)arg2 ;
-(id)_rootWindowForDisplayConfiguration:(id)arg1 createIfNecessary:(BOOL)arg2 ;
@end

@interface FBWindowContextHostManager : NSObject
-(void)_updateHostViewFrameForRequester:(id)arg1 ;
- (void)enableHostingForRequester:(id)arg1 orderFront:(BOOL)arg2;
- (void)enableHostingForRequester:(id)arg1 priority:(int)arg2;
- (void)disableHostingForRequester:(id)arg1;
- (id)hostViewForRequester:(id)arg1 enableAndOrderFront:(BOOL)arg2;
@end

@interface UIApplication (Private)
-(long long)_frontMostAppOrientation;
-(id)_accessibilityFrontMostApplication;
- (void)_relaunchSpringBoardNow;
- (void)launchApplicationWithIdentifier: (NSString*)identifier suspended: (BOOL)suspended;
- (id)displayIdentifier;
- (void)setStatusBarHidden:(bool)arg1 animated:(bool)arg2;
@end