/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

package net.mediaarea.mediainfo.example

class Bridge {
    companion object {
        init {
            System.loadLibrary("mediainfolib_example_bridge")
        }
    }

    external fun Version(): String
    external fun Info(): String
    external fun Inform(path: String): String
}
