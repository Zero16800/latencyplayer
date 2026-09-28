-keep class com.latencyplayer.sdk.** { *; }
-keep class org.freedesktop.gstreamer.** { *; }
-keep class org.freedesktop.gstreamer.android.** { *; }

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep callback interfaces
-keep public interface com.latencyplayer.sdk.LatencyPlayerCallback { *; }
-keep public interface * extends com.latencyplayer.sdk.LatencyPlayerCallback { *; }
