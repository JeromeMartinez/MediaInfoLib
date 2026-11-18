#!/bin/sh
# Generation of the small artificial files of the samples test (samples.sh), for information:
# the files are stored in the repository and in the source archives, they are not generated during the tests.
# They are created by MediaArea from synthetic sources (test patterns and tones), so they can be redistributed
# with the source code under the same license.
# Not generated: Example.ogg (Ogg Vorbis with metadata in several scripts, also used by unicode.sh and copied in the
# developer packages) and Example.dxw (file referencing Example.ogg), the historical example files of MediaInfoLib.
# Created with FFmpeg 6.1.1 (Ubuntu 24.04 package), output may slightly differ with other versions.

set -e
cd "$(dirname "$0")"

FF="ffmpeg -hide_banner -loglevel error -y"
BITEXACT="-fflags +bitexact -flags:v +bitexact -flags:a +bitexact -map_metadata -1"
VIDEO="-f lavfi -i testsrc2=size=64x64:rate=25:duration=0.08"
AUDIO="-f lavfi -i sine=frequency=1000:sample_rate=48000:duration=0.08"
AUDIO_SHORT="-f lavfi -i sine=frequency=1000:sample_rate=48000:duration=0.02"
TITLE="-metadata title=MediaInfoLib"

$FF $VIDEO $AUDIO $BITEXACT $TITLE -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -b:a 32k -ac 1 -movflags +faststart avc_aac.mp4
$FF $VIDEO $BITEXACT $TITLE -c:v libx265 -preset ultrafast -x265-params log-level=error -pix_fmt yuv420p -tag:v hvc1 hevc.mp4
$FF $VIDEO $AUDIO $BITEXACT $TITLE -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a ac3 -b:a 64k -ac 2 avc_ac3.mkv
$FF $VIDEO $AUDIO $BITEXACT $TITLE -c:v libvpx-vp9 -deadline realtime -c:a libopus -b:a 16k -ac 1 vp9_opus.webm
$FF $VIDEO $AUDIO $BITEXACT -c:v mpeg2video -c:a mp2 -b:a 64k -ac 1 mpeg2_mp2.ts
$FF $VIDEO $AUDIO $BITEXACT $TITLE -c:v mpeg4 -q:v 31 -c:a libmp3lame -b:a 32k -ac 1 mpeg4_mp3.avi
$FF $AUDIO_SHORT $BITEXACT $TITLE -c:a pcm_s16le -ac 2 pcm.wav
$FF $AUDIO $BITEXACT $TITLE -c:a libmp3lame -b:a 32k -ac 1 -id3v2_version 3 mp3.mp3
$FF $AUDIO $BITEXACT $TITLE -c:a aac -b:a 32k -ac 1 aac.aac
$FF $AUDIO_SHORT $BITEXACT $TITLE -c:a flac -ac 1 flac.flac
$FF -f lavfi -i testsrc2=size=64x64:rate=25:duration=0.04 -f lavfi -i sine=frequency=1000:sample_rate=48000:duration=0.04 $BITEXACT -c:v mpeg2video -q:v 31 -c:a pcm_s16le -ac 1 mpeg2_pcm.mxf
$FF -f lavfi -i testsrc2=size=16x16 -frames:v 1 $BITEXACT -c:v png image.png
$FF -f lavfi -i testsrc2=size=16x16 -frames:v 1 $BITEXACT -c:v mjpeg -pix_fmt yuvj420p image.jpg
