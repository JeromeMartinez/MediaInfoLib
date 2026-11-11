/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Global configuration of MediaInfo
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#ifndef MediaInfo_ConstH
#define MediaInfo_ConstH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstddef>
#include <limits.h>
#include <sstream>
#include <string>
//ZenLib is not needed by the interface, ZenLib/Conf.h is still included if it is available, for the programs which use its types or macros
//through these headers
#if defined(__has_include)
    #if __has_include(<ZenLib/Conf.h>)
        #include <ZenLib/Conf.h>
    #endif
#else
    #include <ZenLib/Conf.h>
#endif
//---------------------------------------------------------------------------

//***************************************************************************
// Platforms (as in ZenLib/Conf.h)
//***************************************************************************

//---------------------------------------------------------------------------
//Win32
#if defined(__NT__) || defined(_WIN32) || defined(WIN32)
    #ifndef WIN32
        #define WIN32
    #endif
    #ifndef _WIN32
        #define _WIN32
    #endif
    #ifndef __WIN32__
        #define __WIN32__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//Win64
#if defined(_WIN64) || defined(WIN64)
    #ifndef WIN64
        #define WIN64
    #endif
    #ifndef _WIN64
        #define _WIN64
    #endif
    #ifndef __WIN64__
        #define __WIN64__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//Windows
#if defined(WIN32) || defined(WIN64)
    #ifndef WINDOWS
        #define WINDOWS
    #endif
    #ifndef _WINDOWS
        #define _WINDOWS
    #endif
    #ifndef __WINDOWS__
        #define __WINDOWS__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//Windows UWP
#if defined(WIN32) || defined(WIN64)
    #if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_APP)
        #ifndef WINDOWS_UWP
            #define WINDOWS_UWP
        #endif
    #endif
    #if defined(WINAPI_FAMILY) && (WINAPI_FAMILY==WINAPI_FAMILY_GAMES)
        #ifndef WINDOWS_GAMES
            #define WINDOWS_GAMES
        #endif
    #endif
#endif

//---------------------------------------------------------------------------
//Unix (Linux, HP, Sun, BeOS...)
#if defined(UNIX) || defined(_UNIX) || defined(__UNIX__) \
    || defined(__unix) || defined(__unix__) \
    || defined(____SVR4____) || defined(__LINUX__) || defined(__sgi) \
    || defined(__hpux) || defined(sun) || defined(__SUN__) || defined(_AIX) \
    || defined(__EMX__) || defined(__VMS) || defined(__BEOS__)
    #ifndef UNIX
        #define UNIX
    #endif
    #ifndef _UNIX
        #define _UNIX
    #endif
    #ifndef __UNIX__
        #define __UNIX__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//Linux
#if defined(__LINUX__) || defined(__linux__)
    #ifndef LINUX
        #define LINUX
    #endif
    #ifndef _LINUX
        #define _LINUX
    #endif
    #ifndef __LINUX__
        #define __LINUX__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//MacOS Classic
#if defined(macintosh)
    #ifndef MACOS
        #define MACOS
    #endif
    #ifndef _MACOS
        #define _MACOS
    #endif
    #ifndef __MACOS__
        #define __MACOS__ 1
    #endif
#endif

//---------------------------------------------------------------------------
//MacOS X
#if defined(__APPLE__) && defined(__MACH__)
    #ifndef MACOSX
        #define MACOSX
    #endif
    #ifndef _MACOSX
        #define _MACOSX
    #endif
    #ifndef __MACOSX__
        #define __MACOSX__ 1
    #endif
#endif

//Test of targets
#if defined(WINDOWS) && defined(UNIX) && defined(MACOS) && defined(MACOSX)
    #pragma message Multiple platforms???
#endif

#if !defined(WIN32) && !defined(UNIX) && !defined(MACOS) && !defined(MACOSX)
    #pragma message No known platforms, assume default
#endif

//***************************************************************************
// Integer types of the interface, as in the C interface (MediaInfoDLL.h),
// the same types as ZenLib::int8u and ZenLib::int64u (as in ZenLib/Conf.h)
//***************************************************************************

//---------------------------------------------------------------------------
//8-bit int
#if UCHAR_MAX==0xff
    typedef unsigned char       MediaInfo_int8u;
#else
    #pragma message This machine has no 8-bit integertype?
#endif

//---------------------------------------------------------------------------
//64-bit int
#if defined(__MINGW32__) || defined(__CYGWIN32__) || defined(__UNIX__) || defined(__MACOSX__)
    typedef unsigned long long  MediaInfo_int64u;
#elif defined(__WIN32__)
    typedef unsigned __int64    MediaInfo_int64u;
#else
    #pragma message This machine has no 64-bit integer type?
#endif
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//---------------------------------------------------------------------------
/// \mainpage MediaInfoLib Index Page
///
/// \section Interfaces
///
/// There is 2 access methods
///
/// - MediaInfo class \n
/// To manage one file, this is the simplest interface. \n
///  - MediaInfo::Open to analyse file \n
///  - MediaInfo::Inform to have a summary \n
///  - MediaInfo::Get to retreive one piece of information \n
///
/// - MediaInfoList class \n
/// To manage a list of files \n
///  - MediaInfoList::Open to analyse file \n
///  - MediaInfoList::Inform to have a summary \n
///  - MediaInfoList::Get to retreive one piece of information \n
///  - MediaInfoList::Close to close one file \n
///
/// \section C C Interface (MediaInfo_*)
/// For compatibility and DLL interface \n
/// This is a C interface for the List class \n
/// Note : Don't forget to include the MediaInfoDLL.h file in your source file! \n
/// - Example of commands:
///  - MediaInfo_Open to analyse file \n
///  - MediaInfo_Inform to have a summary \n
///  - MediaInfo_Get to retreive one piece of information \n
///  - MediaInfo_Close to free memory \n
///
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/// @brief Kinds of Stream
enum stream_t
{
    Stream_General,                 ///< StreamKind = General
    Stream_Video,                   ///< StreamKind = Video
    Stream_Audio,                   ///< StreamKind = Audio
    Stream_Text,                    ///< StreamKind = Text
    Stream_Other,                   ///< StreamKind = Chapters
    Stream_Image,                   ///< StreamKind = Image
    Stream_Menu,                    ///< StreamKind = Menu
    Stream_Max
};

/// @brief Kind of information
enum info_t
{
    Info_Name,                      ///< InfoKind = Unique name of parameter
    Info_Text,                      ///< InfoKind = Value of parameter
    Info_Measure,                   ///< InfoKind = Unique name of measure unit of parameter
    Info_Options,                   ///< InfoKind = See infooptions_t
    Info_Name_Text,                 ///< InfoKind = Translated name of parameter
    Info_Measure_Text,              ///< InfoKind = Translated name of measure unit
    Info_Info,                      ///< InfoKind = More information about the parameter
    Info_HowTo,                     ///< InfoKind = How this parameter is supported, could be N (No), B (Beta), R (Read only), W (Read/Write)
    Info_Domain,                    ///< InfoKind = Domain of this piece of information
    Info_Max
};

/// Get(...)[infooptions_t] return a string like "YNYN..." \n
/// Use this enum to know at what correspond the Y (Yes) or N (No)
/// If Get(...)[0]==Y, then :
/// @brief Option if InfoKind = Info_Options
enum infooptions_t
{
    InfoOption_ShowInInform,        ///< Show this parameter in Inform()
    InfoOption_Reserved,            ///<
    InfoOption_ShowInSupported,     ///< Internal use only (info : Must be showed in Info_Capacities() )
    InfoOption_TypeOfValue,         ///< Value return by a standard Get() can be : T (Text), I (Integer, warning up to 64 bits), F (Float), D (Date), B (Binary datas coded Base64) (Numbers are in Base 10)
    InfoOption_ShowInXml,           ///<
    InfoOption_Max
};

/// @brief File opening options
enum fileoptions_t
{
    FileOption_Nothing      =0x00,
    FileOption_NoRecursive  =0x01,  ///< Do not browse folders recursively
    FileOption_CloseAll     =0x02,  ///< Close all files before open
    FileOption_Max          =0x04
};

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//Char types
#undef  __T
#define __T(__x)     __T(__x)
#if defined(UNICODE) || defined (_UNICODE)
    typedef wchar_t Char;                                               ///< Unicode/Ansi independant char
    #undef  __T
    #define __T(__x) L ## __x
#else
    typedef char Char;                                                  ///< Unicode/Ansi independant char
    #undef  __T
    #define __T(__x) __x
#endif
typedef std::basic_string<MediaInfoLib::Char>        String;            ///< Unicode/Ansi independant string
typedef std::basic_stringstream<MediaInfoLib::Char>  StringStream;      ///< Unicode/Ansi independant stringstream
typedef std::basic_istringstream<MediaInfoLib::Char> tiStringStream;    ///< Unicode/Ansi independant istringstream
typedef std::basic_ostringstream<MediaInfoLib::Char> toStringStream;    ///< Unicode/Ansi independant ostringstream
//---------------------------------------------------------------------------

} //NameSpace

#endif
