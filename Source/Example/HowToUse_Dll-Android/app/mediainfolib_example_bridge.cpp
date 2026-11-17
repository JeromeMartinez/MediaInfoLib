/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

#include <jni.h>
#include <string>
#include <MediaInfoDLL/MediaInfoDLL_Static.h> // Header supplied automatically by Prefab!

// This can be your custom native logic to do whatever processing before passing the result to your Kotlin side

extern "C"
JNIEXPORT jstring JNICALL
Java_net_mediaarea_mediainfo_example_Bridge_Version(JNIEnv *env, jobject thiz) {
    MediaInfoDLL::MediaInfo MI;
    std::string MediaInfo_Output = MI.Option(__T("Info_Version"));
    return env->NewStringUTF(MediaInfo_Output.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_mediaarea_mediainfo_example_Bridge_Info(JNIEnv *env, jobject thiz) {
    MediaInfoDLL::MediaInfo MI;
    std::string MediaInfo_Output;
    MediaInfo_Output += __T("\n\nInfo_Parameters\n");
    MediaInfo_Output += __T("====================\n");
    MediaInfo_Output += MI.Option(__T("Info_Parameters"));
    MediaInfo_Output += __T("\n\nInfo_Codecs\n");
    MediaInfo_Output += __T("====================\n");
    MediaInfo_Output += MI.Option(__T("Info_Codecs"));
    return env->NewStringUTF(MediaInfo_Output.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_mediaarea_mediainfo_example_Bridge_Inform(JNIEnv *env, jobject thiz, jstring path) {
    // Copy of the path, so that the JNI buffer is released at once
    const char *nativePath = env->GetStringUTFChars(path, nullptr);
    if (!nativePath)
        return nullptr; // OutOfMemoryError thrown
    std::string Path(nativePath);
    env->ReleaseStringUTFChars(path, nativePath);

    MediaInfoDLL::MediaInfo MI;
    MI.Option(__T("Language"), __T("  Config_Text_ColumnSize;20"));
    std::string MediaInfo_Output = __T("\n\nInform\n");
    MediaInfo_Output += __T("====================\n");
    if (MI.Open(Path))
        MediaInfo_Output += MI.Inform();
    else
        MediaInfo_Output += __T("Can not open ") + Path;
    return env->NewStringUTF(MediaInfo_Output.c_str());
}
