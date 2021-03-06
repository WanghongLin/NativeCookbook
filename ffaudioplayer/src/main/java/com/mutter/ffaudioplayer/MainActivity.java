package com.mutter.ffaudioplayer;

import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();
    private static String AUDIO_PATH;
    private FFAudioPlayer audioPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final ImageView coverImageView = (ImageView) findViewById(R.id.audio_cover_image);
        ImageButton startButton = (ImageButton) findViewById(R.id.audio_play_control_start);
        assert startButton != null;
        startButton.setOnClickListener(this);
        ImageButton stopButton = (ImageButton) findViewById(R.id.audio_play_control_stop);
        assert stopButton != null;
        stopButton.setOnClickListener(this);
        audioPlayer = new FFAudioPlayer();
        audioPlayer.setCoverCallback(new FFAudioPlayer.CoverCallback() {
            @Override
            public void onCoverRetrieved(Bitmap bitmap) {
                if (coverImageView != null) {
                    coverImageView.setImageBitmap(bitmap);
                }
            }
        });

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
                audioPlayer.setOutputMode(FFAudioPlayer.OUTPUT_MODE_OPEN_SL_ES);
                audioPlayer.setDataSource(AUDIO_PATH);
                audioPlayer.prepare();
                audioPlayer.start();
                break;
            case R.id.audio_play_control_stop:
                break;
            default:
                break;
        }
    }
}
