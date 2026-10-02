# LatencyPlayer SDK — Maven 接入与完整使用指南

> 坐标：`com.latencyplayer:latencyplayer-sdk:1.0.2`
> 支持：Maven 本地仓库 / Maven 远程仓库（私有 Nexus、GitHub Packages、阿里云效等）

---

## 一、SDK 产物与发布

### 1.1 发布到本地仓库（开发自测，无需网络）

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :sdk:publishReleasePublicationToProjectLocalRepository --no-daemon
```

产出：

```
E:\Android-SDK\maven-repo\com\latencyplayer\latencyplayer-sdk\1.0.2\
    latencyplayer-sdk-1.0.2.aar            (58.2 MB，含 4 ABI so)
    latencyplayer-sdk-1.0.2-sources.jar
    latencyplayer-sdk-1.0.2.pom            (传递依赖: androidx.annotation / appcompat)
```

### 1.2 发布到 Nexus Repository 3

> 本项目已接入的 Nexus：`http://192.168.3.160:8970/repository/maven-releases/`
> （匿名可读；发布用有写权限的账号，发布时通过 `-P` 传账密，不落库）
>
> **下述第一~三步为通用建库指引，本项目已就绪**：仓库 `maven-releases` 已存在
> （`maven2 hosted`，`writePolicy=ALLOW_ONCE`），发布账号用 Nexus 上有写权限的账号即可。
> 你们 Nexus 实际端口是 `8970`（非默认 8081）。

#### 第一步：Nexus 上创建 Maven 仓库（本项目已完成，供换服务器时参考）

1. 登录 Nexus 管理台 `http://<nexus-host>:8081`（默认账号 `admin`）
2. 右上角 **Settings → Repositories → Create repository**
3. 选 recipe **maven2 (hosted)**，配置：
   - Name：`latencyplayer-releases`（正式版）；如需快照另建 `latencyplayer-snapshots`（maven2 hosted，name 带 `-snapshots`）
   - Strict Content Type / Component Queries：默认即可
   - Deployment policy：`Allow redeploy`（若不允许重发，同版本号二次发布会 400，建议升版本号）
4. 创建完仓库地址即：
   ```
   http://<nexus-host>:8081/repository/latencyplayer-releases/
   ```

#### 第二步：创建发布账号（勿用 admin）

1. **Security → Users → Create local user**
2. User ID 如 `latencyplayer-deploy`，设密码
3. Role 添加 **nx-artifact**（只读下载用 `nx-anonymous` 不够，发布需要写权限；最小权限也可自建 role 勾选 `nx-repository-view-maven2-latencyplayer-releases-add`、`-edit`、`-browse`）

#### 第三步：Gradle 配置

`gradle.properties`：

```properties
# ===== Nexus3 远程仓库 =====
latencyplayer.maven.url=http://<nexus-host>:8081/repository/latencyplayer-releases/
latencyplayer.maven.user=latencyplayer-deploy
latencyplayer.maven.password=******
LATENCYPLAYER_VERSION=1.0.2
```

> 不想明文放仓库里，可改用环境变量：代码里 `findProperty` 会读 `-P` 参数，也可
> `gradlew :sdk:publishReleasePublicationToRemoteRepository -Platencyplayer.maven.url=... -Platencyplayer.maven.user=... -Platencyplayer.maven.password=...`
> 或在 `~/.gradle/gradle.properties` 里放账密（只在本机生效，不进代码库）。

#### 第四步：发布

```bat
set JAVA_HOME=E:\Android-SDK\jdk-17
set GSTREAMER_ROOT_ANDROID=E:\Android-SDK\NDK-CMake-GStreamer
gradlew.bat :sdk:publishReleasePublicationToRemoteRepository --no-daemon ^
  -Platencyplayer.maven.url=http://192.168.3.160:8970/repository/maven-releases/ ^
  -Platencyplayer.maven.user=admin -Platencyplayer.maven.password=******
```

> **本机代理坑**：`~/.gradle/gradle.properties` 里配了 `http.proxyHost=127.0.0.1:1080`，
> 不加 `nonProxyHosts` 时 Gradle 会把内网 Nexus 请求也发给代理 → 读超时 / 502。
> `gradle.properties` 已加 `systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1`（https 同）。
>
> **大文件直传**：Gradle 传 29MB AAR 偶发超时，可改用 curl 直传（同样 201）：
> ```bat
> curl -u admin:****** -X PUT --upload-file sdk-release.aar ^
>   http://192.168.3.160:8970/repository/maven-releases/com/latencyplayer/latencyplayer-sdk/1.0.2/latencyplayer-sdk-1.0.2.aar
> ```
>
> **版本覆盖**：`maven-releases` 是 `ALLOW_ONCE`，同版本再发返回 400
> `Repository does not allow updating assets` —— 升 `LATENCYPLAYER_VERSION` 再发。

上传内容：`latencyplayer-sdk-1.0.2.aar` / `.pom` / `.module` / `-sources.jar` + 各式校验文件。

#### 第五步：验证

浏览器打开（或匿名可读时）：

```
http://<nexus-host>:8081/repository/latencyplayer-releases/com/latencyplayer/latencyplayer-sdk/1.0.2/
```

能看到 `.aar`/`.pom` 即成功。Nexus 里 **Components** 列表也能看到该组件。
本项目验证方式（已通过）：清 `~/.gradle/caches/modules-2/.../com.latencyplayer` + 停用本地 `maven-repo` + `--refresh-dependencies` 构建，成功即证明来自 Nexus。

常用任务名：

| 任务 | 作用 |
|------|------|
| `publishReleasePublicationToProjectLocalRepository` | 发到项目内 `maven-repo/` |
| `publishReleasePublicationToRemoteRepository` | 发到 Nexus（需配 url/user/password） |
| `publish` | 两个仓库都发（配了才发远程） |

### 1.3 版本升级

改 `gradle.properties`：

```properties
LATENCYPLAYER_VERSION=1.1.0
```

重新执行 1.1 / 1.2，消费方改坐标版本号即可。
（Nexus hosted 仓库默认禁止覆盖同版本，版本号必须递增；快照版用 `-SNAPSHOT` 后缀 + snapshot 仓库。）

---

## 二、接入方完整配置

### 2.1 settings.gradle.kts（根工程）

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

        // 方式 A：本地仓库（同机开发）
        maven {
            url = uri("file:///E:/Android-SDK/maven-repo")
            metadataSources {
                mavenPom()
                artifact()
            }
        }

        // 方式 B：Nexus3 远程仓库（团队/CI，匿名可读则无需 credentials）
        maven {
            name = "LatencyPlayer"
            url = uri("http://192.168.3.160:8970/repository/maven-releases/")
            isAllowInsecureProtocol = true   // http 仓库必须（Gradle 7+ 默认禁止）
            // 仓库开了写保护 / 匿名不可读时才需要：
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

### 2.2 app/build.gradle.kts

```kotlin
plugins {
    id("com.android.application")
}

android {
    namespace = "com.example.yourapp"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.yourapp"
        minSdk = 21            // SDK 要求 >= 21
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
    // ====== LatencyPlayer SDK（Maven 远程/本地依赖）======
    implementation("com.latencyplayer:latencyplayer-sdk:1.0.2")

    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
}
```

> 无需手动拷 AAR、无需 `jniLibs` 配置：so 已打进 AAR，Gradle 自动按 ABI 拆包。
> 无需 app 级 CMake/NDK：SDK 自带 native 编译产物。

### 2.3 AndroidManifest.xml（App）

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

### 2.4 布局 res/layout/activity_main.xml（完整）

```xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    android:padding="16dp">

    <!-- RTMP 地址输入 -->
    <EditText
        android:id="@+id/et_url"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:hint="RTMP 地址"
        android:inputType="textUri"
        android:singleLine="true"
        android:text="rtmp://example.com/live/stream" />

    <!-- 控制按钮 -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="horizontal">

        <Button
            android:id="@+id/btn_play"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="播放" />

        <Button
            android:id="@+id/btn_stop"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="停止" />

        <Button
            android:id="@+id/btn_pause"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_weight="1"
            android:text="暂停" />
    </LinearLayout>

    <!-- 播放视图：禁止设置不透明背景（#000000 会黑屏） -->
    <com.latencyplayer.sdk.LatencyPlayerView
        android:id="@+id/player_view"
        android:layout_width="match_parent"
        android:layout_height="0dp"
        android:layout_weight="1" />

    <TextView
        android:id="@+id/tv_status"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="状态: 未初始化"
        android:textSize="14sp" />

</LinearLayout>
```

### 2.5 MainActivity.java（完整可运行）

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
            status("初始化失败: " + t.getMessage());
            player = null;
            return;
        }

        // 1) 配置（必须在 start 前 setConfig）
        LatencyPlayerConfig config = LatencyPlayerConfig.builder()
                .url(etUrl.getText().toString().trim())
                .bufferTime(0)            // 0 = 最低延迟
                .lowLatency(true)         // 压扁内部队列
                .fastStartup(true)
                .hardwareDecoder(true)
                .autoReconnect(true)      // 断线自动重连
                .reconnectInterval(3)     // 3 秒一次
                .maxReconnectCount(0)     // 0 = 无限
                .build();
        player.setConfig(config);

        // 2) 绑定渲染 View（不要给 LatencyPlayerView 设不透明背景）
        player.setSurface(playerView);

        // 3) 回调（全部在主线程）
        player.setCallback(new LatencyPlayerCallback() {
            @Override
            public void onStateChanged(LatencyPlayerState state) {
                Log.d(TAG, "state=" + state);
                status("状态: " + state.name());
            }

            @Override
            public void onError(int errorCode, String message) {
                Log.e(TAG, "error=" + errorCode + " " + message);
                status("错误: " + LatencyPlayerError.getDescription(errorCode));
            }

            @Override
            public void onBuffering(int percent) {
                status(percent >= 100 ? "缓冲完成" : "缓冲中: " + percent + "%");
            }

            @Override
            public void onConnected() {
                status("已连接");
            }

            @Override
            public void onDisconnected() {
                status("已断开");
            }

            @Override
            public void onFirstFrameRendered() {
                status("播放中");        // 首帧渲染 = 真正出画
            }

            @Override
            public void onDownloadSpeed(long bytesPerSec) {
                // 下载速度 bytes/s
            }

            @Override
            public void onReconnecting(int attemptCount) {
                status("重连中... 第" + attemptCount + "次");
            }

            @Override
            public void onVideoSizeChanged(int width, int height) {
                status("视频: " + width + "x" + height);
            }

            @Override
            public void onPlaybackCompleted() {
                status("播放完成");
            }
        });

        status("已初始化");
    }

    private void play() {
        if (player == null) return;
        String url = etUrl.getText().toString().trim();
        if (url.isEmpty()) {
            Toast.makeText(this, "请输入 RTMP 地址", Toast.LENGTH_SHORT).show();
            return;
        }
        player.getConfig().setUrl(url);
        player.start();
    }

    private void stop() {
        if (player == null) return;
        player.stop();
        status("已停止");
    }

    private void pauseOrResume() {
        if (player == null) return;
        if (player.isPlaying()) {
            player.pause();
            btnPause.setText("恢复");
        } else {
            player.resume();
            btnPause.setText("暂停");
        }
    }

    private void status(String s) {
        runOnUiThread(() -> tvStatus.setText(s));
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (player != null && player.isPlaying()) {
            player.pause();     // 后台不占解码器
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 需要自动恢复时再调 player.resume()
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (player != null) {
            try {
                player.release();   // 释放 native 资源，必须调用
            } catch (Throwable ignored) {
            }
            player = null;
        }
    }
}
```

### 2.6 混淆（app/proguard-rules.pro，开启 minify 时）

SDK AAR 不含 consumer proguard 文件，开启代码混淆时需自行加：

```proguard
-keep class com.latencyplayer.sdk.** { *; }
-keep class org.freedesktop.gstreamer.** { *; }
-keepclasseswithmembernames class * { native <methods>; }
-keep public interface com.latencyplayer.sdk.LatencyPlayerCallback { *; }
```

---

## 三、API 操作速查

```java
LatencyPlayerManager p = new LatencyPlayerManager();
p.init(context);                                  // 1. 初始化（Application Context）
p.setConfig(config);                              // 2. 配置
p.setSurface(view);                               // 3. 绑定画面
p.setCallback(callback);                          // 4. 回调
p.start();                                        // 5. 播放

p.pause();  p.resume();  p.stop();                // 播放控制
p.switchUrl("rtmp://new/live/stream2");           // 热切换 URL
p.setMute(true);  p.setVolume(50);                // 静音/音量(0-100)
p.setBufferTime(0);                               // 运行时改缓冲
p.setFlip(true, false);  p.setRotation(90);       // 翻转/旋转
p.saveSnapshot("/sdcard/a.png");                  // 截图（.jpg 则输出 JPEG，返回 boolean）
p.isPlaying();  p.getState();                     // 状态查询
p.release();                                      // 释放（onDestroy 必调）
```

配置项（`LatencyPlayerConfig.builder()`）：

| 参数 | 默认 | 说明 |
|------|------|------|
| `url` | null | 播放地址，start 前必填 |
| `bufferTime` | 200 | 缓冲 ms，0–5000，0=最低延迟 |
| `lowLatency` | false | 压扁 queue2/multiqueue 内部队列 |
| `fastStartup` | true | 首屏秒开标记 |
| `hardwareDecoder` | true | 硬解优先 |
| `autoReconnect` | true | 断线自动重连 |
| `reconnectInterval` | 3 | 重连间隔（秒） |
| `maxReconnectCount` | 0 | 0=无限次 |
| `orientation` | 1 | 1=竖屏 2=横屏 |
| `mute` / `volume` | false / 100 | 静音 / 音量 0–100 |

> 改配置后必须再调一次 `setConfig()` 才会下发到 native。

---

## 四、功能清单

| 功能 | 状态 | 说明 |
|------|------|------|
| RTMP / RTMPS 拉流直播 | ✅ | 主路径，rtmpsrc/librtmp |
| appsink 直绘渲染 | ✅ | RGBx/RGBA → ANativeWindow，绕过 EGL |
| 软/硬解切换 | ✅ | openh264 软解 + MediaCodec 路径 |
| 低延迟模式 | ✅ | bufferTime=0 + lowLatency 压队列 |
| 断线自动重连 | ✅ | 间隔/次数可配，0=无限 |
| 暂停/恢复/停止 | ✅ | |
| 热切换 URL | ✅ | switchUrl |
| 静音/音量 | ✅ | 0–100 |
| 翻转/旋转 | ✅ | 水平/垂直翻转、0/90/180/270 |
| 截图 | ✅ | saveSnapshot(path)，PNG/JPEG 按后缀（pngenc/jpegenc），返回 boolean；真机验证 PNG/JPEG 均合法 |
| 下载速度/码率回调 | ✅ | onDownloadSpeed |
| 首帧/视频尺寸回调 | ✅ | 主线程回调 |
| HLS / HTTP-FLV / RTSP | ✅ | 1.0.2 回归通过（RTSP / HTTP-FLV / HLS 点播 + HLS 直播出画） |
| WebRTC | ❌ | 不支持 |

---

## 五、设备与兼容性支持

### 5.1 系统与 ABI

| 项 | 支持范围 |
|----|---------|
| 最低系统 | Android 5.0（API 21） |
| 目标系统 | Android 14（API 34），已实测 |
| ABI | `arm64-v8a`、`armeabi-v7a`、`x86`、`x86_64` |
| 主力真机 | 小米 11（arm64 / Android 14）实测出画通过 |
| 模拟器 | x86_64 可运行（glimagesink 在部分模拟器有兼容问题，SDK 默认走 appsink 直绘规避） |
| 16 KB 页设备 | ✅ 64 位 so LOAD 段 `0x4000` 对齐，`zipalign -P 16` 通过（Android 15+） |
| 32 位老设备 | ✅ 保持 4 KB 对齐（16 KB 只要求 64 位） |
| 折叠屏/平板 | ✅ Surface 尺寸变化自动重绑 |
| 横竖屏 | ✅ configChanges 内切屏不断流 |

### 5.2 已验证机型

- 小米 11（`M2011K2C`，arm64，Android 14，API 34）：720p RTMP 直播出画、播放/停止/重连正常

> 其余 arm64/armv7 设备理论兼容（纯 NDK + GStreamer 静态库），建议接入后用目标机型回归首帧与重连。

### 5.3 体积（接入方关注）

| 产物 | 大小 |
|------|------|
| AAR（4 ABI） | 1.0.0: 29.4 MB / 1.0.1: 34.1 MB / 1.0.2: 58.2 MB |
| arm64 so（strip 后） | 1.0.0: 17.6 MB / 1.0.1: 20.3 MB / 1.0.2: 32.3 MB |
| 接入方 APK 增量 | 按所选 ABI 拆分，单 arm64 约 +32.3 MB（1.0.2） |

---

## 六、常见问题

| 现象 | 处理 |
|------|------|
| `Could not find com.latencyplayer:latencyplayer-sdk:1.0.2` | 确认消费方 `settings.gradle.kts` 加了仓库；本地仓库先执行 `:sdk:publish...ProjectLocalRepository` |
| 拉 Nexus `Read timed out`（curl 却秒通） | 全局代理没绕过内网：`gradle.properties` 加 `systemProp.http.nonProxyHosts=192.168.*|10.*|localhost|127.0.0.1` |
| 发布报 400 `does not allow updating assets` | Nexus `ALLOW_ONCE` 禁止同版本覆盖：升 `LATENCYPLAYER_VERSION` 再发 |
| 发布 29MB AAR 超时/502 | Gradle 大文件偶发超时，可 curl 直传（见 1.2 第四步） |
| 黑屏但状态 PLAYING | `LatencyPlayerView` 不能设不透明背景（`#000000`） |
| 快速 停止→播放 崩溃 | 已知问题（修复中）；停止后等状态回 `STOPPED` 再 start |
| 连点播放崩溃 | 已修复（1.0.0 含 `gst_object_ref_sink` 修复） |
| 初始化崩溃 | 检查 ABI 是否被 app `abiFilters` 过滤掉了 SDK 的 so |
| 首屏慢 | `bufferTime(0)` + `lowLatency(true)` + `fastStartup(true)` |
| 改配置不生效 | 改完必须再调 `setConfig()` |
