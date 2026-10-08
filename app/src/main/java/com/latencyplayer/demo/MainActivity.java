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

    private EditText etUrl;
    private Button btnPlay, btnStop, btnPause, btnMute, btnSnapshot;
    private LatencyPlayerView playerView;
    private ImageView ivFreeze;
    private TextView tvStatus;

    private LatencyPlayerManager playerManager;
    private boolean isMuted = false;

    // 冻结帧衔接：切换前抓最后一帧盖住画面，首帧到达后淡出，全程无黑屏
    private volatile boolean hasFirstFrame;
    private boolean startPending;
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

        btnPlay.setOnClickListener(v -> startPlayback());
        btnStop.setOnClickListener(v -> stopPlayback());
        btnPause.setOnClickListener(v -> pauseResume());
        btnMute.setOnClickListener(v -> toggleMute());
        btnSnapshot.setOnClickListener(v -> takeSnapshot());
    }

    private void initPlayer() {
        try {
            playerManager = new LatencyPlayerManager();
            playerManager.init(getApplicationContext());
        } catch (Throwable t) {
            Log.e(TAG, "Player init failed", t);
            updateStatus("初始化失败: " + t.getMessage());
            playerManager = null;
            return;
        }

        // 配置播放器
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

        playerManager.setConfig(config);

        // 设置播放View
        playerManager.setSurface(playerView);

        // 设置回调
        playerManager.setCallback(new LatencyPlayerCallback() {
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
                Log.d(TAG, "First frame rendered");
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
        });

        updateStatus("已初始化");
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
                updateStatus("正在播放...");
            });
        } else {
            hasFirstFrame = false;
            playerManager.start();
            updateStatus("正在播放...");
        }
    }

    private void stopPlayback() {
        if (playerManager == null) return;
        startPending = false; // 取消还在等待抓帧的切换
        playerManager.stop();
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
    }
}
