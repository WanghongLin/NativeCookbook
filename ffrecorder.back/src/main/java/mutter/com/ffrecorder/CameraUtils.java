package mutter.com.ffrecorder;

import android.app.Activity;
import android.hardware.Camera;
import android.view.OrientationEventListener;

/**
 * Created by mutter on 5/28/16.
 */
public class CameraUtils {

    public static int getRotation(Activity activity, int cameraId) {
        int orientation = activity.getWindowManager().getDefaultDisplay().getRotation();
        if (orientation == OrientationEventListener.ORIENTATION_UNKNOWN) return 0;
        android.hardware.Camera.CameraInfo info =
                new android.hardware.Camera.CameraInfo();
        android.hardware.Camera.getCameraInfo(cameraId, info);
        orientation = (orientation + 45) / 90 * 90;
        int rotation = 0;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            rotation = (info.orientation - orientation + 360) % 360;
        } else {  // back-facing camera
            rotation = (info.orientation + orientation) % 360;
        }
        return rotation;
    }
}
