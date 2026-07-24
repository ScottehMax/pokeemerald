package org.pokeemerald.pc;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;

public final class CoreService extends Service {
    public static final String EXTRA_SHARED_PATH = "sharedPath";
    private final IBinder binder = new Binder();
    private Thread coreThread;

    static {
        System.loadLibrary("main");
    }

    private static native int nativeRunCore(String corePath, String sharedPath);

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (coreThread != null && coreThread.isAlive()) {
            return START_NOT_STICKY;
        }
        String sharedPath = intent == null ? null : intent.getStringExtra(EXTRA_SHARED_PATH);
        if (sharedPath == null) {
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        String corePath = getApplicationInfo().nativeLibraryDir + "/libpokeemerald_core.so";
        coreThread = new Thread(() -> {
            Process.setThreadPriority(Process.THREAD_PRIORITY_DISPLAY);
            nativeRunCore(corePath, sharedPath);
            Process.killProcess(Process.myPid());
        }, "pokeemerald-core-loader");
        coreThread.start();
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }
}
