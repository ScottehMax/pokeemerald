package org.pokeemerald.pc;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.Surface;
import android.widget.ImageButton;
import android.widget.Toast;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import org.libsdl.app.SDLActivity;

public final class PokemonEmeraldActivity extends SDLActivity {
    private static native void nativeNotifyPaused();
    private static native void nativeNotifyResumed();

    private boolean coreBindingRequested;
    private final ServiceConnection coreConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addLinkSettingsButton();
    }

    @Override
    public void setOrientationBis(int width, int height, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
    }

    public String getGameCorePath() {
        return getApplicationInfo().nativeLibraryDir + "/libpokeemerald_core.so";
    }

    public String getLinkServer() {
        return LinkServerSettings.get(this);
    }

    public synchronized void startGameCore(String sharedPath) {
        Intent intent = new Intent(this, CoreService.class);
        intent.putExtra(CoreService.EXTRA_SHARED_PATH, sharedPath);
        startService(intent);
        if (!coreBindingRequested) {
            coreBindingRequested = bindService(intent,
                                               coreConnection,
                                               Context.BIND_AUTO_CREATE
                                               | Context.BIND_IMPORTANT
                                               | Context.BIND_ADJUST_WITH_ACTIVITY);
        }
    }

    public synchronized void stopGameCore() {
        if (coreBindingRequested) {
            unbindService(coreConnection);
            coreBindingRequested = false;
        }
        stopService(new Intent(this, CoreService.class));
    }

    public void prioritizeFrontendThread() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_DISPLAY);
    }

    public void configureGameSurface() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        runOnUiThread(() -> {
            Surface surface = SDLActivity.getNativeSurface();

            if (surface != null && surface.isValid()) {
                surface.setFrameRate(59.7275f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
            }
        });
    }

    private void addLinkSettingsButton() {
        int size = dp(42);
        GradientDrawable background = new GradientDrawable();
        ImageButton button = new ImageButton(this);
        android.widget.FrameLayout.LayoutParams layout =
            new android.widget.FrameLayout.LayoutParams(
                size,
                size,
                Gravity.TOP | Gravity.CENTER_HORIZONTAL);

        background.setColor(Color.argb(145, 18, 22, 20));
        background.setShape(GradientDrawable.OVAL);
        button.setBackground(background);
        button.setImageResource(R.drawable.ic_link_settings);
        button.setContentDescription(getString(R.string.link_settings));
        button.setPadding(dp(10), dp(10), dp(10), dp(10));
        button.setOnClickListener(view ->
            startActivity(new Intent(this, LinkSettingsActivity.class)));
        layout.topMargin = dp(8);
        addContentView(button, layout);
    }

    private int dp(int value) {
        return (int)TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            value,
            getResources().getDisplayMetrics());
    }

    @Override
    protected void onPause() {
        nativeNotifyPaused();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeNotifyResumed();
    }

    public void shareCrashReport(String path) {
        shareTextReport(path, "pokeemerald-pc crash report", "Share crash report");
    }

    public void sharePerformanceReport(String path) {
        shareTextReport(path, "pokeemerald-pc performance report", "Share performance report");
    }

    private void shareTextReport(String path, String subject, String chooserTitle) {
        runOnUiThread(() -> {
            try {
                String report = readTextFile(new File(path));
                Intent intent = new Intent(Intent.ACTION_SEND);
                intent.setType("text/plain");
                intent.putExtra(Intent.EXTRA_SUBJECT, subject);
                intent.putExtra(Intent.EXTRA_TEXT, report);
                startActivity(Intent.createChooser(intent, chooserTitle));
            } catch (IOException exception) {
                Toast.makeText(this, "Could not open report: " + path, Toast.LENGTH_LONG).show();
            }
        });
    }

    private static String readTextFile(File file) throws IOException {
        try (FileInputStream input = new FileInputStream(file);
             ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            int count;

            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
            return new String(output.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
