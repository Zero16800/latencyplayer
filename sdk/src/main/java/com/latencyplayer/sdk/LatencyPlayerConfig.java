package com.latencyplayer.sdk;

/**
 * 播放器配置参数
 */
public class LatencyPlayerConfig {

    /** 默认缓冲时间(ms) */
    private static final int DEFAULT_BUFFER_TIME = 200;

    /** 默认重连间隔(s) */
    private static final int DEFAULT_RECONNECT_INTERVAL = 3;

    /** 播放地址 */
    private String url;

    /** 缓冲时间，范围 0-5000ms，0为最低延迟 */
    private int bufferTime = DEFAULT_BUFFER_TIME;

    /** 是否开启首屏秒开 */
    private boolean fastStartup = true;

    /** 是否开启超低延迟模式 */
    private boolean lowLatency = false;

    /** 是否硬解码优先 */
    private boolean hardwareDecoder = true;

    /** 是否自动重连 */
    private boolean autoReconnect = true;

    /** 重连间隔(秒) */
    private int reconnectInterval = DEFAULT_RECONNECT_INTERVAL;

    /** 最大重连次数，0表示无限重连 */
    private int maxReconnectCount = 0;

    /** 屏幕方向 1=竖屏, 2=横屏 */
    private int orientation = 1;

    /** 是否静音 */
    private boolean mute = false;

    /** 音量 0-100 */
    private int volume = 100;

    /** 是否自动重连 */
    private boolean enableAutoReconnect = true;

    public LatencyPlayerConfig() {
    }

    public LatencyPlayerConfig(String url) {
        this.url = url;
    }

    // ========== Builder Pattern ==========

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private final LatencyPlayerConfig config = new LatencyPlayerConfig();

        public Builder url(String url) {
            config.url = url;
            return this;
        }

        public Builder bufferTime(int bufferTime) {
            config.bufferTime = bufferTime;
            return this;
        }

        public Builder fastStartup(boolean fastStartup) {
            config.fastStartup = fastStartup;
            return this;
        }

        public Builder lowLatency(boolean lowLatency) {
            config.lowLatency = lowLatency;
            return this;
        }

        public Builder hardwareDecoder(boolean hardwareDecoder) {
            config.hardwareDecoder = hardwareDecoder;
            return this;
        }

        public Builder autoReconnect(boolean autoReconnect) {
            config.autoReconnect = autoReconnect;
            return this;
        }

        public Builder reconnectInterval(int seconds) {
            config.reconnectInterval = seconds;
            return this;
        }

        public Builder maxReconnectCount(int count) {
            config.maxReconnectCount = count;
            return this;
        }

        public Builder orientation(int orientation) {
            config.orientation = orientation;
            return this;
        }

        public Builder mute(boolean mute) {
            config.mute = mute;
            return this;
        }

        public Builder volume(int volume) {
            config.volume = volume;
            return this;
        }

        public LatencyPlayerConfig build() {
            return config;
        }
    }

    // ========== Getters & Setters ==========

    public String getUrl() {
        return url;
    }

    public void setUrl(String url) {
        this.url = url;
    }

    public int getBufferTime() {
        return bufferTime;
    }

    public void setBufferTime(int bufferTime) {
        this.bufferTime = Math.max(0, Math.min(5000, bufferTime));
    }

    public boolean isFastStartup() {
        return fastStartup;
    }

    public void setFastStartup(boolean fastStartup) {
        this.fastStartup = fastStartup;
    }

    public boolean isLowLatency() {
        return lowLatency;
    }

    public void setLowLatency(boolean lowLatency) {
        this.lowLatency = lowLatency;
    }

    public boolean isHardwareDecoder() {
        return hardwareDecoder;
    }

    public void setHardwareDecoder(boolean hardwareDecoder) {
        this.hardwareDecoder = hardwareDecoder;
    }

    public boolean isAutoReconnect() {
        return autoReconnect;
    }

    public void setAutoReconnect(boolean autoReconnect) {
        this.autoReconnect = autoReconnect;
    }

    public int getReconnectInterval() {
        return reconnectInterval;
    }

    public void setReconnectInterval(int reconnectInterval) {
        this.reconnectInterval = reconnectInterval;
    }

    public int getMaxReconnectCount() {
        return maxReconnectCount;
    }

    public void setMaxReconnectCount(int maxReconnectCount) {
        this.maxReconnectCount = maxReconnectCount;
    }

    public int getOrientation() {
        return orientation;
    }

    public void setOrientation(int orientation) {
        this.orientation = orientation;
    }

    public boolean isMute() {
        return mute;
    }

    public void setMute(boolean mute) {
        this.mute = mute;
    }

    public int getVolume() {
        return volume;
    }

    public void setVolume(int volume) {
        this.volume = Math.max(0, Math.min(100, volume));
    }

    public boolean isEnableAutoReconnect() {
        return enableAutoReconnect;
    }

    public void setEnableAutoReconnect(boolean enableAutoReconnect) {
        this.enableAutoReconnect = enableAutoReconnect;
    }
}
