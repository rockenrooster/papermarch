package dev.glowtide.game;

import android.app.Activity;
import android.content.Context;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * GLOWTIDE's activity (TECH_SPEC 2.4). SDL runs the native SDL_main() from libmain.so on its own thread; this class only
 * adds arguments from the intent (debuggable builds), immersive full screen over the whole panel, the display mode with
 * the wanted refresh rate, haptics, and a clean process exit when the game quits.
 */
public class GlowtideActivity extends SDLActivity {
    private static final String TAG = "GLOWTIDE";

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    /** adb shell am start -n dev.glowtide.game/.GlowtideActivity --es args '--scenario bench_idle --seconds 30' */
    @Override
    protected String[] getArguments() {
        if ((getApplicationInfo().flags & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) == 0) {
            return new String[0];
        }
        String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        if (args == null) {
            return new String[0];
        }
        args = args.trim();
        return args.isEmpty() ? new String[0] : args.split("\\s+");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        hideSystemBars();
        chooseDisplayMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    @Override
    protected void onDestroy() {
        boolean finishing = isFinishing();
        super.onDestroy();
        // SDL cannot start a second SDL_main() in a process whose first one returned.
        if (finishing) {
            System.exit(0);
        }
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            // Without this the window fits inside the hidden bars and the cutout and renders below native size.
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    /** The mode of the current size with the refresh rate nearest the wanted one (--fps; the highest by default). */
    private void chooseDisplayMode() {
        Display display = Build.VERSION.SDK_INT >= 30 ? getDisplay() : getWindowManager().getDefaultDisplay();
        if (display == null) {
            return;
        }
        float wanted = 0.0f;
        for (String a : getArguments()) {
            if (wanted < 0) {
                try {
                    wanted = Float.parseFloat(a);
                } catch (NumberFormatException e) {
                    wanted = 0.0f;
                }
            }
            if (a.equals("--fps")) {
                wanted = -1.0f;
            }
        }
        if (wanted < 0) {
            wanted = 0.0f;
        }
        Display.Mode current = display.getMode();
        Display.Mode best = current;
        for (Display.Mode m : display.getSupportedModes()) {
            if (m.getPhysicalWidth() != current.getPhysicalWidth() || m.getPhysicalHeight() != current.getPhysicalHeight()) {
                continue;
            }
            if (wanted > 0) {
                if (Math.abs(m.getRefreshRate() - wanted) < Math.abs(best.getRefreshRate() - wanted)) {
                    best = m;
                }
            } else if (m.getRefreshRate() > best.getRefreshRate()) {
                best = m;
            }
        }
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.preferredDisplayModeId = best.getModeId();
        getWindow().setAttributes(params);
        Log.i(TAG, "display: mode " + best.getModeId() + " " + best.getPhysicalWidth() + "x" + best.getPhysicalHeight() + " at "
                + best.getRefreshRate() + " Hz (wanted " + (wanted > 0 ? wanted + " Hz" : "the highest") + ")");
    }

    // The updater (docs/RELEASE_PIPELINE.md): a build with a number reads the latest GitHub release's tag (build-<n>) once a
    // launch, on a thread of its own, and hands n to native code; HOME shows UPDATE while it is higher than its own. Nothing is
    // sent but the request (no account: GitHub allows an address 60 such requests an hour); no answer means no button.
    private static final String LATEST_RELEASE = "https://api.github.com/repos/rockenrooster/glowtide/releases/latest";
    private static final long UPDATE_GAP_MS = 60000;
    private static volatile boolean sUpdateBusy;
    private static volatile long sUpdateAskedAt;

    static void checkForUpdate() {
        final long now = android.os.SystemClock.elapsedRealtime();
        if (sUpdateBusy || (sUpdateAskedAt != 0 && now - sUpdateAskedAt < UPDATE_GAP_MS)) {
            return;
        }
        sUpdateBusy = true;
        sUpdateAskedAt = now;
        new Thread(new Runnable() {
            @Override
            public void run() {
                int build = 0;
                java.net.HttpURLConnection connection = null;
                try {
                    connection = (java.net.HttpURLConnection) new java.net.URL(LATEST_RELEASE).openConnection();
                    connection.setConnectTimeout(8000);
                    connection.setReadTimeout(8000);
                    connection.setRequestProperty("Accept", "application/vnd.github+json");
                    connection.setRequestProperty("User-Agent", "glowtide-updater");
                    if (connection.getResponseCode() == 200) {
                        java.io.InputStream in = connection.getInputStream();
                        java.io.ByteArrayOutputStream body = new java.io.ByteArrayOutputStream();
                        byte[] chunk = new byte[8192];
                        for (int n; (n = in.read(chunk)) > 0 && body.size() < (1 << 20);) {
                            body.write(chunk, 0, n);
                        }
                        in.close();
                        String tag = new org.json.JSONObject(body.toString("UTF-8")).optString("tag_name", "");
                        if (tag.startsWith("build-")) {
                            build = Integer.parseInt(tag.substring(6));
                        }
                    }
                } catch (Exception e) {
                    Log.w(TAG, "update: the latest release could not be read: " + e);
                } finally {
                    if (connection != null) {
                        connection.disconnect();
                    }
                }
                Log.i(TAG, "update: the latest release is build " + build);
                if (build > 0) {
                    try {
                        nativeUpdateFound(build);
                    } catch (Throwable t) {  // (never let the updater take the game down)
                        Log.w(TAG, "update: " + t);
                    }
                }
                sUpdateBusy = false;
            }
        }, "glowtide-update").start();
    }

    static native void nativeUpdateFound(int build);

    /** HOME's UPDATE button (from native code): download the latest release, check it, open Android's own installer. */
    static void startUpdate() {
        final Activity activity = mSingleton;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ApkUpdater.downloadAndInstall(activity, "rockenrooster/glowtide", "glowtide.apk", new ApkUpdater.Listener() {
                    @Override
                    public void onProgress(float fraction) {
                        nativeUpdateProgress(fraction);
                    }

                    @Override
                    public void onFailed(String reason) {
                        Log.w(TAG, "update: not installed: " + reason);
                        nativeUpdateProgress(-1.0f);
                        android.widget.Toast.makeText(activity, "Update not installed: " + reason, android.widget.Toast.LENGTH_LONG).show();
                    }
                });
            }
        });
    }

    /** The download's progress for the button (0..1), or -1 when nothing is downloading. */
    static native void nativeUpdateProgress(float fraction);

    /** Native code asks for the mode of the current size nearest `hz` (the 60 Hz fallback); on the UI thread. */
    static void requestRefreshRate(final float hz) {
        final Activity activity = mSingleton;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Display display = Build.VERSION.SDK_INT >= 30 ? activity.getDisplay() : activity.getWindowManager().getDefaultDisplay();
                if (display == null) {
                    return;
                }
                Display.Mode current = display.getMode();
                Display.Mode best = current;
                for (Display.Mode m : display.getSupportedModes()) {
                    if (m.getPhysicalWidth() == current.getPhysicalWidth() && m.getPhysicalHeight() == current.getPhysicalHeight()
                            && Math.abs(m.getRefreshRate() - hz) < Math.abs(best.getRefreshRate() - hz)) {
                        best = m;
                    }
                }
                WindowManager.LayoutParams params = activity.getWindow().getAttributes();
                params.preferredDisplayModeId = best.getModeId();
                activity.getWindow().setAttributes(params);
                Log.i(TAG, "display: asked for " + hz + " Hz: mode " + best.getModeId() + " at " + best.getRefreshRate() + " Hz");
            }
        });
    }

    /** The panel's current refresh rate (native code: benchmarks and the start-up log). */
    static float refreshRate() {
        Activity activity = mSingleton;
        if (activity == null) {
            return 0.0f;
        }
        Display display = Build.VERSION.SDK_INT >= 30 ? activity.getDisplay() : activity.getWindowManager().getDefaultDisplay();
        return display != null ? display.getRefreshRate() : 0.0f;
    }

    // Haptics (GAME_DESIGN 13): 0 light tick, 1 medium, 2 strong, 3 win, 4 lose, 5 double tick. Effects are built once
    // and posted to the UI thread, so the game thread never makes the cross-process vibrate call itself.
    private static final long[][] TIMINGS = {{8}, {30}, {45}, {30, 60, 30, 60, 90}, {250}, {8, 50, 8}};
    private static final int[][] AMPLITUDES = {{70}, {150}, {255}, {170, 0, 170, 0, 255}, {80}, {90, 0, 90}};
    private static final Runnable[] sHaptics = new Runnable[6];
    private static VibrationEffect[] sEffects;
    private static Vibrator sVibrator;
    private static AudioAttributes sAttributes;
    static {
        for (int i = 0; i < 6; ++i) {
            final int kind = i;
            sHaptics[i] = new Runnable() {
                @Override
                public void run() {
                    playHaptic(kind);
                }
            };
        }
    }

    private static void playHaptic(int kind) {
        if (sEffects == null) {
            Activity activity = mSingleton;
            if (activity == null) {
                return;
            }
            sEffects = new VibrationEffect[6];
            Vibrator vibrator = (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
            if (vibrator == null || !vibrator.hasVibrator()) {
                Log.i(TAG, "haptics: no vibrator");
                return;
            }
            boolean amplitude = vibrator.hasAmplitudeControl();
            for (int i = 0; i < 6; ++i) {
                long[] t = TIMINGS[i];
                if (t.length == 1) {
                    sEffects[i] = VibrationEffect.createOneShot(t[0], amplitude ? AMPLITUDES[i][0] : VibrationEffect.DEFAULT_AMPLITUDE);
                } else {
                    long[] wave = new long[t.length + 1];
                    int[] amps = new int[t.length + 1];
                    for (int k = 0; k < t.length; ++k) {
                        wave[k + 1] = t[k];
                        amps[k + 1] = amplitude ? AMPLITUDES[i][k] : (AMPLITUDES[i][k] > 0 ? 255 : 0);
                    }
                    sEffects[i] = VibrationEffect.createWaveform(wave, amps, -1);
                }
            }
            sAttributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).build();
            sVibrator = vibrator;
            Log.i(TAG, "haptics: vibrator, amplitude control " + (amplitude ? "yes" : "no (durations only)"));
        }
        if (sVibrator != null && sEffects[kind] != null) {
            sVibrator.vibrate(sEffects[kind], sAttributes);
        }
    }

    static void haptic(int kind) {
        View view = mSurface;
        if (view != null && kind >= 0 && kind < sHaptics.length) {
            view.post(sHaptics[kind]);
        }
    }
}
