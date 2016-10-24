package com.mutter.ffplayer;

import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.SurfaceView;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.Spinner;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();
    private static final String SAMPLE_VIDEO_MP4 = "SampleVideo_360x240_1mb.mp4";
    private static final String SAMPLE_VIDEO_MKV = "SampleVideo_320x240.mkv";
    private static final String SAMPLE_VIDEO_RM = "SampleVideo_320x240.rm";
    private SurfaceView surfaceView;
    private ImageButton previousButton;
    private ImageButton nextButton;
    private ImageButton playPauseButton;
    private Spinner spinner;
    private SeekBar seekBar;
    private FFPlayer ffPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        surfaceView = (SurfaceView) findViewById(R.id.player_surface_view);
        previousButton = (ImageButton) findViewById(R.id.player_controller_previous);
        nextButton = (ImageButton) findViewById(R.id.player_controller_next);
        playPauseButton = (ImageButton) findViewById(R.id.player_controller_play_pause);
        seekBar = (SeekBar) findViewById(R.id.player_controller_seek_bar);
        spinner = (Spinner) findViewById(R.id.player_file_choose_spinner);
        seekBar.setMax(100);
        seekBar.setIndeterminate(false);

        final List<String> assetFileNames = new ArrayList<>();
        assetFileNames.add(getResources().getString(R.string.prompt_select_media_format));
        assetFileNames.add(SAMPLE_VIDEO_MP4);
        assetFileNames.add(SAMPLE_VIDEO_MKV);
        assetFileNames.add(SAMPLE_VIDEO_RM);

        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (position > 0) {
                    new SampleVideoAsyncTask().execute(assetFileNames.get(position));
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });
        spinner.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, assetFileNames));

        assert playPauseButton != null;
        playPauseButton.setOnClickListener(this);

        ffPlayer = FFPlayer.getInstance(surfaceView);
        ffPlayer.setFFPlayerListener(new FFPlayer.FFPlayerListener() {
            @Override
            public void onStart() {
                playPauseButton.setImageResource(android.R.drawable.ic_media_pause);
            }

            @Override
            public void onProgressUpdate(Double progress) {
                seekBar.setProgress((int) (progress * 100));
            }

            @Override
            public void onStop() {
                playPauseButton.setImageResource(android.R.drawable.ic_media_play);
            }

            @Override
            public void onPause() {
                playPauseButton.setImageResource(android.R.drawable.ic_media_play);
            }
        });
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (ffPlayer != null) {
            ffPlayer.release();
        }
    }

    protected class SampleVideoAsyncTask extends AsyncTask<String, Void, String> {

        @Override
        protected String doInBackground(String... params) {
            return extractSampleVideo(params[0]);
        }

        @Override
        protected void onPostExecute(String s) {
            super.onPostExecute(s);
            if (ffPlayer != null) {
                ffPlayer.setDataSource(s);
            }
        }
    }

    private String extractSampleVideo(String assetFileName) {
        String file = getFilesDir().getAbsolutePath() + File.separator + assetFileName;
        FileOutputStream fileOutputStream = null;
        try {
            fileOutputStream = new FileOutputStream(new File(file));

            InputStream inputStream = getAssets().open(assetFileName);
            byte[] buffer = new byte[8192];
            int n;
            while ((n = inputStream.read(buffer)) != -1) {
                fileOutputStream.write(buffer, 0, n);
            }
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (fileOutputStream != null) {
                try {
                    fileOutputStream.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        return file;
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.player_controller_play_pause:
                if (ffPlayer != null) {
                    if (ffPlayer.isPlaying()) {
                        ffPlayer.pause();
                    } else {
                        ffPlayer.start();
                    }
                }
                break;
            case R.id.player_controller_previous:
                break;
            case R.id.player_controller_next:
                break;
            default:
                break;
        }
    }
}
