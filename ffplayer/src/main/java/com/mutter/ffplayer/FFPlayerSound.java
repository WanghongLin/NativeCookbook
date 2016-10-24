package com.mutter.ffplayer;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by mutter on 8/16/16.
 */
public class FFPlayerSound implements Handler.Callback {

    private static final String TAG = "FFPlayerSound";
    private ByteBuffer soundBuffer;
    private Handler soundHandler;
    private int channel;
    private int sampleRate;
    private AudioTrack audioTrack;

    private static final boolean VERBOSE = false;
    private static final int SAMPLE_RATE_IN_HZ = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
    private static final int CHANNEL_CONFIG = AudioFormat.CHANNEL_OUT_MONO;
    private static final int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT;

    public FFPlayerSound(Handler ffplayerHandler) {
        HandlerThread handlerThread = new HandlerThread("ffplayer_sound");
        handlerThread.start();
        soundHandler = new Handler(handlerThread.getLooper(), this);

        int bufferSize = AudioTrack.getMinBufferSize(SAMPLE_RATE_IN_HZ, CHANNEL_CONFIG, AUDIO_FORMAT);
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                SAMPLE_RATE_IN_HZ, CHANNEL_CONFIG, AUDIO_FORMAT, bufferSize,
                AudioTrack.MODE_STREAM);
        soundBuffer = ByteBuffer.allocateDirect(bufferSize);

        channel = 1;
        sampleRate = SAMPLE_RATE_IN_HZ;

        if (audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
            Log.d(TAG, "FFPlayerSound initialize audio player");
            audioTrack.play();
            FFPlayer.onSoundPlayerReady(ffplayerHandler);
        }
    }

    @Override
    public boolean handleMessage(Message msg) {
        switch (msg.what) {
            case FFPlayer.FF_PLAYER_MSG_ON_SOUND_AVAILABLE:
                if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
                    if (msg.obj instanceof byte[]) {
                        byte[] data = (byte[]) msg.obj;
                        if (VERBOSE) Log.d(TAG, "handleMessage: on sound available " + data.length);
                        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(data.length);
                        byteBuffer.put(data).rewind();
                        short[] audioData = new short[byteBuffer.capacity() / 2];
                        byteBuffer.order(ByteOrder.nativeOrder()).asShortBuffer().get(audioData);
                        audioTrack.write(audioData, 0, audioData.length);
                    }
                }
                break;
        }
        return false;
    }

    public ByteBuffer getSoundBuffer() {
        return soundBuffer;
    }

    public Handler getSoundHandler() {
        return soundHandler;
    }

    public int getChannel() {
        return channel;
    }

    public int getSampleRate() {
        return sampleRate;
    }

    public void stop() {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.stop();
        }
    }

    public void start() {
        if (audioTrack != null && audioTrack.getPlayState() != AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.play();
        }
    }
}
