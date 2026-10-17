//
//  LPLog.h
//  Internal logging helper for the LatencyPlayer iOS SDK.
//

#ifndef LPLog_h
#define LPLog_h

#import <Foundation/Foundation.h>

/// Set to NO to silence SDK logging (default YES).
FOUNDATION_EXPORT BOOL LPLogEnabled;

#define LPLog(fmt, ...) \
    do { if (LPLogEnabled) NSLog(@"[LatencyPlayer] " fmt, ##__VA_ARGS__); } while (0)

#define LPLogI(fmt, ...) LPLog("I/" fmt, ##__VA_ARGS__)
#define LPLogD(fmt, ...) LPLog("D/" fmt, ##__VA_ARGS__)
#define LPLogW(fmt, ...) LPLog("W/" fmt, ##__VA_ARGS__)
#define LPLogE(fmt, ...) LPLog("E/" fmt, ##__VA_ARGS__)

#endif /* LPLog_h */
