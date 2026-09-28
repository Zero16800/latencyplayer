package com.latencyplayer.sdk;

/**
 * 播放器事件回调接口
 */
public interface LatencyPlayerCallback {

    /**
     * 播放状态变化回调
     *
     * @param state 新的播放状态
     */
    void onStateChanged(LatencyPlayerState state);

    /**
     * 错误回调
     *
     * @param errorCode 错误码，参见 {@link LatencyPlayerError}
     * @param message   错误描述
     */
    void onError(int errorCode, String message);

    /**
     * 缓冲进度回调
     *
     * @param percent 缓冲百分比 0-100
     */
    void onBuffering(int percent);

    /**
     * 连接成功回调
     */
    void onConnected();

    /**
     * 连接断开回调
     */
    void onDisconnected();

    /**
     * 首帧渲染回调
     */
    void onFirstFrameRendered();

    /**
     * 下载速度回调
     *
     * @param bytesPerSec 每秒下载字节数
     */
    void onDownloadSpeed(long bytesPerSec);

    /**
     * 正在重连回调
     *
     * @param attemptCount 第几次重连
     */
    void onReconnecting(int attemptCount);

    /**
     * 视频尺寸变化回调
     *
     * @param width  视频宽度
     * @param height 视频高度
     */
    void onVideoSizeChanged(int width, int height);

    /**
     * 播放完成回调（仅用于非直播场景）
     */
    void onPlaybackCompleted();
}
