# JNI_OnLoad of libmediainfo.so looks this class up by name, and the native functions use its mi field
-keep class net.mediaarea.mediainfo.MediaInfo {
    long mi;
    native <methods>;
}
