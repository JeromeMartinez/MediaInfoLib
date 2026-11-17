# MediaInfoLib for Android

This project builds MediaInfoLib as an Android library (AAR), for arm64-v8a, armeabi-v7a, x86 and x86_64, with:
- a Kotlin interface, the `net.mediaarea.mediainfo.MediaInfo` class;
- a [Prefab](https://google.github.io/prefab/) package for C and C++ code, with the C API of MediaInfoLib;
- libcurl, with BoringSSL, nghttp2, ngtcp2 and nghttp3, for URLs.

Android 7.0 (API level 24) or newer is needed.

## Build

Prerequisites:
- the ZenLib sources next to the MediaInfoLib ones (`../ZenLib` from the MediaInfoLib folder);
- JDK 21, in `JAVA_HOME`;
- the Android SDK, in `ANDROID_HOME`, with its licenses accepted (`sdkmanager --licenses`), so that the Android Gradle plugin can install the NDK and CMake;
- network access, for Gradle and the packages it downloads.

From this folder:
```sh
./gradlew assembleRelease
```
The AAR is then `mediainfolib/build/outputs/aar/mediainfolib-release.aar`.

`Release/Release_DLL_Android.sh` builds it the same way, then makes the release packages in `Release`:
- `MediaInfo_DLL_<version>_Android.aar`, the AAR with `License.html`;
- `MediaInfo_DLL_<version>_Android.zip`, for the projects not using Gradle, with the libraries (`lib/<abi>/libmediainfo.so`), the headers (`include`) and `License.html`.

## Use from Kotlin or Java

Add the AAR to the app module:
```kotlin
dependencies {
    implementation(files("libs/MediaInfo_DLL_<version>_Android.aar"))
}
```
An AAR file brings no dependencies, so an app without Kotlin code also needs the Kotlin standard library (`org.jetbrains.kotlin:kotlin-stdlib`).

Example:
```kotlin
import net.mediaarea.mediainfo.MediaInfo

val text = MediaInfo().use { mi ->
    mi.Open(path) // A file name or a URL
    mi.Inform()
}
```

Notes:
- Each `MediaInfo` object holds native memory until `Destroy()` or `close()` is called, for example by `use { }`.
- The calls on one object are serialized, so `Destroy()` waits until a running call returns. Opening a URL can take a while: do the work, including `Destroy()`, on a worker thread.
- `Open(pfd, name)` opens the file of a `ParcelFileDescriptor`, for example from `ContentResolver.openFileDescriptor()`, and leaves it open. `Open(fd, name)` takes ownership of `fd` and closes it, so `fd` must come from `ParcelFileDescriptor.detachFd()` or similar.
- The R8 rules needed by the native library are in the AAR.

## Use from C or C++ (Prefab)

In the app module:
```kotlin
android {
    buildFeatures {
        prefab = true
    }
}
```

In `CMakeLists.txt`:
```cmake
find_package(mediainfolib REQUIRED CONFIG)
target_link_libraries(your_library mediainfolib::mediainfo)
```

In the code:
```cpp
#include <MediaInfoDLL/MediaInfoDLL_Static.h>
```

The library exports only C functions, and links libc++ statically: the app can use any STL.

## Debug info

The libraries keep line tables (`-gline-tables-only`), so that crash traces in MediaInfoLib show function names, file names and line numbers once symbolized.

The Android Gradle plugin strips this debug info when packaging an app, if the NDK of the app build is installed: the version in `android.ndkVersion`, or the default one of the plugin. Otherwise, it warns "Unable to strip the following libraries, packaging them as they are", and the app gets libraries 3 to 5 times bigger.

## Example

`Source/Example/HowToUse_Dll-Android` is an example app using both interfaces.
