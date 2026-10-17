# LatencyPlayer SDK — Maven Integration & Complete Usage Guide

> Coordinate: `com.latencyplayer:latencyplayer-sdk:1.0.13`
> Supports: Maven local repository / Maven remote repository (private Nexus, GitHub Packages, Alibaba Cloud Effect, etc.)

> 中文文档见 [MAVEN_INTEGRATION.md](MAVEN_INTEGRATION.md)。

---

## 1. SDK artifacts and publishing

### 1.1 Publish to a local repository (dev self-test, no network)

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon
```

Output:

```
E:\Android-SDK\maven-repo\com\latencyplayer\latencyplayer-sdk\1.0.13\
    latencyplayer-sdk-1.0.13.aar            (contains 4-ABI .so)
    latencyplayer-sdk-1.0.13-sources.jar
    latencyplayer-sdk-1.0.13.pom            (transitive deps: androidx.annotation / appcompat)
```

### 1.2 Publish to Nexus Repository 3

> Nexus already integrated by this project: `http://192.168.3.160:8970/repository/maven-releases/`
> (anonymous read; publishing uses an account with write permission, credentials passed via `-P`, never stored)
>
> **Steps 1–3 below are the generic repository-setup guidance; this project is already set up**: the
> `maven-releases` repository exists (`maven2 hosted`, `writePolicy=ALLOW_ONCE`); just publish with an
> account that has write permission on Nexus. Your Nexus actually runs on port `8970` (not the default 8081).

#### Step 1: Create a Maven repository on Nexus (done in this project; for reference when changing servers)

1. Log into the Nexus console at `http://<nexus-host>:8081` (default account `admin`)
2. Top-right **Settings → Repositories → Create repository**
3. Choose recipe **maven2 (hosted)** and configure:
   - Name: `latencyplayer-releases` (for releases); for snapshots create a separate `latencyplayer-snapshots` (maven2 hosted, name containing `-snapshots`)
   - Strict Content Type / Component Queries: defaults are fine
   - Deployment policy: `Allow redeploy` (if redeploy is not allowed, a second publish of the same version returns 400; prefer bumping the version)
4. Once created, the repository URL is:
   ```
   http://<nexus-host>:8081/repository/latencyplayer-releases/
   ```

#### Step 2: Create a publishing account (do not use admin)

1. **Security → Users → Create local user**
2. User ID e.g. `latencyplayer-deploy`, set a password
3. Add role **nx-artifact** (`nx-anonymous` is not enough for read-only download; publishing needs write permission; for least privilege you can also create a role with `nx-repository-view-maven2-latencyplayer-releases-add`, `-edit`, `-browse`)

#### Step 3: Gradle configuration

`gradle.properties`:

```properties
# ===== Nexus3 remote repository =====
latencyplayer.maven.url=http://<nexus-host>:8081/repository/latencyplayer-releases/
latencyplayer.maven.user=latencyplayer-deploy
latencyplayer.maven.password=******
LATENCYPLAYER_VERSION=1.0.13
```

> If you do not want plaintext credentials in the repository, use environment variables instead: the
> code's `findProperty` reads `-P` parameters, so you can also run
> `gradlew :sdk:publishReleasePublicationToRemoteRepository -Platencyplayer.maven.url=... -Platencyplayer.maven.user=... -Platencyplayer.maven.password=...`
> or put the credentials in `~/.gradle/gradle.properties` (effective only on this machine, not committed).

#### Step 4: Publish

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon ^
  -Platencyplayer.maven.url=http://192.168.3.160:8970/repository/maven-releases/ ^
  -Platencyplayer.maven.user=admin -Platencyplayer.maven.password=******
```

> **Local proxy pitfall**: `~/.gradle/gradle.properties` sets `http.proxyHost=127.0.0.1:1080`; without
> `nonProxyHosts`, Gradle also sends intranet Nexus requests to the proxy → read timeout / 502.
> `gradle.properties` already adds `systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1` (same for https).
>
> **Large-file direct upload**: Gradle occasionally times out uploading a 29 MB AAR; you can use curl instead (also 201):
> ```bat
> curl -u admin:****** -X PUT --upload-file sdk-release.aar ^
>   http://192.168.3.160:8970/repository/maven-releases/com/latencyplayer/latencyplayer-sdk/1.0.13/latencyplayer-sdk-1.0.13.aar
> ```
>
> **Version overwrite**: `maven-releases` is `ALLOW_ONCE`, so re-publishing the same version returns 400
> `Repository does not allow updating assets` — bump `LATENCYPLAYER_VERSION` and publish again.

Uploaded content: `latencyplayer-sdk-1.0.13.aar` / `.pom` / `.module` / `-sources.jar` plus the various checksum files.

#### Step 5: Verify

Open in a browser (or when anonymous read is allowed):

```
http://<nexus-host>:8081/repository/latencyplayer-releases/com/latencyplayer/latencyplayer-sdk/1.0.13/
```

Seeing `.aar`/`.pom` means success. The **Components** list in Nexus also shows the component.
This project's verification method (already passed): clear `~/.gradle/caches/modules-2/.../com.latencyplayer` + disable the local `maven-repo` + build with `--refresh-dependencies`; success proves it came from Nexus.

Common task names:

| Task | Purpose |
|------|---------|
| `publishReleasePublicationToProjectLocalRepository` | publish to the in-project `maven-repo/` |
| `publishReleasePublicationToRemoteRepository` | publish to Nexus (requires url/user/password) |
| `publish` | publish to both repositories (remote only if configured) |

### 1.3 Version upgrades

Change `gradle.properties`:

```properties
LATENCYPLAYER_VERSION=1.1.0
```

Re-run 1.1 / 1.2 and have consumers change the coordinate version.
(Nexus hosted repositories forbid overwriting the same version by default, so the version must increment; snapshots use a `-SNAPSHOT` suffix + a snapshot repository.)

---

## 2. Complete integrator configuration

### 2.1 settings.gradle.kts (root project)

```kotlin
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()

        // Option A: local repository (same-machine development)
        maven {
            url = uri("file:///E:/Android-SDK/maven-repo")
            metadataSources {
                mavenPom()
                artifact()
            }
        }

        // Option B: Nexus3 remote repository (team/CI; no credentials needed if anonymous read is allowed)
        maven {
            name = "LatencyPlayer"
            url = uri("http://192.168.3.160:8970/repository/maven-releases/")
            isAllowInsecureProtocol = true   // required for http repositories (Gradle 7+ forbids by default)
            // Only needed when the repository is write-protected / not anonymously readable:
            // credentials {
            //     username = findProperty("latencyplayer.maven.user") as String? ?: ""
            //     password = findProperty("latencyplayer.maven.password") as String? ?: ""
            // }
        }
    }
}

rootProject.name = "YourApp"
include(":app")
```

### 2.2 android/app/build.gradle.kts

```kotlin
plugins {
    id("com.android.application")
}

android {
    namespace = "com.example.yourapp"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.yourapp"
        minSdk = 21            // SDK requires >= 21
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}

dependencies {
    // ====== LatencyPlayer SDK (Maven remote/local dependency) ======
    implementation("com.latencyplayer:latencyplayer-sdk:1.0.13")

    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
}
```

> No need to manually copy the AAR, no `jniLibs` configuration: the `.so` is packaged into the AAR and Gradle splits by ABI automatically.
> No app-level CMake/NDK needed: the SDK ships its own native build artifacts.

### 2.3 AndroidManifest.xml (App)

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:theme="@style/Theme.AppCompat.Light.NoActionBar">

        <activity
            android:name=".MainActivity"
            android:exported="true"
            android:configChanges="orientation|screenSize|keyboardHidden">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

    </application>
</manifest>
```

### 2.4 Layout res/layout/activity_main.xml (complete)

```xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    android:padding="16dp">

    <!-- RTMP URL input -->
    <EditText
        android:id="@+id/et_url"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:hint="RTMP URL"
        android:inputType="textUri"
        android:singleLine="true"
        android:text="rtmp://example.com/live/stream" />

    <!-- Control buttons -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="horizontal">

        <Button
            android:id="@+id/btn_play"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="Play" />

        <Button
            android:id="@+id/btn_stop"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="Stop" />

        <Button
            android:id="@+id/btn_pause"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="Pause" />
    </LinearLayout>

    <!-- Player view: do NOT set an opaque background (#000000 causes a black screen) -->
    <com.latencyplayer.sdk.LatencyPlayerView
        android:id="@+id/player_view"
        android:layout_width="match_parent"
        android:layout_height="0dp"
        android:layout_weight="1" />

    <TextView
        android:id="@+id/tv_status"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="Status: not initialized"
        android:textSize="14sp" />

</LinearLayout>
```

### 2.5 MainActivity.java (complete and runnable)

```java
package com.example.yourapp;

import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.latencyplayer.sdk.LatencyPlayerCallback;
import com.latencyplayer.sdk.LatencyPlayerConfig;
import com.latencyplayer.sdk.LatencyPlayerError;
import com.latencyplayer.sdk.LatencyPlayerManager;
import com.latencyplayer.sdk.LatencyPlayerState;
import com.latencyplayer.sdk.LatencyPlayerView;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "PlayerDemo";

    private EditText etUrl;
    private TextView tvStatus;
    private Button btnPause;
    private LatencyPlayerView playerView;

    private LatencyPlayerManager player;
    private boolean muted = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        etUrl = findViewById(R.id.et_url);
        tvStatus = findViewById(R.id.tv_status);
        btnPause = findViewById(R.id.btn_pause);
        playerView = findViewById(R.id.player_view);

        initPlayer();

        findViewById(R.id.btn_play).setOnClickListener(v -> play());
        findViewById(R.id.btn_stop).setOnClickListener(v -> stop());
        btnPause.setOnClickListener(v -> pauseOrResume());
    }

    private void initPlayer() {
        try {
            player = new LatencyPlayerManager();
            player.init(getApplicationContext());
        } catch (Throwable t) {
            Log.e(TAG, "init failed", t);
            status("Init failed: " + t.getMessage());
            player = null;
            return;
        }

        // 1) Config (setConfig is required before start)
        LatencyPlayerConfig config = LatencyPlayerConfig.builder()
                .url(etUrl.getText().toString().trim())
                .bufferTime(0)            // 0 = lowest latency
                .lowLatency(true)         // flatten internal queues
                .fastStartup(true)
                .hardwareDecoder(true)
                .autoReconnect(true)      // auto-reconnect on disconnect
                .reconnectInterval(3)     // every 3 seconds
                .maxReconnectCount(0)     // 0 = unlimited
                .build();
        player.setConfig(config);

        // 2) Bind the rendering View (do not set an opaque background on LatencyPlayerView)
        player.setSurface(playerView);

        // 3) Callbacks (all on the main thread)
        player.setCallback(new LatencyPlayerCallback() {
            @Override
            public void onStateChanged(LatencyPlayerState state) {
                Log.d(TAG, "state=" + state);
                status("State: " + state.name());
            }

            @Override
            public void onError(int errorCode, String message) {
                Log.e(TAG, "error=" + errorCode + " " + message);
                status("Error: " + LatencyPlayerError.getDescription(errorCode));
            }

            @Override
            public void onBuffering(int percent) {
                status(percent >= 100 ? "Buffering complete" : "Buffering: " + percent + "%");
            }

            @Override
            public void onConnected() {
                status("Connected");
            }

            @Override
            public void onDisconnected() {
                status("Disconnected");
            }

            @Override
            public void onFirstFrameRendered() {
                status("Playing");        // first frame rendered = actually on screen
            }

            @Override
            public void onDownloadSpeed(long bytesPerSec) {
                // download speed in bytes/s
            }

            @Override
            public void onReconnecting(int attemptCount) {
                status("Reconnecting... attempt " + attemptCount);
            }

            @Override
            public void onVideoSizeChanged(int width, int height) {
                status("Video: " + width + "x" + height);
            }

            @Override
            public void onPlaybackCompleted() {
                status("Playback completed");
            }
        });

        status("Initialized");
    }

    private void play() {
        if (player == null) return;
        String url = etUrl.getText().toString().trim();
        if (url.isEmpty()) {
            Toast.makeText(this, "Please enter an RTMP URL", Toast.LENGTH_SHORT).show();
            return;
        }
        player.getConfig().setUrl(url);
        player.start();
    }

    private void stop() {
        if (player == null) return;
        player.stop();
        status("Stopped");
    }

    private void pauseOrResume() {
        if (player == null) return;
        if (player.isPlaying()) {
            player.pause();
            btnPause.setText("Resume");
        } else {
            player.resume();
            btnPause.setText("Pause");
        }
    }

    private void status(String s) {
        runOnUiThread(() -> tvStatus.setText(s));
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (player != null && player.isPlaying()) {
            player.pause();     // do not hold the decoder in the background
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        // call player.resume() here when you want automatic resume
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (player != null) {
            try {
                player.release();   // release native resources; must be called
            } catch (Throwable ignored) {
            }
            player = null;
        }
    }
}
```

### 2.6 Obfuscation (android/app/proguard-rules.pro, when minify is enabled)

The SDK AAR does not include a consumer ProGuard file, so add this yourself when enabling code obfuscation:

```proguard
-keep class com.latencyplayer.sdk.** { *; }
-keep class org.freedesktop.gstreamer.** { *; }
-keepclasseswithmembernames class * { native <methods>; }
-keep public interface com.latencyplayer.sdk.LatencyPlayerCallback { *; }
```

---

## 3. API quick reference

```java
LatencyPlayerManager p = new LatencyPlayerManager();
p.init(context);                                  // 1. initialize (Application Context)
p.setConfig(config);                              // 2. configure
p.setSurface(view);                               // 3. bind the picture
p.setCallback(callback);                          // 4. callbacks
p.start();                                        // 5. play

p.pause();  p.resume();  p.stop();                // playback control
p.switchUrl("rtmp://new/live/stream2");           // hot URL switch
p.setMute(true);  p.setVolume(50);                // mute/volume (0-100)
p.setBufferTime(0);                               // change buffering at runtime
p.setFlip(true, false);  p.setRotation(90);       // flip/rotate
p.saveSnapshot("/sdcard/a.png");                  // snapshot (.jpg outputs JPEG; returns boolean)
p.isPlaying();  p.getState();                     // state queries
p.release();                                      // release (must be called in onDestroy)
```

Configuration options (`LatencyPlayerConfig.builder()`):

| Parameter | Default | Description |
|-----------|---------|-------------|
| `url` | null | Playback URL; required before start |
| `bufferTime` | 200 | Buffer ms, 0–5000, 0 = lowest latency |
| `lowLatency` | false | Flatten queue2/multiqueue internal queues |
| `fastStartup` | true | Fast-startup flag |
| `hardwareDecoder` | true | Prefer hardware decode |
| `autoReconnect` | true | Auto-reconnect on disconnect |
| `reconnectInterval` | 3 | Reconnect interval (seconds) |
| `maxReconnectCount` | 0 | 0 = unlimited |
| `orientation` | 1 | 1 = portrait, 2 = landscape |
| `mute` / `volume` | false / 100 | Mute / volume 0–100 |

> After changing the config you must call `setConfig()` again for it to be pushed to native.

---

## 4. Feature list

| Feature | Status | Notes |
|---------|--------|-------|
| RTMP / RTMPS live ingest | ✅ | Main path, rtmpsrc/librtmp |
| appsink direct-draw rendering | ✅ | RGBx/RGBA → ANativeWindow, bypasses EGL |
| Software/hardware decode switch | ✅ | openh264 software decode + MediaCodec path |
| Low-latency mode | ✅ | bufferTime=0 + lowLatency queue flattening |
| Auto-reconnect on disconnect | ✅ | interval/count configurable, 0 = unlimited |
| Background-return self-recovery | ✅ | 1.0.13: auto-reconnect when video stops flowing >3 s after backgrounding/freezing (`getLastFrameAgeMs` stall watchdog) |
| Pause/resume/stop | ✅ | |
| Hot URL switch | ✅ | switchUrl |
| Mute/volume | ✅ | 0–100 |
| Flip/rotate | ✅ | horizontal/vertical flip, 0/90/180/270 |
| Snapshot | ✅ | saveSnapshot(path), PNG/JPEG by suffix (pngenc/jpegenc), returns boolean; verified valid PNG/JPEG on device |
| Download speed/bitrate callback | ✅ | onDownloadSpeed |
| First-frame/video-size callback | ✅ | main-thread callbacks |
| HLS / HTTP-FLV / RTSP | ✅ | 1.0.3 regression passed (RTSP / HTTP-FLV / HLS VOD + HLS live rendered) |
| WebRTC | ❌ | Not supported |

---

## 5. Device & compatibility support

### 5.1 OS and ABI

| Item | Supported range |
|------|-----------------|
| Minimum OS | Android 5.0 (API 21) |
| Target OS | Android 14 (API 34), verified |
| ABI | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64` |
| Primary device | Xiaomi 11 (arm64 / Android 14), rendering verified |
| Emulator | x86_64 runs (glimagesink has compatibility issues on some emulators; the SDK uses appsink direct-draw by default to avoid this) |
| 16 KB page devices | ✅ 64-bit `.so` LOAD segments `0x4000`-aligned, `zipalign -P 16` passes (Android 15+) |
| Old 32-bit devices | ✅ kept 4 KB aligned (16 KB is required only for 64-bit) |
| Foldables/tablets | ✅ Surface size changes are rebound automatically |
| Landscape/portrait | ✅ switching within configChanges does not interrupt the stream |

### 5.2 Verified devices

- Xiaomi 11 (`M2011K2C`, arm64, Android 14, API 34): 720p RTMP live renders, play/stop/reconnect normal

> Other arm64/armv7 devices are theoretically compatible (pure NDK + static GStreamer); after integration, regression-test first-frame and reconnect on your target devices.

### 5.3 Size (of interest to integrators)

| Artifact | Size |
|----------|------|
| AAR (4 ABIs) | 1.0.0: 29.4 MB / 1.0.1: 34.1 MB / 1.0.2: 58.2 MB / 1.0.3: 59.6 MB / 1.0.4–1.0.13: 59.6 MB |
| arm64 so (stripped) | 1.0.0: 17.6 MB / 1.0.1: 20.3 MB / 1.0.2: 32.3 MB / 1.0.3: 33.1 MB / 1.0.4–1.0.13: 33.1 MB |
| Integrator APK delta | split by the chosen ABI; single arm64 ≈ +33.1 MB (on par since 1.0.4 through 1.0.13; +32.3 MB in 1.0.2) |

---

## 6. FAQ

| Symptom | Resolution |
|---------|------------|
| `Could not find com.latencyplayer:latencyplayer-sdk:1.0.13` | Confirm the consumer's `settings.gradle.kts` adds the repository; for a local repository first run `:sdk:publish...ProjectLocalRepository` |
| Pulling from Nexus `Read timed out` (curl is instant) | The global proxy does not bypass the intranet: add `systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1` to `gradle.properties` |
| Publish returns 400 `does not allow updating assets` | Nexus `ALLOW_ONCE` forbids overwriting the same version: bump `LATENCYPLAYER_VERSION` and publish again |
| Publishing a 29 MB AAR times out / 502 | Gradle occasionally times out on large files; use curl direct upload (see 1.2 Step 4) |
| Black screen but state PLAYING | `LatencyPlayerView` must not have an opaque background (`#000000`) |
| Crash on rapid stop→play | **Fixed in 1.0.7** (rtmpsrc unlock race; real-device stress 120 rounds, 0 crashes); the historical workaround was to wait for the state to return to `STOPPED` before start |
| Crash on repeated play taps | Fixed (1.0.0 includes the `gst_object_ref_sink` fix) |
| Stays ~2.5 s behind the source after startup (CDN burst) | **Fixed in 1.0.13**: appsink `sync=FALSE` + catch-up dropping, `pipe_backlog` dropped to 40–200 ms |
| Black screen after backgrounding / being frozen and returning | **Fixed in 1.0.13**: auto-reconnect when video stops flowing >3 s (`getLastFrameAgeMs`), ~750 ms to render after a 22 s background |
| Crash on initialization | Check whether the app's `abiFilters` filtered out the SDK's `.so` |
| Slow first frame | `bufferTime(0)` + `lowLatency(true)` + `fastStartup(true)` |
| Config change has no effect | After changing it you must call `setConfig()` again |
