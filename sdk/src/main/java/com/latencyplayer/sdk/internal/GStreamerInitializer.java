package com.latencyplayer.sdk.internal;

import android.content.Context;
import android.util.Log;

/**
 * GStreamer初始化管理器
 * <p>
 * 负责GStreamer引擎的单例初始化，确保只初始化一次。
 * GStreamer Android库由CMake自动链接，无需手动加载。
 */
public final class GStreamerInitializer {

    private static final String TAG = "GStreamerInit";

    private static volatile boolean isInitialized = false;
    private static final Object LOCK = new Object();

    private GStreamerInitializer() {
    }

    /**
     * 初始化GStreamer引擎
     * <p>
     * 使用双重检查锁确保线程安全的单例初始化。
     * 重复调用会直接返回，不会重复初始化。
     *
     * @param context Application Context
     */
    public static void init(Context context) {
        if (isInitialized) {
            return;
        }
        synchronized (LOCK) {
            if (isInitialized) {
                return;
            }
            try {
                Log.d(TAG, "Initializing GStreamer engine...");
                // GStreamer Android库由CMake构建系统自动链接
                // 这里只需要调用native初始化方法
                nativeInit(context);
                isInitialized = true;
                Log.d(TAG, "GStreamer engine initialized successfully");
            } catch (UnsatisfiedLinkError e) {
                Log.e(TAG, "Failed to load GStreamer native library", e);
                throw new RuntimeException("GStreamer initialization failed. " +
                    "Make sure GStreamer Android SDK is properly configured.", e);
            }
        }
    }

    /**
     * 检查是否已初始化
     */
    public static boolean isInitialized() {
        return isInitialized;
    }

    /**
     * 获取GStreamer版本信息
     */
    public static String getVersion() {
        if (!isInitialized) {
            return "Not initialized";
        }
        return nativeGetVersion();
    }

    // Native methods
    private static native void nativeInit(Context context);
    private static native String nativeGetVersion();
}
