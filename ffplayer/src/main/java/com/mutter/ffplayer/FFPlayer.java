package com.mutter.ffplayer;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.SurfaceView;

/**
 * Created by mutter on 8/16/16.
 */
public class FFPlayer implements Handler.Callback {

    public interface FFPlayerListener {
        void onStart();
        void onProgressUpdate(Double progress);
        void onStop();
        void onPause();
    }

    public static final int FF_PLAYER_MSG_START = 0;
    public static final int FF_PLAYER_MSG_ON_PICTURE_AVAILABLE = 1;
    public static final int FF_PLAYER_MSG_ON_SOUND_AVAILABLE = 2;
    public static final int FF_PLAYER_MSG_STOP = 3;
    public static final int FF_PLAYER_MSG_PROGRESS_UPDATE = 4;
    private static final int FF_PLAYER_PICTURE_PLAYER_READY = 851;
    private static final int FF_PLAYER_SOUND_PLAYER_READ = 469;
    private static final String TAG = FFPlayer.class.getSimpleName();

    private Handler playerHandler;
    private String dataSource;
    private FFPlayerPicture playerPicture;
    private FFPlayerSound playerSound;
    private FFPlayerListener ffPlayerListener;
    private boolean playing;
    private FFPlayerState ffPlayerState = FFPlayerState.FF_PLAYER_STATE_STOPPED;

    public void setDataSource(String dataSource) {
        this.dataSource = dataSource;
    }

    private FFPlayer(SurfaceView surfaceView) {
        HandlerThread handlerThread = new HandlerThread("ffplayer");
        handlerThread.start();
        this.playerHandler = new Handler(handlerThread.getLooper(), this);

        this.playerPicture = new FFPlayerPicture(surfaceView, playerHandler);
        this.playerSound = new FFPlayerSound(playerHandler);
    }


    public static FFPlayer getInstance(SurfaceView surfaceView) {
        return new FFPlayer(surfaceView);
    }

    @Override
    public boolean handleMessage(Message msg) {
        switch (msg.what) {
            case FFPlayer.FF_PLAYER_MSG_START:
                if (playerSound != null) {
                    playerSound.start();
                }
                setPlaying(true);
                this.ffPlayerState = FFPlayerState.FF_PLAYER_STATE_PLAYING;
                if (ffPlayerListener != null) {
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            ffPlayerListener.onStart();
                        }
                    });
                }
                break;
            case FFPlayer.FF_PLAYER_MSG_STOP:
                if (playerSound != null) {
                    playerSound.stop();
                }
                setPlaying(false);
                this.ffPlayerState = FFPlayerState.FF_PLAYER_STATE_STOPPED;
                if (ffPlayerListener != null) {
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            ffPlayerListener.onStop();
                        }
                    });
                }
                break;
            case FFPlayer.FF_PLAYER_PICTURE_PLAYER_READY:
                FFPlayerNative.nativeInitPlayer(playerHandler,
                        playerPicture.getPictureBitmap(), playerPicture.getPictureHandler(), playerPicture.getWidth(), playerPicture.getHeight(),
                        playerSound.getSoundBuffer(), playerSound.getSoundHandler(), playerSound.getChannel(), playerSound.getSampleRate());
                break;
            case FF_PLAYER_SOUND_PLAYER_READ:
                break;
            case FF_PLAYER_MSG_PROGRESS_UPDATE:
                if (ffPlayerListener != null) {
                    ffPlayerListener.onProgressUpdate((Double) msg.obj);
                }
                break;
            default:
                break;
        }
        return false;
    }

    public void start() {
        if (ffPlayerState == FFPlayerState.FF_PLAYER_STATE_PAUSED) {
            resume();
        } else {
            if (dataSource == null) {
                Log.e(TAG, "start: data source not set");
            } else {
                FFPlayerNative.nativeStartPlayer(dataSource);
            }
        }
    }

    public void setFFPlayerListener(FFPlayerListener ffPlayerListener) {
        this.ffPlayerListener = ffPlayerListener;
    }

    public static void onPicturePlayerReady(Handler handler) {
        handler.obtainMessage(FF_PLAYER_PICTURE_PLAYER_READY).sendToTarget();
    }

    public static void onSoundPlayerReady(Handler handler) {
        handler.obtainMessage(FF_PLAYER_SOUND_PLAYER_READ).sendToTarget();
    }

    public boolean isPlaying() {
        return playing;
    }

    private void setPlaying(boolean playing) {
        this.playing = playing;
    }

    public void pause() {
        setPlaying(false);
        this.ffPlayerState = FFPlayerState.FF_PLAYER_STATE_PAUSED;
        FFPlayerNative.nativePausePlayer();
    }

    private void resume() {
        setPlaying(true);
        this.ffPlayerState = FFPlayerState.FF_PLAYER_STATE_PLAYING;
        FFPlayerNative.nativeResumePlayer();
        if (ffPlayerListener != null) {
            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    ffPlayerListener.onPause();
                }
            });
        }
    }

    public void stop() {
        setPlaying(false);
        this.ffPlayerState = FFPlayerState.FF_PLAYER_STATE_STOPPED;
        FFPlayerNative.nativeStopPlayer();
    }

    public void release() {
        if (isPlaying()) {
            stop();
        }
        FFPlayerNative.nativeReleasePlayer();
    }

}
