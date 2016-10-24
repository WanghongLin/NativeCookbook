package mutter.com.ffrecorder;

import java.nio.ByteBuffer;

/**
 * Created by mutter on 5/24/16.
 */
public class FFmpegNative {

    static {
        System.loadLibrary("jniffmpeg");
    }

    public static final int AUDIO_CODEC_AAC_FRAME_SIZE = 1024;
    public native static void naInitEncoder(int width, int height, String outPath, int orientationHint);
    public native static int naEncodeVideo(ByteBuffer data);
    public native static void naEncodeAudio(short[] data);
    public native static void naFinalizeEncoder(boolean isAudioEnd, boolean isVideoEnd);
}
