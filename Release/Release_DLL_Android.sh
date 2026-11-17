#!/bin/bash

##  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 #
 #  Use of this source code is governed by a BSD-style license that can
 #  be found in the License.html file in the root of the source tree.
 ##

#-----------------------------------------------------------------------
# Helpers
set -e # fail on any error
releasedir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#-----------------------------------------------------------------------
# Used variables
if [ -z "$VERSION" ] ; then
    VERSION=$(<"$releasedir"/../Project/version.txt)
fi

if [ -z "$JAVA_HOME" ] ; then
    echo "error: missing mandatory environment \$JAVA_HOME" >&2
    exit 1
fi

# The SDK licenses must also be accepted (sdkmanager --licenses), for the installation of the NDK and of CMake by Gradle
if [ -z "$ANDROID_HOME" ] ; then
    echo "error: missing mandatory environment \$ANDROID_HOME" >&2
    exit 1
fi

if [ ! -d "$releasedir"/../../ZenLib/Source ] ; then
    echo "error: missing ZenLib sources next to MediaInfoLib" >&2
    exit 1
fi

#-----------------------------------------------------------------------
# Clean
rm -fr "$releasedir"/../Source/AndroidLib/mediainfolib/.cxx
rm -fr "$releasedir"/../Source/AndroidLib/mediainfolib/build
rm -fr "$releasedir"/MediaInfo_DLL_Android
rm -fr "$releasedir"/MediaInfo_DLL_Android_AAR
rm -f "$releasedir"/MediaInfo_DLL_*_Android.aar
rm -f "$releasedir"/MediaInfo_DLL_*_Android.zip

#-----------------------------------------------------------------------
# Build
pushd "$releasedir"/../Source/AndroidLib
    ./gradlew --no-daemon assembleRelease
popd

#-----------------------------------------------------------------------
# Package
pushd "$releasedir"
    # AAR, with the license
    cp ../Source/AndroidLib/mediainfolib/build/outputs/aar/mediainfolib-release.aar MediaInfo_DLL_${VERSION}_Android.aar
    zip -j MediaInfo_DLL_${VERSION}_Android.aar ../License.html

    # Zip for the projects not using Gradle: libraries and headers from the Prefab package of the AAR, MediaInfoDLL.h, license
    unzip -q MediaInfo_DLL_${VERSION}_Android.aar 'prefab/**' -d MediaInfo_DLL_Android_AAR
    for libdir in MediaInfo_DLL_Android_AAR/prefab/modules/mediainfo/libs/android.* ; do
        abi=${libdir##*/android.}
        mkdir -p MediaInfo_DLL_Android/lib/$abi
        cp "$libdir"/libmediainfo.so MediaInfo_DLL_Android/lib/$abi
    done
    cp -r MediaInfo_DLL_Android_AAR/prefab/modules/mediainfo/include MediaInfo_DLL_Android
    cp ../Source/MediaInfoDLL/MediaInfoDLL.h MediaInfo_DLL_Android/include/MediaInfoDLL
    cp ../License.html MediaInfo_DLL_Android
    rm -fr MediaInfo_DLL_Android_AAR
    zip -r MediaInfo_DLL_${VERSION}_Android.zip MediaInfo_DLL_Android
popd
