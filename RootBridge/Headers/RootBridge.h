#ifndef rootbridge_h
#define rootbridge_h

#import <Foundation/Foundation.h>

@interface RootBridge : NSObject
+ (NSString *)getCallerPath;
+ (BOOL)isJBRootless;
+ (NSString *)getJBPath:(NSString *)path;
@end

#endif
