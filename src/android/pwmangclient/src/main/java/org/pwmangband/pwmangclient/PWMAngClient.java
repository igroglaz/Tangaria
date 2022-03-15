package org.pwmangband.pwmangclient;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import org.libsdl.app.SDLActivity;

public class PWMAngClient extends SDLActivity {
    /** State of requested permissions. */
    private enum PermissionState {
        UNKNOWN,
        REQUESTED,
        GRANTED,
        DENIED
    }
    /** State of WRITE_EXTERNAL_STORAGE permission. */
    private PermissionState storagePermission = PermissionState.UNKNOWN;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Settings.setEnvVars();
        checkPermission();
        if (!storageAvailable()) finish();
    }

    /** Create directory structure on SD card. */
    private void checkPermission() {
        if (storagePermission == PermissionState.UNKNOWN) {
            String extStorage = Manifest.permission.WRITE_EXTERNAL_STORAGE;
            if (ContextCompat.checkSelfPermission(this, extStorage) ==
                    PackageManager.PERMISSION_GRANTED) {
                storagePermission = PermissionState.GRANTED;
            } else {
                storagePermission = PermissionState.REQUESTED;
                ActivityCompat.requestPermissions(this, new String[]{extStorage}, 0);
            }
        }
        if (storagePermission != PermissionState.GRANTED)
            return;

        Settings.checkInstall(getApplicationContext());
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        if (storagePermission == PermissionState.REQUESTED) {
            if ((results.length > 0) && (results[0] == PackageManager.PERMISSION_GRANTED))
                storagePermission = PermissionState.GRANTED;
            else
                storagePermission = PermissionState.DENIED;
        }
        checkPermission();
    }

    /** Return true if the WRITE_EXTERNAL_STORAGE permission has been granted. */
    private boolean storageAvailable() {
        return storagePermission == PermissionState.GRANTED;
    }
}
