//
//  LatencyPlayerState.h
//  LatencyPlayer iOS SDK
//
//  Playback state. Values are identical to the Android SDK
//  (com.latencyplayer.sdk.LatencyPlayerState).
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, LatencyPlayerState) {
    /** IDLE — 空闲状态，未初始化 */
    LatencyPlayerStateIdle         = 0,
    /** INITIALIZING — 初始化中 */
    LatencyPlayerStateInitializing = 1,
    /** READY — 已就绪，等待播放 */
    LatencyPlayerStateReady        = 2,
    /** BUFFERING — 缓冲中 */
    LatencyPlayerStateBuffering    = 3,
    /** PLAYING — 正在播放 */
    LatencyPlayerStatePlaying      = 4,
    /** PAUSED — 已暂停 */
    LatencyPlayerStatePaused       = 5,
    /** STOPPING — 正在停止 */
    LatencyPlayerStateStopping     = 6,
    /** STOPPED — 已停止 */
    LatencyPlayerStateStopped      = 7,
    /** ERROR — 发生错误 */
    LatencyPlayerStateError        = 8,
};

/// Returns the state matching a numeric value (Java `fromValue`). Unknown
/// values map to `LatencyPlayerStateIdle`.
FOUNDATION_EXPORT LatencyPlayerState LatencyPlayerStateFromValue(NSInteger value);

/// Human-readable (English) name of a state, e.g. `"PLAYING"`.
FOUNDATION_EXPORT NSString *LatencyPlayerStateToString(LatencyPlayerState state);

NS_ASSUME_NONNULL_END
