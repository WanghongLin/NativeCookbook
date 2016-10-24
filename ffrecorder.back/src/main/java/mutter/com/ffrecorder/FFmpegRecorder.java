package mutter.com.ffrecorder;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;

import java.nio.ByteBuffer;
import java.nio.ShortBuffer;

/**
 * Created by mutter on 5/24/16.
 */
public class FFmpegRecorder {

    private static final int MESSAGE_TYPE_VIDEO_RECORDING = 0;
    private static final int MESSAGE_TYPE_STOP_RECORDING = 1;
    private static final int MESSAGE_TYPE_START_RECORDING = 2;
    private static final int MESSAGE_TYPE_AUDIO_RECORDING = 3;
    private static final int SAMPLE_RATE = 44100;
    private static final int MINIMAL_BUFFER_SIZE =
            AudioRecord.getMinBufferSize(
                    SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT);

    private int previewWidth;
    private int previewHeight;
    private int orientationHint;
    private String outPath;

    private boolean isRecording = false;
    private HandlerThread recordThread = new HandlerThread("video_record");
    private Handler recordHandler;
    private Handler.Callback recordCallback = new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            switch (msg.what) {
                case MESSAGE_TYPE_START_RECORDING:
                    FFmpegNative.naInitEncoder(previewWidth,
                            previewHeight, outPath, orientationHint);
                    startAudio();
                    break;
                case MESSAGE_TYPE_VIDEO_RECORDING:
                    ByteBuffer buffer = (ByteBuffer) msg.obj;
                    FFmpegNative.naEncodeVideo(buffer);
                    System.gc();
                    break;
                case MESSAGE_TYPE_AUDIO_RECORDING:
                    ShortBuffer shortBuffer = (ShortBuffer) msg.obj;
                    FFmpegNative.naEncodeAudio(shortBuffer.array());
                    break;
                case MESSAGE_TYPE_STOP_RECORDING:
                    recordHandler.removeCallbacksAndMessages(null);
                    FFmpegNative.naFinalizeEncoder(true, true);
                    break;
            }
            return true;
        }
    };

    public FFmpegRecorder(int previewWidth, int previewHeight, String outPath, int orientationHint) {
        this.previewWidth = previewWidth;
        this.previewHeight = previewHeight;
        this.outPath = outPath;
        this.orientationHint = orientationHint;
    }

    public void setOrientationHint(int orientationHint) {
        this.orientationHint = orientationHint;
    }

    public void release() {

    }

    public void stop() {
        isRecording = false;
        stopAudio();
        recordHandler.obtainMessage(MESSAGE_TYPE_STOP_RECORDING)
                .sendToTarget();
    }

    public void start() {
        isRecording = true;
        recordThread.start();
        recordHandler = new Handler(recordThread.getLooper(), recordCallback);
        recordHandler.obtainMessage(MESSAGE_TYPE_START_RECORDING)
                .sendToTarget();
    }

    public void record(byte[] data) {
        if (isRecording && recordHandler != null) {
            recordHandler.obtainMessage(
                    MESSAGE_TYPE_VIDEO_RECORDING,
                    ByteBuffer.allocateDirect(data.length).put(data))
                    .sendToTarget();
        }
    }

    private void startAudio() {
        audioRecord = new AudioRecord(
                MediaRecorder.AudioSource.MIC,
                SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                MINIMAL_BUFFER_SIZE
        );
        if (audioRecord.getState() == AudioRecord.STATE_UNINITIALIZED) {
            throw new RuntimeException("Initial audio record fail");
        }
        audioRecord.startRecording();

        new AudioSampleThread().start();
    }

    private void stopAudio() {
        if (audioRecord != null) {
            audioRecord.stop();
            audioRecord.release();
            audioRecord = null;
        }
    }

    private AudioRecord audioRecord;

    protected class AudioSampleThread extends Thread {

        @Override
        public void run() {
            super.run();
            while (true) {
                if (audioRecord != null && audioRecord.getRecordingState()
                        == AudioRecord.RECORDSTATE_RECORDING) {
                    ShortBuffer shortBuffer = ShortBuffer.allocate(FFmpegNative.AUDIO_CODEC_AAC_FRAME_SIZE);
                    int numOfByte = audioRecord.read(shortBuffer.array(), 0, shortBuffer.capacity());
                    if (numOfByte > 0) {
                        shortBuffer.limit(numOfByte);
                    }
                    recordHandler.obtainMessage(MESSAGE_TYPE_AUDIO_RECORDING, shortBuffer)
                            .sendToTarget();
                } else {
                    break;
                }
            }
        }
    }
}
