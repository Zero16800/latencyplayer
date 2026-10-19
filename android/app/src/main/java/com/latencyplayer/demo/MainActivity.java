package com.latencyplayer.demo;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.latencyplayer.sdk.LatencyPlayerCallback;
import com.latencyplayer.sdk.LatencyPlayerConfig;
import com.latencyplayer.sdk.LatencyPlayerError;
import com.latencyplayer.sdk.LatencyPlayerManager;
import com.latencyplayer.sdk.LatencyPlayerState;
import com.latencyplayer.sdk.LatencyPlayerView;

import java.io.File;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * LatencyPlayer Demo Activity
 * <p>
 * 演示RTMP播放器的基本使用方法。
 */
public class MainActivity extends AppCompatActivity {

    private static final String TAG = "LatencyPlayerDemo";

    // 阿里云流列表
    private static final String[] STREAM_URLS = new String[]{
            "rtmp://aliyunjp02.caiyunwang.net/xiaochou/141?auth_key=1782553361-0-0-a1c430ba36349758563f81bb0a126ce5",
            "rtmp://aliyunjp02.caiyunwang.net/leisheng/174?auth_key=1782541771-0-0-d48d080ecff5e8df10a4b71bee134ae8"
    };

    private EditText etUrl;
    private Button btnPlay, btnStop, btnPause, btnMute, btnSnapshot, btnPreconnect;
    private Button btnStream1, btnStream2;
    private LatencyPlayerView playerView;
    private ImageView ivFreeze;
    private TextView tvStatus;

    private LatencyPlayerManager playerManager;   // ACTIVE：正在播的实例
    private LatencyPlayerManager standbyPlayer;   // IDLE：后台预连接待切换流的实例
    private String activeUrl;                     // ACTIVE 当前/最近一次的 URL
    private String standbyUrl;                    // IDLE 预连接中的 URL
    private LatencyPlayerCallback playerCallback; // 两个实例共用一套回调
    private boolean isMuted = false;

    // 冻结帧衔接：切换前抓最后一帧盖住画面，首帧到达后淡出，全程无黑屏
    private volatile boolean hasFirstFrame;
    private boolean startPending;
    private volatile long playTapT0;
    private final ExecutorService freezeExecutor = Executors.newSingleThreadExecutor();

    private interface FreezeCallback {
        void onCaptured(Bitmap bitmap);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initViews();
        initPlayer();
    }

    private void initViews() {
        etUrl = findViewById(R.id.et_url);
        btnPlay = findViewById(R.id.btn_play);
        btnStop = findViewById(R.id.btn_stop);
        btnPause = findViewById(R.id.btn_pause);
        btnMute = findViewById(R.id.btn_mute);
        btnSnapshot = findViewById(R.id.btn_snapshot);
        playerView = findViewById(R.id.player_view);
        ivFreeze = findViewById(R.id.iv_freeze);
        tvStatus = findViewById(R.id.tv_status);
        btnPreconnect = findViewById(R.id.btn_preconnect);

        btnPlay.setOnClickListener(v -> startPlayback());
        btnStop.setOnClickListener(v -> stopPlayback());
        btnPause.setOnClickListener(v -> pauseResume());
        btnMute.setOnClickListener(v -> toggleMute());
        btnSnapshot.setOnClickListener(v -> takeSnapshot());
        btnPreconnect.setOnClickListener(v -> doPreconnect());
        btnStream1 = findViewById(R.id.btn_stream1);
        btnStream2 = findViewById(R.id.btn_stream2);
        btnStream1.setOnClickListener(v -> switchToStream(STREAM_URLS[0]));
        btnStream2.setOnClickListener(v -> switchToStream(STREAM_URLS[1]));
    }

    private void doPreconnect() {
        if (playerManager == null) {
            Toast.makeText(this, "播放器未初始化", Toast.LENGTH_SHORT).show();
            return;
        }
        String url = etUrl.getText().toString().trim();
        if (url.isEmpty()) {
            Toast.makeText(this, "请输入RTMP地址", Toast.LENGTH_SHORT).show();
            return;
        }
        LatencyPlayerManager target = (standbyPlayer != null) ? standbyPlayer : playerManager;
        target.preconnect(url);
        updateStatus("预连接中...");
    }

    /** 平滑秒换：冻结当前最后一帧 → 停旧 → 预连接了目标流的实例秒起
     *  → 新首帧淡入；空出来的实例去后台预连接刚离开的那条流 */
    private void switchToStream(String url) {
        Log.i(TAG, "TIMING switchToStream tap -> " + shortUrl(url)
                + " t=" + android.os.SystemClock.elapsedRealtime());
        if (playerManager == null) {
            Toast.makeText(this, "播放器未初始化", Toast.LENGTH_SHORT).show();
            return;
        }
        if (startPending) return;
        if (url.equals(activeUrl) && playerManager.isPlaying() && hasFirstFrame) {
            updateStatus("正在播放该流");
            return;
        }
        playTapT0 = android.os.SystemClock.elapsedRealtime();
        etUrl.setText(url);

        final LatencyPlayerManager oldM = playerManager;
        // 优先选已预连接目标流的实例（待命位命中则角色互换，native 侧只做 PAUSED->PLAYING）
        final boolean useStandby = standbyPlayer != null && url.equals(standbyUrl);
        final boolean wasPlaying = oldM.isPlaying() && hasFirstFrame;
        final long t0 = playTapT0;

        Runnable doSwitch = () -> {
            if (useStandby) {
                oldM.stop();
                playerManager = standbyPlayer;
                standbyPlayer = oldM;
                standbyUrl = null;
            }
            playerManager.getConfig().setUrl(url);
            playerManager.start();
            activeUrl = url;
            Log.i(TAG, "TIMING switch start t="
                    + (android.os.SystemClock.elapsedRealtime() - t0) + "ms after tap");
            updateStatus("切换中...");
            // 空出来的播放器去预连接另一条流，保证下次切换仍是秒开
            String other = url.equals(STREAM_URLS[0]) ? STREAM_URLS[1] : STREAM_URLS[0];
            if (standbyPlayer != null && !other.equals(standbyUrl)
                    && url.equals(activeUrl)) {
                standbyPlayer.preconnect(other);
                standbyUrl = other;
            }
        };

        if (wasPlaying) {
            startPending = true;
            captureLastFrame(bitmap -> {
                if (!startPending) return; // 期间被停止/再次切换，放弃本次
                if (bitmap != null) showFreeze(bitmap);
                hasFirstFrame = false;
                startPending = false;
                doSwitch.run();
            });
        } else {
            hasFirstFrame = false;
            doSwitch.run();
        }
    }

    private void initPlayer() {
        try {
            playerCallback = createCallback();
            playerManager = createPlayer();
            standbyPlayer = createPlayer();
        } catch (Throwable t) {
            Log.e(TAG, "Player init failed", t);
            updateStatus("初始化失败: " + t.getMessage());
            playerManager = null;
            standbyPlayer = null;
            return;
        }

        // SurfaceView 事件广播给两个播放器实例（视图只允许挂一个监听，
        // 两个实例各自 ANativeWindow_fromSurface 持有同一 Surface 引用）
        playerView.setOnSurfaceReadyListener(new LatencyPlayerView.OnSurfaceReadyListener() {
            @Override
            public void onSurfaceReady(android.view.Surface surface) {
                if (playerManager != null) playerManager.onSurfaceReady(surface);
                if (standbyPlayer != null) standbyPlayer.onSurfaceReady(surface);
            }

            @Override
            public void onSurfaceChanged(int width, int height) {
                if (playerManager != null) playerManager.onSurfaceChanged(width, height);
                if (standbyPlayer != null) standbyPlayer.onSurfaceChanged(width, height);
            }

            @Override
            public void onSurfaceDestroyed() {
                if (playerManager != null) playerManager.onSurfaceDestroyed();
                if (standbyPlayer != null) standbyPlayer.onSurfaceDestroyed();
            }
        });

        // 兜底：表面可能已创建而监听刚挂上，手动补一次绑定
        android.view.Surface existed = playerView.getHolder().getSurface();
        if (existed != null && existed.isValid()) {
            playerManager.onSurfaceReady(existed);
            standbyPlayer.onSurfaceReady(existed);
        }

        // 启动即各自预连接一条流：之后不管点哪条都是秒开
        playerManager.preconnect(STREAM_URLS[0]);
        standbyPlayer.preconnect(STREAM_URLS[1]);

        updateStatus("已初始化（双流预连接中）");
    }

    private LatencyPlayerManager createPlayer() {
        LatencyPlayerManager manager = new LatencyPlayerManager();
        manager.init(getApplicationContext());

        LatencyPlayerConfig config = LatencyPlayerConfig.builder()
                .url(etUrl.getText().toString().trim())
                .bufferTime(0)
                .fastStartup(true)
                .lowLatency(true)
                .hardwareDecoder(true)
                .autoReconnect(true)
                .reconnectInterval(3)
                .maxReconnectCount(0) // 无限重连
                .build();

        manager.setConfig(config);
        manager.setCallback(playerCallback);
        return manager;
    }

    private LatencyPlayerCallback createCallback() {
        return new LatencyPlayerCallback() {
            @Override
            public void onStateChanged(LatencyPlayerState state) {
                Log.d(TAG, "State changed: " + state);
                updateStatus("状态: " + state.name());
            }

            @Override
            public void onError(int errorCode, String message) {
                Log.e(TAG, "Error: " + message);
                // 出错/重连期间保留冻结画面（优于黑屏），下一次首帧回调再撤掉
                updateStatus("错误: " + LatencyPlayerError.getDescription(errorCode));
                Toast.makeText(MainActivity.this, "播放错误: " + message, Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onBuffering(int percent) {
                if (percent >= 100) {
                    updateStatus("缓冲完成");
                } else if (percent > 0) {
                    updateStatus("缓冲中: " + percent + "%");
                }
            }

            @Override
            public void onConnected() {
                Log.d(TAG, "Connected");
                updateStatus("已连接");
            }

            @Override
            public void onDisconnected() {
                Log.d(TAG, "Disconnected");
                updateStatus("已断开");
            }

            @Override
            public void onFirstFrameRendered() {
                long elapsed = playTapT0 > 0 ? (android.os.SystemClock.elapsedRealtime() - playTapT0) : -1;
                Log.i(TAG, "TIMING first-frame t=" + elapsed + "ms after tap");
                hasFirstFrame = true;
                updateStatus("播放中");
                dismissFreeze();
            }

            @Override
            public void onDownloadSpeed(long bytesPerSec) {
                String speed = formatSpeed(bytesPerSec);
                updateStatus("下载速度: " + speed);
            }

            @Override
            public void onReconnecting(int attemptCount) {
                updateStatus("重连中... 第" + attemptCount + "次");
            }

            @Override
            public void onVideoSizeChanged(int width, int height) {
                Log.d(TAG, "Video size: " + width + "x" + height);
                updateStatus("视频: " + width + "x" + height);
            }

            @Override
            public void onPlaybackCompleted() {
                updateStatus("播放完成");
            }
        };
    }

    private void startPlayback() {
        if (playerManager == null) {
            Toast.makeText(this, "播放器未初始化", Toast.LENGTH_SHORT).show();
            return;
        }
        String url = etUrl.getText().toString().trim();
        if (url.isEmpty()) {
            Toast.makeText(this, "请输入RTMP地址", Toast.LENGTH_SHORT).show();
            return;
        }
        if (startPending) return;

        final long t0 = android.os.SystemClock.elapsedRealtime();
        playTapT0 = t0;
        Log.i(TAG, "TIMING tap->startPlayback t=" + t0);
        playerManager.getConfig().setUrl(url);

        if (playerManager.isPlaying() && hasFirstFrame) {
            // 切换：先在后台抓取当前最后一帧并盖住画面，再拆旧管线，
            // 这样拆旧/连新/等关键帧期间用户看到的是定格画面而不是黑屏
            startPending = true;
            captureLastFrame(bitmap -> {
                if (!startPending) return; // 期间被停止，放弃本次切换
                if (bitmap != null) showFreeze(bitmap);
                hasFirstFrame = false;
                startPending = false;
                playerManager.start();
                Log.i(TAG, "TIMING switch start t=" + (android.os.SystemClock.elapsedRealtime() - t0) + "ms after tap");
                updateStatus("正在播放...");
            });
        } else {
            hasFirstFrame = false;
            playerManager.start();
            Log.i(TAG, "TIMING start() returned t=" + (android.os.SystemClock.elapsedRealtime() - t0) + "ms after tap");
            updateStatus("正在播放...");
        }
    }

    private void stopPlayback() {
        if (playerManager == null) return;
        startPending = false; // 取消还在等待抓帧的切换
        playerManager.stop();
        if (standbyPlayer != null) standbyPlayer.stop();
        activeUrl = null;
        standbyUrl = null;
        hideFreezeNow();
        updateStatus("已停止");
    }

    /** 后台抓取播放器当前最后一帧（appsink last-sample → PNG）。 */
    private void captureLastFrame(FreezeCallback callback) {
        freezeExecutor.execute(() -> {
            Bitmap bitmap = null;
            try {
                File file = new File(getCacheDir(),
                        "freeze_" + System.currentTimeMillis() + ".png");
                if (playerManager.saveSnapshot(file.getAbsolutePath())) {
                    bitmap = BitmapFactory.decodeFile(file.getAbsolutePath());
                }
                //noinspection ResultOfMethodCallIgnored
                file.delete();
            } catch (Throwable t) {
                Log.e(TAG, "Capture last frame failed", t);
            }
            final Bitmap frame = bitmap;
            runOnUiThread(() -> callback.onCaptured(frame));
        });
    }

    private void showFreeze(Bitmap frame) {
        ivFreeze.setAlpha(1f);
        ivFreeze.setImageBitmap(frame);
        ivFreeze.setVisibility(View.VISIBLE);
    }

    /** 新流首帧到达：120ms 淡出冻结帧，露出新画面。 */
    private void dismissFreeze() {
        if (ivFreeze.getVisibility() != View.VISIBLE) return;
        ivFreeze.animate().alpha(0f).setDuration(120).withEndAction(() -> {
            ivFreeze.setVisibility(View.GONE);
            ivFreeze.setImageBitmap(null);
            ivFreeze.setAlpha(1f);
        }).start();
    }

    private void hideFreezeNow() {
        ivFreeze.animate().cancel();
        ivFreeze.setVisibility(View.GONE);
        ivFreeze.setImageBitmap(null);
        ivFreeze.setAlpha(1f);
    }

    private void pauseResume() {
        if (playerManager == null) return;
        if (playerManager.isPlaying()) {
            playerManager.pause();
            btnPause.setText("恢复");
        } else {
            playerManager.resume();
            btnPause.setText("暂停");
        }
    }

    private void toggleMute() {
        if (playerManager == null) return;
        isMuted = !isMuted;
        playerManager.setMute(isMuted);
        if (standbyPlayer != null) standbyPlayer.setMute(isMuted);
        btnMute.setText(isMuted ? "取消静音" : "静音");
    }

    private void takeSnapshot() {
        if (playerManager == null) return;
        java.io.File dir = getExternalFilesDir(null);
        if (dir == null) dir = getFilesDir();
        String path = new java.io.File(dir,
                "snap_" + System.currentTimeMillis() + ".png").getAbsolutePath();
        boolean ok;
        try {
            ok = playerManager.saveSnapshot(path);
        } catch (Throwable t) {
            Log.e(TAG, "Snapshot failed", t);
            ok = false;
        }
        Log.i(TAG, "Snapshot " + (ok ? "saved to " : "FAILED: ") + path);
        Toast.makeText(this, (ok ? "已保存: " : "截图失败: ") + path,
                Toast.LENGTH_SHORT).show();
        if (ok) updateStatus("截图: " + path);
    }

    private void updateStatus(String status) {
        runOnUiThread(() -> tvStatus.setText(status));
    }

    private String shortUrl(String url) {
        int q = url.indexOf('?');
        if (q > 0) url = url.substring(0, q);
        String[] seg = url.split("/");
        return seg.length >= 2 ? seg[seg.length - 2] + "/" + seg[seg.length - 1] : url;
    }

    private String formatSpeed(long bytesPerSec) {
        if (bytesPerSec < 1024) {
            return bytesPerSec + " B/s";
        } else if (bytesPerSec < 1024 * 1024) {
            return String.format("%.1f KB/s", bytesPerSec / 1024.0);
        } else {
            return String.format("%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        freezeExecutor.shutdownNow();
        if (playerManager != null) {
            try {
                playerManager.release();
            } catch (Throwable ignored) {
            }
        }
        if (standbyPlayer != null) {
            try {
                standbyPlayer.release();
            } catch (Throwable ignored) {
            }
        }
    }
}
