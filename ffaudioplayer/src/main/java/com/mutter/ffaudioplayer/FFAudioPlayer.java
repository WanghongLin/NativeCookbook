package com.mutter.ffaudioplayer;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by mutter on 8/13/16.
 */
public class FFAudioPlayer {

    private static final String TAG = FFAudioPlayer.class.getSimpleName();

    static {
        System.loadLibrary("ffaudioplayer");
    }

    private static native void nativeInit(int numOfChannels, int sampleRate);
    private static native void nativeSetDataSource(String dataSource);
    private static native void nativeSetOutputSink(ByteBuffer byteBuffer);
    private static native void nativeSetOutputMode(int mode);
    private static native void nativeSetHandler(Handler handler);
    private static native void nativeStart();
    private static native void nativePause();
    private static native void nativeStop();
    private static native void nativeDestroy();

    public static final int OUTPUT_MODE_OPEN_SL_ES = 0;
    public static final int OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER = 1;
    public static final int OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY = 2;

    private static final int PLAYER_EVENT_ON_AUDIO_DATA_AVAILABLE = 0;
    private static final int PLAYER_EVENT_INIT = 1;
    private static final int PLAYER_EVENT_START = 2;
    private static final int PLAYER_EVENT_PAUSE = 3;
    private static final int PLAYER_EVENT_STOP = 4;
    private static final int PLAYER_EVENT_DESTROY = 5;

    private PlayerState playerState = PlayerState.PLAYER_STATE_UNINITIALIZED;

    public enum PlayerState {
        PLAYER_STATE_UNINITIALIZED,
        PLAYER_STATE_INITIALIZED,
        PLAYER_STATE_PAUSED,
        PLAYER_STATE_STOPPED,
        PLAYER_STATE_PLAYING,
        PLAYER_STATE_DESTROYED
    }

    private final Handler.Callback callback = new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            switch (msg.what) {
                case PLAYER_EVENT_INIT:
                    setPlayerState(PlayerState.PLAYER_STATE_INITIALIZED);
                case PLAYER_EVENT_DESTROY:
                    setPlayerState(PlayerState.PLAYER_STATE_DESTROYED);
                    break;
                case PLAYER_EVENT_START:
                    setPlayerState(PlayerState.PLAYER_STATE_PLAYING);
                    break;
                case PLAYER_EVENT_PAUSE:
                    if (audioTrack != null) {
                        audioTrack.pause();
                    }
                    setPlayerState(PlayerState.PLAYER_STATE_PAUSED);
                    break;
                case PLAYER_EVENT_STOP:
                    if (audioTrack != null) {
                        audioTrack.stop();
                    }
                    setPlayerState(PlayerState.PLAYER_STATE_STOPPED);
                    break;
                case PLAYER_EVENT_ON_AUDIO_DATA_AVAILABLE:
                    handleAudioDataAvailable(msg);
                    setPlayerState(PlayerState.PLAYER_STATE_PLAYING);
                    break;
                default:
                    break;
            }
            return true;
        }
    };

    private void setPlayerState(PlayerState playerState) {
        this.playerState = playerState;
    }

    private void handleAudioDataAvailable(Message message) {
        if (outputMode == OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER) {
            if (audioTrack != null && outputByteBuffer != null) {
                short[] audioData = new short[message.arg1 / 2];
                outputByteBuffer.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(audioData);
                Log.d(TAG, "audio track write data " + audioTrack.write(audioData, 0, audioData.length));
                outputByteBuffer.rewind();
            }
        } else if (outputMode == OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY) {

        }
    }

    private final Handler outputHandler = new Handler(callback);

    private AudioTrack audioTrack;

    private ByteBuffer outputByteBuffer;
    private int outputMode;
    private int bufferSize;

    public FFAudioPlayer() {
        final int sampleRate = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);

        bufferSize = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_OUT_MONO,
                AudioFormat.ENCODING_PCM_16BIT);

        audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC, sampleRate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);

        if (audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
            nativeInit(1, sampleRate);
            nativeSetHandler(outputHandler);
            nativeSetOutputMode(OUTPUT_MODE_OPEN_SL_ES);
        } else {
            Log.e(TAG, "FFAudioPlayer: can not create audio player");
        }
    }

    public void setOutputMode(int outputMode) {
        this.outputMode = outputMode;
        switch (outputMode) {
            case OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY:
                break;
            case OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER:
                outputByteBuffer = ByteBuffer.allocateDirect(bufferSize);
                nativeSetOutputSink(outputByteBuffer);
                break;
            case OUTPUT_MODE_OPEN_SL_ES:
                break;
            default:
                break;
        }
        nativeSetOutputMode(outputMode);
    }

    public void setDataSource(String dataSource) {
        nativeSetDataSource(dataSource);
    }

    public void start() {
        if (outputMode == OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY
                || outputMode == OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER) {
            if (audioTrack != null && audioTrack.getPlayState() != AudioTrack.PLAYSTATE_PLAYING) {
                audioTrack.play();
            }
        }
        nativeStart();
    }

    public void pause() {
        nativePause();
    }

    public void stop() {
        nativeStop();
    }

    public void release() {
        nativeDestroy();
    }

    public boolean isPlaying() {
        return playerState == PlayerState.PLAYER_STATE_PLAYING;
    }
}
