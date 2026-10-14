package com.latencyplayer.sdk;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * RTMP播放器View
 * <p>
 * 封装SurfaceView，提供播放器渲染窗口。
 */
public class LatencyPlayerView extends SurfaceView implements SurfaceHolder.Callback {

    private SurfaceHolder surfaceHolder;
    private OnSurfaceReadyListener listener;

    public LatencyPlayerView(Context context) {
        super(context);
        init();
    }

    public LatencyPlayerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public LatencyPlayerView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setBackgroundColor(0);
        getHolder().setFormat(android.graphics.PixelFormat.RGBA_8888);
        getHolder().addCallback(this);
    }

    /**
     * 设置Surface就绪监听器
     */
    public void setOnSurfaceReadyListener(OnSurfaceReadyListener listener) {
        this.listener = listener;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        this.surfaceHolder = holder;
        if (listener != null) {
            listener.onSurfaceReady(holder.getSurface());
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (listener != null) {
            listener.onSurfaceChanged(width, height);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        this.surfaceHolder = null;
        if (listener != null) {
            listener.onSurfaceDestroyed();
        }
    }

    /**
     * 获取当前SurfaceHolder
     */
    public SurfaceHolder getSurfaceHolder() {
        return surfaceHolder;
    }

    /**
     * Surface就绪监听器
     */
    public interface OnSurfaceReadyListener {
        void onSurfaceReady(android.view.Surface surface);
        void onSurfaceChanged(int width, int height);
        void onSurfaceDestroyed();
    }
}
