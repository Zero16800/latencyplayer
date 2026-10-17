//
//  LatencyPlayerView.h
//  LatencyPlayer iOS SDK
//
//  Render surface. On Android this wraps a SurfaceView; on iOS the video is
//  drawn on an AVSampleBufferDisplayLayer hosted by this UIView. The surface
//  lifecycle listener API mirrors
//  com.latencyplayer.sdk.LatencyPlayerView.OnSurfaceReadyListener.
//

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol LatencyPlayerSurfaceReadyListener <NSObject>

/// Surface 已就绪（对应 Android `onSurfaceReady(Surface)`）
- (void)onSurfaceReady;

/// Surface 尺寸变化（对应 Android `onSurfaceChanged(width, height)`）
- (void)onSurfaceChanged:(int)width height:(int)height;

/// Surface 已销毁（对应 Android `onSurfaceDestroyed()`）
- (void)onSurfaceDestroyed;

@end

@interface LatencyPlayerView : UIView

/// 视频渲染层. The layer the SDK feeds decoded frames into.
@property (nonatomic, readonly) AVSampleBufferDisplayLayer *displayLayer;

/// 视频渲染内容缩放方式. Mirrors Android `SurfaceView` + aspect handling.
@property (nonatomic, assign) AVLayerVideoGravity videoGravity;

/// 设置 Surface 就绪监听器（对应 Android `setOnSurfaceReadyListener`）
- (void)setOnSurfaceReadyListener:(nullable id<LatencyPlayerSurfaceReadyListener>)listener;

/// 当前 Surface 是否已就绪
@property (nonatomic, readonly) BOOL isSurfaceReady;

@end

NS_ASSUME_NONNULL_END
