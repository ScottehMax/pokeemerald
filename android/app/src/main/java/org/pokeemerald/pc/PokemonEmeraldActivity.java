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
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
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
    private static native void nativeSetTouchControlsEnabled(boolean enabled);

    private boolean coreBindingRequested;
    private String gameDataPath;
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
        gameDataPath = GameDataSettings.prepare(this).getAbsolutePath();
        super.onCreate(savedInstanceState);
        addTopButtons();
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

    public String getGameDataPath() {
        return gameDataPath;
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

    private void addTopButtons() {
        int size = dp(42);
        boolean touchControlsEnabled = getSharedPreferences("controls", MODE_PRIVATE)
            .getBoolean("touch_controls_enabled", true);
        LinearLayout buttons = new LinearLayout(this);
        ImageButton settingsButton = createTopButton(size, R.drawable.ic_link_settings);
        ImageButton controlsButton = createTopButton(size, R.drawable.ic_touch_buttons);
        FrameLayout.LayoutParams layout = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            size,
            Gravity.TOP | Gravity.CENTER_HORIZONTAL);

        buttons.setOrientation(LinearLayout.HORIZONTAL);
        settingsButton.setContentDescription(getString(R.string.settings));
        settingsButton.setOnClickListener(view ->
            startActivity(new Intent(this, LinkSettingsActivity.class)));
        buttons.addView(settingsButton);
        android.view.View spacer = new android.view.View(this);
        buttons.addView(spacer, new LinearLayout.LayoutParams(dp(32), size));
        updateControlsButton(controlsButton, touchControlsEnabled);
        nativeSetTouchControlsEnabled(touchControlsEnabled);
        controlsButton.setOnClickListener(view -> {
            boolean enabled = !getSharedPreferences("controls", MODE_PRIVATE)
                .getBoolean("touch_controls_enabled", true);

            getSharedPreferences("controls", MODE_PRIVATE)
                .edit()
                .putBoolean("touch_controls_enabled", enabled)
                .apply();
            nativeSetTouchControlsEnabled(enabled);
            updateControlsButton(controlsButton, enabled);
        });
        buttons.addView(controlsButton);
        layout.topMargin = dp(8);
        addContentView(buttons, layout);
    }

    private ImageButton createTopButton(int size, int icon) {
        GradientDrawable background = new GradientDrawable();
        ImageButton button = new ImageButton(this);

        background.setColor(Color.argb(145, 18, 22, 20));
        background.setShape(GradientDrawable.OVAL);
        button.setBackground(background);
        button.setImageResource(icon);
        button.setPadding(dp(10), dp(10), dp(10), dp(10));
        button.setLayoutParams(new LinearLayout.LayoutParams(size, size));
        return button;
    }

    private void updateControlsButton(ImageButton button, boolean enabled) {
        button.setAlpha(enabled ? 1.0f : 0.55f);
        button.setContentDescription(getString(
            enabled ? R.string.hide_touch_controls : R.string.show_touch_controls));
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
