/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

package net.mediaarea.mediainfo

import android.os.ParcelFileDescriptor
import java.io.IOException

/**
 * MediaInfoLib object.
 *
 * It holds native memory until [Destroy] or [close] is called, for example with `MediaInfo().use { }`.
 *
 * Calls on one object are serialized: [Destroy] and [close] wait until a running call returns, for example a long
 * [Open] of a URL. So, call them from the thread doing the work rather than from the main thread.
 */
class MediaInfo : AutoCloseable {
    companion object {
        // load the native library.
        init {
            System.loadLibrary("mediainfo")
        }
    }

    enum class Stream {
        General,
        Video,
        Audio,
        Text,
        Other,
        Image,
        Menu,
        Max
    }

    enum class Info {
        Name,
        Text,
        Measure,
        Options,
        Name_Text,
        Measure_Text,
        Info,
        HowTo,
        Domain,
        Max
    }

    // Native object, used by the native functions, which set it to 0 in Destroy()
    private var mi: Long = Init()

    @Synchronized private external fun Init(): Long

    /** Frees the native object. The object must not be used afterwards. */
    @Synchronized external fun Destroy(): Int

    /** Frees the native object, as [Destroy] does. [Close] only closes the file. */
    override fun close() {
        Destroy()
    }

    @Synchronized private external fun OpenFd(fd: Int, name: String): Int
    @Synchronized external fun Open(name:String): Int

    /**
     * Opens a file descriptor, with [name] as file name.
     *
     * The descriptor is closed by this call, so it must be owned by the caller and not used afterwards,
     * for example from [ParcelFileDescriptor.detachFd]. The variant with a [ParcelFileDescriptor] does that.
     */
    fun Open(fd: Int, name: String): Int {
        return OpenFd(fd, name)
    }

    /**
     * Opens the file of [pfd], with [name] as file name.
     *
     * A duplicate of the descriptor is used, so [pfd] stays open and owned by the caller. The duplicate shares the file
     * offset of [pfd], which the parsing moves. The file is not opened if the descriptor can not be duplicated,
     * for example if [pfd] is closed.
     */
    fun Open(pfd: ParcelFileDescriptor, name: String): Int {
        val fd = try {
            pfd.dup().detachFd()
        } catch (e: IOException) {
            return 0
        }
        return OpenFd(fd, name)
    }
    @Synchronized external fun Open_Buffer_Init(fileSize: Long, fileOffset: Long): Int
    @Synchronized external fun Open_Buffer_Continue(buffer: ByteArray, bufferSize: Long): Int
    @Synchronized external fun Open_Buffer_Continue_GoTo_Get(): Long
    @Synchronized external fun Open_Buffer_Finalize(): Long

    /** Closes the file. The object can then open another file. To free the object, use [Destroy] or [close]. */
    @Synchronized external fun Close(): Int
    @Synchronized external fun Inform(): String
    @Synchronized private external fun GetI(streamKind: Int, streamNumber: Int, parameter: Int, infoKind: Int): String
    @Synchronized private external fun GetS(streamKind: Int, streamNumber: Int, parameter: String, infoKind: Int, searchKind: Int): String
    fun Get(streamKind: Stream, streamNumber: Int, parameter: Int, infoKind: Info = Info.Text) : String {
        return GetI(streamKind.ordinal, streamNumber, parameter, infoKind.ordinal)
    }
    fun Get(streamKind: Stream, streamNumber: Int, parameter: String, infoKind: Info = Info.Text, searchKind: Info = Info.Name) : String {
        return GetS(streamKind.ordinal, streamNumber, parameter, infoKind.ordinal, searchKind.ordinal)
    }
    @Synchronized external fun Option(option: String, value: String = ""): String
    @Synchronized external fun State_Get(): Int
    @Synchronized private external fun Count_Get(streamKind: Int, streamNumber: Int): Int
    fun Count_Get(streamKind: Stream, streamNumber: Int = -1): Int {
        return Count_Get(streamKind.ordinal, streamNumber)
    }
}
