package com.mutter.ffplayer;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Created by mutter on 8/16/16.
 */
public class FFPlayerPicture implements SurfaceHolder.Callback, Handler.Callback {

    private static final String TAG = FFPlayerPicture.class.getSimpleName();
    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private Handler pictureHandler;
    private Bitmap pictureBitmap;
    private int width;
    private int height;
    private Handler ffplayerHandler;
    private static final boolean VERBOSE = false;

    public FFPlayerPicture(SurfaceView surfaceView, Handler ffplayerHandler) {
        this.surfaceView = surfaceView;
        this.ffplayerHandler = ffplayerHandler;
        initPicture();
    }

    private void initPicture() {
        surfaceView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        surfaceView.getHolder().addCallback(this);
        HandlerThread handlerThread = new HandlerThread("ffplayer_picture");
        handlerThread.start();
        pictureHandler = new Handler(handlerThread.getLooper(), this);
        Log.d(TAG, "initPicture");
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        this.surfaceHolder = holder;
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        this.width = width;
        this.height = height;
        pictureBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        FFPlayer.onPicturePlayerReady(ffplayerHandler);
        Log.d(TAG, "surfaceChanged: create bitmap " + width + "x" + height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (pictureBitmap != null && !pictureBitmap.isRecycled()) {
            pictureBitmap.recycle();
            pictureBitmap = null;
        }
    }

    @Override
    public boolean handleMessage(Message msg) {
        switch (msg.what) {
            case FFPlayer.FF_PLAYER_MSG_ON_PICTURE_AVAILABLE:
                if (surfaceHolder != null) {
                    if (VERBOSE) Log.d(TAG, "handleMessage: on picture available");
                    Canvas canvas = surfaceHolder.lockCanvas();
                    canvas.drawBitmap(pictureBitmap, 0, 0, null);
                    surfaceHolder.unlockCanvasAndPost(canvas);
                }
                break;
        }
        return false;
    }

    public Bitmap getPictureBitmap() {
        return pictureBitmap;
    }

    public Handler getPictureHandler() {
        return pictureHandler;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }
}
