package com.mutter.ffaudioplayer;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import java.io.ByteArrayOutputStream;
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

    public interface CoverCallback {
        /**
         * Called when cover found in audio file
         * @param bitmap the cover bitmap
         */
        void onCoverRetrieved(Bitmap bitmap);
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
    private static final int PLAYER_EVENT_OUTPUT_FORMAT_CHANGED = 6;
    private static final int PLAYER_EVENT_ON_COVER_RETRIEVED = 7;

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
                case PLAYER_EVENT_OUTPUT_FORMAT_CHANGED:
                    handleOutputFormatChanged(msg);
                    break;
                case PLAYER_EVENT_ON_COVER_RETRIEVED:
                    handleOnCoverRetrieved(msg);
                    break;
                default:
                    break;
            }
            return true;
        }
    };

    private void handleOnCoverRetrieved(Message msg) {
        Log.d(TAG, "handleOnCoverRetrieved: " + msg.arg1 + "x" + msg.arg2);
        if (coverCallback != null) {
            if (msg.obj instanceof byte[]) {
                byte[] bytes = ((byte[]) msg.obj);
                YuvImage yuvImage = new YuvImage(bytes, ImageFormat.NV21, msg.arg1, msg.arg2, null);
                ByteArrayOutputStream jpegOutputStream = new ByteArrayOutputStream();
                if (yuvImage.compressToJpeg(new Rect(0, 0, msg.arg1, msg.arg2), 100, jpegOutputStream)) {
                    byte[] jpegBytes = jpegOutputStream.toByteArray();
                    Bitmap bitmap = BitmapFactory.decodeByteArray(jpegBytes, 0, jpegBytes.length);
                    coverCallback.onCoverRetrieved(bitmap);
                }
            }
        }
    }

    private void handleOutputFormatChanged(Message msg) {
        sampleRate = msg.arg1;
        if (outputMode == OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY ||
                outputMode == OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER) {
            prepare();
            startAudioTrack();
        }
    }

    private void startAudioTrack() {
        if (outputMode == OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER ||
                outputMode == OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY) {
            if (audioTrack != null && audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
                if (audioTrack.getPlayState() != AudioTrack.PLAYSTATE_PLAYING) {
                    audioTrack.play();
                } else {
                    Log.w(TAG, "startAudioTrack: already in playing state");
                }
            } else {
                throw new IllegalStateException("Can not start AudioTrack");
            }
        }
    }

    private void setPlayerState(PlayerState playerState) {
        this.playerState = playerState;
    }

    private void handleAudioDataAvailable(Message message) {
        if (outputMode == OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER) {
            if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING
                    && outputByteBuffer != null) {
                short[] audioData = new short[message.arg1 / 2];
                outputByteBuffer.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(audioData);
                Log.d(TAG, "audio track write data " + audioTrack.write(audioData, 0, audioData.length));
                outputByteBuffer.rewind();
            }
        } else if (outputMode == OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY) {
            if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
                if (message.obj instanceof byte[]) {
                    byte[] bytes = (byte[]) message.obj;
                    ByteBuffer byteBuffer = ByteBuffer.allocate(bytes.length);
                    byteBuffer.put(bytes).rewind();
                    short[] audioData = new short[bytes.length / 2];
                    byteBuffer.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(audioData);
                    Log.d(TAG, "audio track write data " + audioTrack.write(audioData, 0, audioData.length));
                }
            }
        }
    }

    private final Handler outputHandler = new Handler(callback);

    private int sampleRate;
    private AudioTrack audioTrack;

    private ByteBuffer outputByteBuffer;
    private int outputMode;
    private int bufferSize;

    private CoverCallback coverCallback;

    public FFAudioPlayer() {
        sampleRate = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);

        nativeInit(1, sampleRate);
        nativeSetHandler(outputHandler);
        nativeSetOutputMode(OUTPUT_MODE_OPEN_SL_ES);
    }

    private void prepareAudioTrack() {
        if (audioTrack != null) {
            audioTrack.release();
        }
        bufferSize = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_OUT_MONO,
                AudioFormat.ENCODING_PCM_16BIT);

        audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC, sampleRate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);

        if (audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
            throw new RuntimeException("Can not initialize AudioTrack");
        }
    }

    public void setOutputMode(int outputMode) {
        this.outputMode = outputMode;
        nativeSetOutputMode(outputMode);
    }

    public void setDataSource(String dataSource) {
        nativeSetDataSource(dataSource);
    }

    public void prepare() {
        switch (outputMode) {
            case OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY:
                prepareAudioTrack();
                break;
            case OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER:
                prepareAudioTrack();
                outputByteBuffer = ByteBuffer.allocateDirect(bufferSize);
                nativeSetOutputSink(outputByteBuffer);
                break;
            case OUTPUT_MODE_OPEN_SL_ES:
                break;
            default:
                break;
        }
    }

    public void start() {
        startAudioTrack();
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

    public void setCoverCallback(CoverCallback coverCallback) {
        this.coverCallback = coverCallback;
    }
}
