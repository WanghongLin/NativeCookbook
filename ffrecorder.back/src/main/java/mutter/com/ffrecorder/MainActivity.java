package mutter.com.ffrecorder;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.PermissionChecker;
import android.support.v7.app.AppCompatActivity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import java.io.File;
import java.io.IOException;

import mutter.com.ffmpeg.R;

public class MainActivity extends AppCompatActivity implements
        SurfaceHolder.Callback, Camera.PreviewCallback, View.OnClickListener {

    private static final String TAG = MainActivity.class.getSimpleName();
    private SurfaceView surfaceView;
    private Camera camera;
    private Button button;
    private boolean isRecording = false;
    private static final String OUT_DIR =
            Environment.getExternalStorageDirectory().getAbsolutePath() + File.separator + "ffmpeg";

    private static final String OUT_PATH = OUT_DIR + File.separator + "o.mp4";

    private static final int PREVIEW_WIDTH = 1280;
    private static final int PREVIEW_HEIGHT = 720;

    private FFmpegRecorder ffmpegRecorder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != PermissionChecker.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[] {
                    Manifest.permission.RECORD_AUDIO,
                    Manifest.permission.CAMERA,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
            }, 0);
        }

        surfaceView = (SurfaceView) findViewById(R.id.surface_view);
        button = (Button) findViewById(R.id.button);
        button.setOnClickListener(this);
        Button previewButton = (Button) findViewById(R.id.button_audio);
        previewButton.setOnClickListener(this);

        ViewGroup.LayoutParams layoutParams = surfaceView.getLayoutParams();
        layoutParams.width = PREVIEW_HEIGHT;
        layoutParams.height = PREVIEW_WIDTH;
        surfaceView.setLayoutParams(layoutParams);

        ffmpegRecorder = new FFmpegRecorder(layoutParams.height, layoutParams.width, OUT_PATH, 0);
        surfaceView.getHolder().addCallback(this);
        surfaceView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        openCamera();
    }

    private void openCamera() {
        camera = Camera.open();
        try {
            Camera.Parameters parameters = camera.getParameters();
            parameters.setPreviewSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
            parameters.setPreviewFpsRange(28000, 28000);
            camera.setParameters(parameters);
            camera.setPreviewCallback(this);
            camera.setPreviewDisplay(surfaceView.getHolder());
            setCameraDisplayOrientation(this, 0, camera);
            camera.startPreview();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        closeCamera();
    }

    private void closeCamera() {
        if (camera != null) {
            camera.setPreviewCallback(null);
            camera.stopPreview();
            camera.release();
            camera = null;
        }
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (ffmpegRecorder != null) {
            ffmpegRecorder.record(data);
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.button:
                if (isRecording) {
                    stopFFmpegRecorder();
                } else {
                    startFFmpegRecorder();
                }
                break;
            case R.id.button_audio:
                startActivity(new Intent(this, PreviewActivity.class));
                break;
        }
    }

    public static void setCameraDisplayOrientation(Activity activity,
                                                   int cameraId, android.hardware.Camera camera) {
        android.hardware.Camera.CameraInfo info =
                new android.hardware.Camera.CameraInfo();
        android.hardware.Camera.getCameraInfo(cameraId, info);
        int rotation = activity.getWindowManager().getDefaultDisplay()
                .getRotation();
        int degrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0: degrees = 0; break;
            case Surface.ROTATION_90: degrees = 90; break;
            case Surface.ROTATION_180: degrees = 180; break;
            case Surface.ROTATION_270: degrees = 270; break;
        }

        int result;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + degrees) % 360;
            result = (360 - result) % 360;  // compensate the mirror
        } else {  // back-facing
            result = (info.orientation - degrees + 360) % 360;
        }
        camera.setDisplayOrientation(result);
    }

    private void startFFmpegRecorder() {
        isRecording = true;
        ffmpegRecorder.setOrientationHint(CameraUtils.getRotation(this, 0));
        ffmpegRecorder.start();
        button.setText(R.string.stop);
    }

    private void stopFFmpegRecorder() {
        isRecording = false;
        ffmpegRecorder.stop();
        button.setText(R.string.record);
    }

}
