/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

package net.mediaarea.mediainfo.example

import android.app.Application
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.os.ParcelFileDescriptor
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.graphics.createBitmap
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.mediaarea.mediainfo.MediaInfo
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

data class MediaInfoState(
    val version: String = "Loading...",
    val inform: String = "Loading...",
    val parameters: String = "Loading...",
    val codecs: String = "Loading...",
    val nativeOutput: String = "Loading..."
)

data class UrlState(
    val url: String = "https://raw.githubusercontent.com/MediaArea/MediaInfoLib/refs/heads/master/Source/Resource/Image/MediaInfo.ico",
    val isLoading: Boolean = false,
    val result: String = "",
    val error: String = ""
)

class MediaViewModel(application: Application) : AndroidViewModel(application) {
    var uiState by mutableStateOf(MediaInfoState())
        private set

    // In the ViewModel, so that the work and its result are kept on rotation or tab switch
    var urlState by mutableStateOf(UrlState())
        private set

    init {
        loadData()
    }

    fun setUrl(url: String) {
        urlState = urlState.copy(url = url)
    }

    fun processUrl() {
        val url = urlState.url
        if (url.isBlank() || urlState.isLoading)
            return
        urlState = urlState.copy(isLoading = true, result = "", error = "")

        viewModelScope.launch {
            // MediaInfo on a background worker thread, then the state is updated on the main thread
            val (result, error) = withContext(Dispatchers.IO) {
                // The MediaInfo object is destroyed at the end of use { }, on this worker thread
                MediaInfo().use { mi ->
                    mi.Option("Language", "  Config_Text_ColumnSize;20")
                    if (mi.Open(url) == 0)
                        Pair("", "Can not open $url")
                    else
                        Pair(mi.Inform(), "")
                }
            }
            urlState = urlState.copy(isLoading = false, result = result, error = error)
        }
    }

    private fun loadData() {
        viewModelScope.launch(Dispatchers.IO) {
            val context = getApplication<Application>()

            // Create sample file
            val path = createSamplePng(context)

            // Kotlin interface
            // Using net.mediaarea.mediainfo Kotlin JNI wrapper, the object is destroyed at the end of use { }
            val state = MediaInfo().use { mi ->
                val version = mi.Option("Info_Version")
                val parameters = mi.Option("Info_Parameters")
                val codecs = mi.Option("Info_Codecs")
                mi.Option("Language", "  Config_Text_ColumnSize;20")
                if (path != null) {
                    // MediaInfo reads a duplicate of the descriptor, the ParcelFileDescriptor is closed at the end of use { }
                    ParcelFileDescriptor.open(File(path), ParcelFileDescriptor.MODE_READ_ONLY).use { pfd ->
                        mi.Open(pfd, "sample.png")
                    }
                }
                MediaInfoState(
                    version = version,
                    inform = mi.Inform(),
                    parameters = parameters,
                    codecs = codecs
                )
            }

            // C++ interface
            // Bridge is just an example bridge that receives output from C++ code
            val bridge = Bridge()
            var output = bridge.Version()
            output += bridge.Inform(path ?: "")
            output += bridge.Info()

            withContext(Dispatchers.Main) {
                uiState = state.copy(nativeOutput = output)
            }
        }
    }

    private fun createSamplePng(context: Context): String? {
        val bitmap = createBitmap(100, 100)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GRAY)

        val file = File(context.filesDir, "sample.png")

        try {
            FileOutputStream(file).use { out ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
                return file.absolutePath
            }
        } catch (e: IOException) {
            e.printStackTrace()
            return null
        }
    }
}
