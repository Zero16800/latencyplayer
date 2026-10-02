package com.latencyplayer.demo;

import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.latencyplayer.sdk.LatencyPlayerCallback;
import com.latencyplayer.sdk.LatencyPlayerConfig;
import com.latencyplayer.sdk.LatencyPlayerError;
import com.latencyplayer.sdk.LatencyPlayerManager;
import com.latencyplayer.sdk.LatencyPlayerState;
import com.latencyplayer.sdk.LatencyPlayerView;

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
    private TextView tvStatus;

    private LatencyPlayerManager playerManager;
    private boolean isMuted = false;

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
                updateStatus("播放中");
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

        playerManager.getConfig().setUrl(url);
        playerManager.start();
        updateStatus("正在播放...");
    }

    private void stopPlayback() {
        if (playerManager == null) return;
        playerManager.stop();
        updateStatus("已停止");
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
        if (playerManager != null) {
            try {
                playerManager.release();
            } catch (Throwable ignored) {
            }
        }
    }
}
