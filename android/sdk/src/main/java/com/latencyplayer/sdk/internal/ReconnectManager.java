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

    /** 视频停止流动判定阈值：连续这么久没有新视频帧即认为管线卡死 */
    private static final long STALL_THRESHOLD_MS = 3000;

    /** 卡死检查周期 */
    private static final long STALL_CHECK_INTERVAL_MS = 1000;

    /**
     * 卡死看门狗
     * <p>
     * 退到后台/被系统冻结返回后，管线可能仍在收字节但视频帧不再流动
     * （画面黑屏）。这里周期性检查最近一帧的年龄，超阈值即立即重连自恢复。
     */
    private final Runnable stallCheckRunnable = new Runnable() {
        @Override
        public void run() {
            if (!isMonitoring) {
                return;
            }
            try {
                if (!isReconnecting && player.isPlaying()) {
                    long age = player.getLastFrameAgeMs();
                    if (age > STALL_THRESHOLD_MS) {
                        Log.w(TAG, "Video stalled: no frame for " + age
                                + "ms, forcing reconnect");
                        isReconnecting = true;
                        attemptReconnect();
                    }
                }
            } catch (Throwable t) {
                Log.e(TAG, "Stall check failed", t);
            }
            handler.postDelayed(this, STALL_CHECK_INTERVAL_MS);
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
        handler.removeCallbacks(stallCheckRunnable);
        handler.postDelayed(stallCheckRunnable, STALL_CHECK_INTERVAL_MS);
        Log.d(TAG, "Reconnect monitor started");
    }

    /**
     * 停止监控
     */
    public void stop() {
        isMonitoring = false;
        isReconnecting = false;
        handler.removeCallbacks(reconnectRunnable);
        handler.removeCallbacks(stallCheckRunnable);
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
