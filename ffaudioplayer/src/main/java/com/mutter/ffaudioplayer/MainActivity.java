package com.mutter.ffaudioplayer;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.ImageButton;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();
    private AudioTrack audioTrack;
    private AudioPlayerHandler audioPlayerHandler;
    private ByteBuffer outputByteBuffer;
    private static String AUDIO_PATH;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        ImageButton startButton = (ImageButton) findViewById(R.id.audio_play_control_start);
        assert startButton != null;
        startButton.setOnClickListener(this);
        ImageButton stopButton = (ImageButton) findViewById(R.id.audio_play_control_stop);
        assert stopButton != null;
        stopButton.setOnClickListener(this);

        extractAssetResources();
    }

    private void extractAssetResources() {
        new AsyncTask<Void, Void, Void>() {

            @Override
            protected Void doInBackground(Void... params) {
                assets2File("test.mp3");
                return null;
            }
        }.execute();
    }

    private void assets2File(String assetName) {
        InputStream inputStream = null;
        FileOutputStream outputStream = null;
        try {
            inputStream = getAssets().open(assetName);
            File audioFile = new File(getFilesDir().getAbsolutePath() + File.separator + assetName);
            AUDIO_PATH = audioFile.getAbsolutePath();
            outputStream = new FileOutputStream(audioFile);

            byte[] buffer = new byte[8192];
            int n;
            while ((n = inputStream.read(buffer)) > 0) {
                outputStream.write(buffer, 0, n);
            }

            Log.i(TAG, "assets2File: " + assetName + " ---> " + AUDIO_PATH);
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.audio_play_control_start:
                startPlay();
                break;
            case R.id.audio_play_control_stop:
                stopPlay();
                break;
            default:
                break;
        }
    }

    private void stopPlay() {
        if (audioTrack != null) {
            audioTrack.stop();
            audioTrack = null;
        }
    }

    private void startPlay() {

        final int sampleRate = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);
        final int bufferSize = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);

        audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC, sampleRate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);
        outputByteBuffer = ByteBuffer.allocateDirect(bufferSize);

        audioPlayerHandler = new AudioPlayerHandler(new WeakReference<>(this));
        if (audioTrack.getState() == AudioTrack.STATE_INITIALIZED) {
            Log.d(TAG, "startPlay: buffer size " + bufferSize);
            FFAudioNative.nativeInitAudioDecoder(FFAudioNative.CHANNELS, sampleRate);
            FFAudioNative.nativeSetAudioSource(AUDIO_PATH, outputByteBuffer, audioPlayerHandler);
            new Thread() {
                @Override
                public void run() {
                    super.run();
                    FFAudioNative.nativeStartPlay();
                }
            }.start();
        }
    }

    private static class AudioPlayerHandler extends Handler {

        private final WeakReference<MainActivity> ACTIVITY_WEAK_REFERENCE;

        public AudioPlayerHandler(WeakReference<MainActivity> ACTIVITY_WEAK_REFERENCE) {
            this.ACTIVITY_WEAK_REFERENCE = ACTIVITY_WEAK_REFERENCE;
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (ACTIVITY_WEAK_REFERENCE.get() != null) {
                ACTIVITY_WEAK_REFERENCE.get().handleAudioPlayEvent(msg.what, msg.arg1, msg.arg2);
            }
        }
    }

    private void handleAudioPlayEvent(int what, int arg1, int arg2) {
        switch (what) {
            case 0:
                Log.d(TAG, "handleAudioPlayEvent: start play");
                audioTrack.play();
                break;
            case 1:
                if (audioTrack != null && outputByteBuffer != null) {
                    short[] audioData = new short[arg1 / 2];
                    outputByteBuffer.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(audioData);
                    Log.d(TAG, "handleAudioPlayEvent: write sample " + audioTrack.write(audioData, 0, audioData.length));
                }
                break;
            case 2:
                Log.d(TAG, "handleAudioPlayEvent: stop play");
                stopPlay();
                break;
            default:
                break;
        }
    }
}
