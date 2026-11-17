plugins {
    alias(libs.plugins.android.library)
}

// Version of the release, also written into the Prefab metadata
version = rootDir.resolve("../../Project/version.txt").readText().trim()

// Public headers: the C API and the events it can report
// The C++ classes are hidden by the version script, and MediaInfoDLL.h loads the library at runtime with another name
val consolidateHeaders by tasks.registering(Sync::class) {
    from("../../../Source/MediaInfo") {
        include(
            "MediaInfo_Events.h"
        )
        into("MediaInfo")
    }
    from("../../../Source/MediaInfoDLL") {
        include(
            "MediaInfoDLL_Static.h"
        )
        into("MediaInfoDLL")
    }
    destinationDir = layout.buildDirectory.dir("prefabHeadersStaging").get().asFile
}

android {
    namespace = "net.mediaarea.mediainfo.lib"
    compileSdk {
        version = release(37)
    }
    // Same NDK for all releases, whatever the default NDK of the Android Gradle plugin
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 24

        aarMetadata {
            minCompileSdk = minSdk
        }

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
        }

        @Suppress("UnstableApiUsage")
        externalNativeBuild {
            cmake {
                arguments(
                    // libc++ is linked statically and hidden by the version script, so the apps can use any STL
                    "-DANDROID_STL=c++_static",
                    "-DENABLE_UNICODE=ON"
                )
            }
        }

        consumerProguardFiles("consumer-rules.pro")
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
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
    buildFeatures {
        prefabPublishing = true
        prefab = true
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    packaging {
        jniLibs {
            // Debug info (line tables only, see CMakeLists.txt) kept for the crash traces of the apps. The Android Gradle plugin
            // strips it when packaging an app, if the NDK of the app build is installed
            keepDebugSymbols.add("**/*.so")
        }
    }
    prefab {
        create("mediainfo") {
            headers = consolidateHeaders.map { it.destinationDir.absolutePath }.get()
        }
    }
}

tasks.configureEach {
    if (name.contains("prefab") && name.contains("ConfigurePackage")) {
        dependsOn(consolidateHeaders)
    }

    // The Android Gradle plugin declares the STL of the library (c++_static) in the Prefab metadata, and Prefab then
    // refuses this shared library for apps using an STL. libc++ is not exposed by the C API, its symbols are hidden by
    // the version script, and memory is freed on the side which allocated it: for this case, Prefab says to declare no STL.
    if (name.startsWith("prefab") && name.endsWith("Package") && !name.contains("Configure")) {
        doLast {
            outputs.files.asFileTree.matching { include("**/abi.json") }.forEach { f ->
                f.writeText(f.readText().replace(Regex("\"stl\"\\s*:\\s*\"c\\+\\+_static\""), "\"stl\": \"none\""))
            }
        }
    }
}

dependencies {
    // Curl static build from https://github.com/vvb2060/curl-android
    implementation(libs.curl)
}
