package com.latencyplayer.sdk;

/**
 * 播放状态枚举
 */
public enum LatencyPlayerState {

    /** 空闲状态，未初始化 */
    IDLE(0),

    /** 初始化中 */
    INITIALIZING(1),

    /** 已就绪，等待播放 */
    READY(2),

    /** 缓冲中 */
    BUFFERING(3),

    /** 正在播放 */
    PLAYING(4),

    /** 已暂停 */
    PAUSED(5),

    /** 正在停止 */
    STOPPING(6),

    /** 已停止 */
    STOPPED(7),

    /** 发生错误 */
    ERROR(8);

    private final int value;

    LatencyPlayerState(int value) {
        this.value = value;
    }

    public int getValue() {
        return value;
    }

    public static LatencyPlayerState fromValue(int value) {
        for (LatencyPlayerState state : values()) {
            if (state.value == value) {
                return state;
            }
        }
        return IDLE;
    }
}
