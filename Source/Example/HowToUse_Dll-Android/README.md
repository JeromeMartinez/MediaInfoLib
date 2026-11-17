# MediaInfoLib Android example

This app uses the MediaInfoLib Android library in two ways:
- from Kotlin, with the `MediaInfo` class: a generated PNG file opened through a `ParcelFileDescriptor`, and a URL;
- from C++, with the Prefab package: `app/mediainfolib_example_bridge.cpp`, called through `Bridge.kt`.

## Build

The app uses the AAR built in `Source/AndroidLib` (`mediainfolib/build/outputs/aar/mediainfolib-release.aar`), so build the library first: see `Source/AndroidLib/README.md`, which also lists the prerequisites.

Then, from this folder:
```sh
./gradlew assembleDebug
```
The APK is then `app/build/outputs/apk/debug/app-debug.apk`.
