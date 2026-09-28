package com.latencyplayer.sdk.internal;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.latencyplayer.sdk.LatencyPlayerManager;

/**
 * 断线重连管理器
 * <p>
 * 监听播放器断连事件，按照配置的策略自动重连。
 */
public class ReconnectManager {

    private static final String TAG = "ReconnectManager";

    /** 播放器实例 */
    private final LatencyPlayerManager player;

    /** 主线程Handler */
    private final Handler handler = new Handler(Looper.getMainLooper());

    /** 是否正在监控 */
    private volatile boolean isMonitoring = false;

    /** 当前重连次数 */
    private int reconnectCount = 0;

    /** 是否正在重连 */
    private volatile boolean isReconnecting = false;

    /** 重连任务 */
    private final Runnable reconnectRunnable = new Runnable() {
        @Override
        public void run() {
            if (!isMonitoring) {
                return;
            }
            attemptReconnect();
        }
    };

    public ReconnectManager(LatencyPlayerManager player) {
        this.player = player;
    }

    /**
     * 开始监控连接状态
     */
    public void startMonitor() {
        isMonitoring = true;
        reconnectCount = 0;
        Log.d(TAG, "Reconnect monitor started");
    }

    /**
     * 停止监控
     */
    public void stop() {
        isMonitoring = false;
        isReconnecting = false;
        handler.removeCallbacks(reconnectRunnable);
        Log.d(TAG, "Reconnect monitor stopped");
    }

    /**
     * 调度重连
     * <p>
     * 由播放器在检测到断连时调用。
     */
    public void scheduleReconnect() {
        if (!isMonitoring || isReconnecting) {
            return;
        }

        int maxCount = player.getConfig().getMaxReconnectCount();
        if (maxCount > 0 && reconnectCount >= maxCount) {
            Log.w(TAG, "Max reconnect attempts reached: " + maxCount);
            return;
        }

        int interval = player.getConfig().getReconnectInterval();
        Log.i(TAG, "Scheduling reconnect #" + (reconnectCount + 1) + " in " + interval + "s");

        isReconnecting = true;
        handler.postDelayed(reconnectRunnable, interval * 1000L);
    }

    /**
     * 尝试重连
     */
    private void attemptReconnect() {
        reconnectCount++;
        isReconnecting = false;

        Log.i(TAG, "Attempting reconnect #" + reconnectCount);

        // 通知回调
        // callback.onReconnecting(reconnectCount) is called via LatencyPlayerManager

        try {
            String url = player.getConfig().getUrl();
            if (url != null && !url.isEmpty()) {
                player.switchUrl(url);
            }
        } catch (Exception e) {
            Log.e(TAG, "Reconnect failed", e);
            // 调度下一次重连
            if (isMonitoring) {
                scheduleReconnect();
            }
        }
    }

    /**
     * 获取当前重连次数
     */
    public int getReconnectCount() {
        return reconnectCount;
    }

    /**
     * 重置重连计数
     */
    public void resetCount() {
        reconnectCount = 0;
    }
}
