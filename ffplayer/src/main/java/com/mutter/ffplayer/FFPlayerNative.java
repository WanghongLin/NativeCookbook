package com.mutter.ffplayer;

import android.graphics.Bitmap;
import android.os.Handler;

import java.nio.ByteBuffer;

/**
 * Created by mutter on 8/16/16.
 */
public class FFPlayerNative {

    static {
        System.loadLibrary("ffplayer");
    }
    public static native void nativeInitPlayer(Handler playerHandler,
                                               Bitmap pictureBitmap, Handler pictureHandler, int width, int height,
                                               ByteBuffer soundBuffer, Handler soundHandler, int channel, int sampleRate);

    public static native void nativeStartPlayer(String dataSource);
    public static native void nativePausePlayer();
    public static native void nativeResumePlayer();
    public static native void nativeStopPlayer();
    public static native void nativeReleasePlayer();
}
