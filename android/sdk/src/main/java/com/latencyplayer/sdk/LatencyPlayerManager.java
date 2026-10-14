package com.latencyplayer.sdk;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.latencyplayer.sdk.internal.GStreamerInitializer;
import com.latencyplayer.sdk.internal.ReconnectManager;

/**
 * RTMP播放器核心管理类
 * <p>
 * 基于GStreamer实现RTMP/RTSP流媒体播放。
 * 提供播放、暂停、停止、音量控制、截图等功能。
 *
 * <p>使用示例：
 * <pre>
 * LatencyPlayerManager player = new LatencyPlayerManager();
 * player.init(context);
 * player.setConfig(LatencyPlayerConfig.builder()
 *     .url("rtmp://example.com/live/stream")
 *     .bufferTime(200)
 *     .build());
 * player.setSurface(surfaceView);
 * player.setCallback(callback);
 * player.start();
 * </pre>
 */
public class LatencyPlayerManager {

    private static final String TAG = "LatencyPlayerManager";

    static {
        System.loadLibrary("latencyplayer");
    }

    /** Native层句柄 */
    private long nativeHandle;

    /** 主线程Handler */
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    /** 配置 */
    private LatencyPlayerConfig config;

    /** 回调 */
    private LatencyPlayerCallback callback;

    /** 播放View */
    private LatencyPlayerView playerView;

    /** 重连管理器 */
    private ReconnectManager reconnectManager;

    /** 当前状态 */
    private volatile LatencyPlayerState currentState = LatencyPlayerState.IDLE;

    /** 是否已初始化 */
    private volatile boolean isInitialized = false;

    /** 是否已释放 */
    private volatile boolean isReleased = false;

    /**
     * 初始化播放器
     *
     * @param context Application Context
     */
    public void init(@NonNull Context context) {
        if (isInitialized) {
            Log.w(TAG, "Already initialized");
            return;
        }
        if (isReleased) {
            throw new IllegalStateException("Player has been released, cannot reinitialize");
        }

        Log.d(TAG, "Initializing player...");
        GStreamerInitializer.init(context.getApplicationContext());
        nativeHandle = nativeCreate();
        if (nativeHandle == 0) {
            throw new RuntimeException("Failed to create native player instance");
        }

        config = new LatencyPlayerConfig();
        reconnectManager = new ReconnectManager(this);
        isInitialized = true;
        setState(LatencyPlayerState.READY);
        Log.d(TAG, "Player initialized successfully");
    }

    /**
     * 设置播放器配置
     */
    public void setConfig(@NonNull LatencyPlayerConfig config) {
        checkInitialized();
        this.config = config;
        nativeSetBufferTime(nativeHandle, config.getBufferTime());
        nativeSetLowLatency(nativeHandle, config.isLowLatency());
        nativeSetMute(nativeHandle, config.isMute());
        nativeSetVolume(nativeHandle, config.getVolume());
    }

    /**
     * 获取当前配置
     */
    @NonNull
    public LatencyPlayerConfig getConfig() {
        checkInitialized();
        return config;
    }

    /**
     * 设置播放回调
     */
    public void setCallback(@Nullable LatencyPlayerCallback callback) {
        this.callback = callback;
    }

    /**
     * 设置播放View
     */
    public void setSurface(@NonNull LatencyPlayerView view) {
        checkInitialized();
        this.playerView = view;
        view.setOnSurfaceReadyListener(new LatencyPlayerView.OnSurfaceReadyListener() {
            @Override
            public void onSurfaceReady(Surface surface) {
                Log.i(TAG, "onSurfaceReady: " + surface);
                nativeSetSurface(nativeHandle, surface);
            }

            @Override
            public void onSurfaceChanged(int width, int height) {
                Surface s = playerView != null && playerView.getSurfaceHolder() != null
                        ? playerView.getSurfaceHolder().getSurface() : null;
                if (s != null && s.isValid()) {
                    Log.i(TAG, "onSurfaceChanged: rebind " + width + "x" + height);
                    nativeSetSurface(nativeHandle, s);
                }
            }

            @Override
            public void onSurfaceDestroyed() {
                Log.i(TAG, "onSurfaceDestroyed");
                if (nativeHandle != 0) {
                    nativeSetSurface(nativeHandle, null);
                }
            }
        });

        // Surface may already be created before callback registration — bind now.
        if (view.getSurfaceHolder() != null && view.getSurfaceHolder().getSurface() != null
                && view.getSurfaceHolder().getSurface().isValid()) {
            Log.i(TAG, "Surface already ready, binding immediately");
            nativeSetSurface(nativeHandle, view.getSurfaceHolder().getSurface());
        }
    }

    /**
     * 直接设置Surface
     */
    public void setSurface(@NonNull Surface surface) {
        checkInitialized();
        nativeSetSurface(nativeHandle, surface);
    }

    /**
     * 开始播放
     */
    public void start() {
        checkInitialized();
        if (config.getUrl() == null || config.getUrl().isEmpty()) {
            throw new IllegalStateException("URL not set, call setConfig() first");
        }

        Log.i(TAG, "Starting playback: " + config.getUrl());
        setState(LatencyPlayerState.BUFFERING);
        nativePlay(nativeHandle, config.getUrl());

        if (config.isAutoReconnect()) {
            reconnectManager.startMonitor();
        }
    }

    /**
     * 停止播放
     */
    public void stop() {
        checkInitialized();
        Log.i(TAG, "Stopping playback");
        reconnectManager.stop();
        nativeStop(nativeHandle);
        setState(LatencyPlayerState.STOPPED);
    }

    /**
     * 暂停播放
     */
    public void pause() {
        checkInitialized();
        Log.d(TAG, "Pausing playback");
        nativePause(nativeHandle);
        setState(LatencyPlayerState.PAUSED);
    }

    /**
     * 恢复播放
     */
    public void resume() {
        checkInitialized();
        Log.d(TAG, "Resuming playback");
        nativeResume(nativeHandle);
        setState(LatencyPlayerState.PLAYING);
    }

    /**
     * 设置静音
     */
    public void setMute(boolean mute) {
        checkInitialized();
        config.setMute(mute);
        nativeSetMute(nativeHandle, mute);
    }

    /**
     * 是否静音
     */
    public boolean isMute() {
        return config != null && config.isMute();
    }

    /**
     * 设置音量
     *
     * @param volume 音量值 0-100
     */
    public void setVolume(int volume) {
        checkInitialized();
        config.setVolume(volume);
        nativeSetVolume(nativeHandle, volume);
    }

    /**
     * 设置缓冲时间
     *
     * @param bufferTimeMs 缓冲时间(毫秒)，范围0-5000
     */
    public void setBufferTime(int bufferTimeMs) {
        checkInitialized();
        config.setBufferTime(bufferTimeMs);
        nativeSetBufferTime(nativeHandle, bufferTimeMs);
    }

    /**
     * 热切换URL
     *
     * @param newUrl 新的播放地址
     */
    public void switchUrl(@NonNull String newUrl) {
        checkInitialized();
        Log.i(TAG, "Switching URL to: " + newUrl);
        config.setUrl(newUrl);
        nativeStop(nativeHandle);
        setState(LatencyPlayerState.BUFFERING);
        nativePlay(nativeHandle, newUrl);
    }

    /**
     * 截图（编码并写入文件，PNG 或 JPEG 由 savePath 后缀决定）
     *
     * @param savePath 截图保存路径，如 /sdcard/Download/snap.png
     * @return true 表示文件已成功写入
     */
    public boolean saveSnapshot(@NonNull String savePath) {
        checkInitialized();
        return nativeSaveSnapshot(nativeHandle, savePath);
    }

    /**
     * 设置播放方向
     *
     * @param orientation 1=竖屏, 2=横屏
     */
    public void setOrientation(int orientation) {
        checkInitialized();
        config.setOrientation(orientation);
        nativeSetOrientation(nativeHandle, orientation);
    }

    /**
     * 设置视频翻转
     *
     * @param flipHorizontal 水平翻转
     * @param flipVertical   垂直翻转
     */
    public void setFlip(boolean flipHorizontal, boolean flipVertical) {
        checkInitialized();
        nativeSetFlip(nativeHandle, flipHorizontal, flipVertical);
    }

    /**
     * 设置视频旋转角度
     *
     * @param degrees 旋转角度 0/90/180/270
     */
    public void setRotation(int degrees) {
        checkInitialized();
        nativeSetRotation(nativeHandle, degrees);
    }

    /**
     * 是否正在播放
     */
    public boolean isPlaying() {
        return currentState == LatencyPlayerState.PLAYING;
    }

    /**
     * 获取当前状态
     */
    @NonNull
    public LatencyPlayerState getState() {
        return currentState;
    }

    /**
     * 释放播放器资源
     */
    public void release() {
        if (isReleased) {
            return;
        }
        Log.d(TAG, "Releasing player");
        reconnectManager.stop();
        if (nativeHandle != 0) {
            nativeRelease(nativeHandle);
            nativeHandle = 0;
        }
        isReleased = true;
        isInitialized = false;
    }

    // ========== 内部方法 ==========

    private void checkInitialized() {
        if (!isInitialized) {
            throw new IllegalStateException("Player not initialized, call init() first");
        }
        if (isReleased) {
            throw new IllegalStateException("Player has been released");
        }
    }

    private void setState(LatencyPlayerState newState) {
        if (currentState != newState) {
            currentState = newState;
            if (callback != null) {
                mainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (callback != null) {
                            callback.onStateChanged(currentState);
                        }
                    }
                });
            }
        }
    }

    // ========== GStreamer回调方法 ==========

    @SuppressWarnings("unused")
    private void onGStreamerMessage(int type, long param1, long param2) {
        Log.d(TAG, "GStreamer message: type=" + type + ", param1=" + param1 + ", param2=" + param2);
        switch (type) {
            case 0: // STATE_CHANGED
                LatencyPlayerState newState = LatencyPlayerState.fromValue((int) param1);
                setState(newState);
                break;

            case 1: // ERROR
                setState(LatencyPlayerState.ERROR);
                if (callback != null) {
                    final int errorCode = (int) param1;
                    final String msg = LatencyPlayerError.getDescription(errorCode);
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onError(errorCode, msg);
                            }
                        }
                    });
                    // 触发重连
                    if (config != null && config.isAutoReconnect()) {
                        reconnectManager.scheduleReconnect();
                    }
                }
                break;

            case 2: // BUFFERING
                if (callback != null) {
                    final int percent = (int) param1;
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onBuffering(percent);
                            }
                        }
                    });
                }
                break;

            case 3: // CONNECTED
                setState(LatencyPlayerState.PLAYING);
                if (callback != null) {
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onConnected();
                            }
                        }
                    });
                }
                break;

            case 4: // DISCONNECTED
                if (callback != null) {
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onDisconnected();
                            }
                        }
                    });
                }
                break;

            case 5: // FIRST_FRAME
                setState(LatencyPlayerState.PLAYING);
                if (callback != null) {
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onFirstFrameRendered();
                            }
                        }
                    });
                }
                break;

            case 6: // DOWNLOAD_SPEED
                if (callback != null) {
                    final long speed = param1;
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onDownloadSpeed(speed);
                            }
                        }
                    });
                }
                break;

            case 7: // VIDEO_SIZE_CHANGED
                if (callback != null) {
                    final int width = (int) param1;
                    final int height = (int) param2;
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onVideoSizeChanged(width, height);
                            }
                        }
                    });
                }
                break;

            case 8: // PLAYBACK_COMPLETED
                setState(LatencyPlayerState.STOPPED);
                if (callback != null) {
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (callback != null) {
                                callback.onPlaybackCompleted();
                            }
                        }
                    });
                }
                break;
        }
    }

    // ========== Native方法声明 ==========

    private native long nativeCreate();

    private native void nativeSetSurface(long handle, Surface surface);

    private native void nativePlay(long handle, String uri);

    private native void nativeStop(long handle);

    private native void nativePause(long handle);

    private native void nativeResume(long handle);

    private native void nativeSetMute(long handle, boolean mute);

    private native void nativeSetVolume(long handle, int volume);

    private native void nativeSetBufferTime(long handle, int bufferTimeMs);

    private native void nativeSetLowLatency(long handle, boolean enable);

    private native void nativeSetOrientation(long handle, int orientation);

    private native void nativeSetFlip(long handle, boolean horizontal, boolean vertical);

    private native void nativeSetRotation(long handle, int degrees);

    private native boolean nativeSaveSnapshot(long handle, String path);

    private native void nativeRelease(long handle);
}
