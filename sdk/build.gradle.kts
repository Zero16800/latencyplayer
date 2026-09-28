plugins {
    id("com.android.library")
    id("maven-publish")
}

android {
    namespace = "com.latencyplayer.sdk"
    compileSdk = 34

    defaultConfig {
        minSdk = 21
        targetSdk = 34

        externalNativeBuild {
            cmake {
                cppFlags += ""
                arguments += listOf(
                    // c++_static: NDK r25 的 libc++_shared.so 只有 4KB 对齐，
                    // 16KB 页设备不合规；SDK 源码纯 C，静态化后不再打入该 so。
                    "-DANDROID_STL=c++_static",
                    "-DGSTREAMER_ROOT_ANDROID=${findProperty("GSTREAMER_ROOT_ANDROID") ?: System.getenv("GSTREAMER_ROOT_ANDROID") ?: ""}"
                )
            }
        }

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

dependencies {
    implementation("androidx.annotation:annotation:1.7.1")
    implementation("androidx.appcompat:appcompat:1.6.1")
}

// ========== Maven 发布 ==========
// 坐标: com.latencyplayer:latencyplayer-sdk:<version>
//
// 本地仓库（默认）: gradlew :sdk:publishReleasePublicationToProjectLocalRepository
//   -> E:\Android-SDK\maven-repo，消费方 repositories 加 maven { url = .../maven-repo }
//
// 远程仓库: gradlew :sdk:publishReleasePublicationToRemoteRepository
//   需在 gradle.properties 或命令行提供:
//   latencyplayer.maven.url=https://maven.example.com/repository/releases/
//   latencyplayer.maven.user=xxx   latencyplayer.maven.password=xxx
val sdkVersion: String = (findProperty("LATENCYPLAYER_VERSION") as String?) ?: "1.0.0"

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("release") {
                from(components["release"])
                groupId = "com.latencyplayer"
                artifactId = "latencyplayer-sdk"
                version = sdkVersion

                pom {
                    name.set("LatencyPlayer SDK")
                    description.set("Android RTMP live player SDK based on GStreamer 1.28.7 (static build)")
                    url.set("https://example.com/latencyplayer")
                    licenses {
                        license {
                            name.set("Proprietary")
                        }
                    }
                }
            }
        }

        repositories {
            // 1) 项目内本地仓库（默认，无需网络）
            maven {
                name = "ProjectLocal"
                url = uri(rootProject.layout.projectDirectory.dir("maven-repo"))
            }

            // 2) 远程仓库（配置了 url 才启用）
            val remoteUrl = findProperty("latencyplayer.maven.url") as String?
            if (remoteUrl != null && remoteUrl.isNotBlank()) {
                maven {
                    name = "Remote"
                    url = uri(remoteUrl)
                    credentials {
                        username = findProperty("latencyplayer.maven.user") as String? ?: ""
                        password = findProperty("latencyplayer.maven.password") as String? ?: ""
                    }
                    // 允许发布到快照/正式仓库（http 基本认证即可，无需 allowInsecureProtocol 时去掉）
                    if (remoteUrl.startsWith("http://")) {
                        isAllowInsecureProtocol = true
                    }
                }
            }
        }
    }
}
