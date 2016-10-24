package com.mutter.ffaudioplayer;

import android.os.Handler;

import java.nio.ByteBuffer;

/**
 * Created by mutter on 8/13/16.
 */
public class FFAudioNative {

    static {
        System.loadLibrary("ffaudioplayer");
    }

    public static final int CHANNELS = 1;
    public native static void nativeInitAudioDecoder(int numberOfChannels, int sampleRate);
    public native static void nativeFinalizeAudioDecoder();
    public native static void nativeSetAudioSource(String audioPath, ByteBuffer outBuffer, Handler dataAvailableHandler);
    public native static void nativeStartPlay();
}
