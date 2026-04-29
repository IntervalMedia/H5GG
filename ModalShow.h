//
//  ModalShow.h
//  h5gg
//
//  Created by admin on 21/3/2022.
//

#ifndef ModalShow_h
#define ModalShow_h

#include <objc/runtime.h>
#include "makeWindow.h"

#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wobjc-protocol-method-implementation"
#pragma GCC diagnostic ignored "-Wnullability-completeness"

@interface ModalShow : NSObject
+(void)alert:(NSString*)title message:(NSString*)message;
+(void)alert:(NSString*)title message:(NSString*)message InWindow:(UIWindow*)window;
+(BOOL)confirm:(NSString*)message;
+(BOOL)confirm:(NSString*)message InWindow:(UIWindow*)window;
+(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText;
+(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText InWindow:(UIWindow*)window;
@end

@implementation ModalShow

extern "C"  {
    NSRunLoop* WebThreadNSRunLoop(void);
    void* objc_autoreleasePoolPush();
    void WebThreadUnlock(void);
    void WebThreadLockPopModal(void);
    
    void (*WebThreadUnlockFromAnyThread)(void);
}

// Each call gets its own semaphore. The dismiss block is passed into the alert creation
// block so UIAlertAction handlers capture it directly — no shared state, no deadlocks
// when modals overlap (e.g. h5gg method error triggers alert while prompt bridge is waiting).
+(void)present:(UIViewController*(^)(void(^dismiss)(void)))alertBuilder InWindow:(UIWindow*)window {
    
    NSLog(@"ModalShow present[%d] %@", [NSThread isMainThread], [NSThread currentThread].name);
    
    dispatch_semaphore_t localSemaphore = dispatch_semaphore_create(0);
    
    void(^localDismiss)(void) = ^{
        dispatch_semaphore_signal(localSemaphore);
    };
    
    void(^submit)(void) = ^{
        NSLog(@"ModalShow running[%d] %@", [NSThread isMainThread], [NSThread currentThread].name);
        UIViewController *presenter = H5GGPresenterForWindow(window);
        if (!presenter) {
            NSLog(@"ModalShow skipped: no presenter");
            localDismiss();
            return;
        }

        UIViewController *alertController = alertBuilder(localDismiss);
        if (!alertController) {
            localDismiss();
            return;
        }

        [presenter presentViewController:alertController animated:YES completion:nil];
    };
    
    //NSLog(@"env=%@ %d",  [UIDevice currentDevice].systemVersion, [NSProcessInfo processInfo].isMacCatalystApp);
    
    if([NSThread isMainThread])
    {
        submit();
        while(dispatch_semaphore_wait(localSemaphore, DISPATCH_TIME_NOW))
            [[NSRunLoop currentRunLoop] runMode:[[NSRunLoop currentRunLoop] currentMode] beforeDate:[NSDate distantFuture]];
    } else {
        dispatch_async(dispatch_get_main_queue(), submit);
        
        *(void**)&WebThreadUnlockFromAnyThread = dlsym(RTLD_DEFAULT, "WebThreadUnlockFromAnyThread");
        
        if([[NSThread currentThread].name isEqualToString:@"WebThread"])
            WebThreadUnlockFromAnyThread();
        
        dispatch_semaphore_wait(localSemaphore, DISPATCH_TIME_FOREVER);
    }

    NSLog(@"ModalShow dismiss!");
}

+(void)alert:(NSString*)title message:(NSString*)message
{
    [self alert:title message:message InWindow:H5GGPreferredWindow(nil)];
}

+(BOOL)confirm:(NSString*)message
{
    return [self confirm:message InWindow:H5GGPreferredWindow(nil)];
}

+(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText
{
    return [self prompt:text defaultText:defaultText InWindow:H5GGPreferredWindow(nil)];
}

+(void)alert:(NSString*)title message:(NSString*)message InWindow:(UIWindow*)window
{
    [self present:^(void(^dismiss)(void)) {
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];

        [alert addAction:[UIAlertAction actionWithTitle:@"确定" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            dismiss();
        }]];
        
        return alert;
    }  InWindow:window ];
}

+(BOOL)confirm:(NSString*)message InWindow:(UIWindow*)window
{
    __block BOOL result = NO;
    
    [self present:^(void(^dismiss)(void)) {
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:Localized(@"提示") message:message preferredStyle:UIAlertControllerStyleAlert];

        [alert addAction:[UIAlertAction actionWithTitle:Localized(@"确定") style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            result = YES;
            dismiss();
        }]];
        
        [alert addAction:[UIAlertAction actionWithTitle:Localized(@"取消") style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
            result = NO;
            dismiss();
        }]];
    
        return alert;
    }  InWindow:window ];
    
    return result;
}

+(NSString*)prompt:(NSString*)text defaultText:(NSString*)defaultText InWindow:(UIWindow*)window {
    __block NSString* result;
    
    [self present:^(void(^dismiss)(void)) {
        
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:nil message:text preferredStyle:UIAlertControllerStyleAlert];
        
        [alert addTextFieldWithConfigurationHandler:^(UITextField * _Nonnull textField) {
            textField.text = defaultText;
        }];

        [alert addAction:[UIAlertAction actionWithTitle:Localized(@"确定") style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            result = alert.textFields.lastObject.text;
            dismiss();
        }]];

        [alert addAction:[UIAlertAction actionWithTitle:Localized(@"取消") style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
            result = nil;
            dismiss();
        }]];
    
        return alert;
    } InWindow:window ];
    
    return result;
}

@end


#endif /* ModalShow_h */
