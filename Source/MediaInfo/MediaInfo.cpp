// MediaInfo - All info about media files


//---------------------------------------------------------------------------
// Pre-compilation
#include "MediaInfo/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/MediaInfo.h"
#include "MediaInfo/MediaInfoList.h"
#include "MediaInfo/MediaInfo_Internal.h"
#include "MediaInfo/MediaInfoList_Internal.h"
#include "ZenLib/OS_Utils.h"
#include <cstring>
#if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
    #include <math.h> // needed for _set_FMA3_enable()
#endif
using namespace ZenLib;
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//---------------------------------------------------------------------------
//To clarify the code
namespace MediaInfo_Debug_MediaInfo
{

#if defined (MEDIAINFO_DEBUG_CONFIG) || defined (MEDIAINFO_DEBUG_BUFFER) || defined (MEDIAINFO_DEBUG_OUTPUT)
    #ifdef WINDOWS
        const Char* MediaInfo_Debug_Name=__T("MediaInfo_Debug");
    #else
        const Char* MediaInfo_Debug_Name=__T("/tmp/MediaInfo_Debug");
    #endif
#endif

#ifdef MEDIAINFO_DEBUG_CONFIG
    #define MEDIAINFO_DEBUG_STATIC(_TOAPPEND) \
        { \
            File F(Ztring(MediaInfo_Debug_Name)+__T(".Config.static.txt"), File::Access_Write_Append); \
            Ztring Debug; \
            _TOAPPEND; \
            Debug+=__T("\r\n"); \
            F.Write(Debug); \
            F.Close(); \
        }
#else // MEDIAINFO_DEBUG_CONFIG
    #define MEDIAINFO_DEBUG_STATIC(_TOAPPEND)
#endif // MEDIAINFO_DEBUG_CONFIG

}
using namespace MediaInfo_Debug_MediaInfo;

//***************************************************************************
// Constructor/destructor
//***************************************************************************

//---------------------------------------------------------------------------
MediaInfo::MediaInfo()
{
    Internal=new MediaInfo_Internal();

    // FMA3 support in the 2013 CRT is broken on Vista and Windows 7 RTM (fixed in SP1).
    // See https://connect.microsoft.com/VisualStudio/feedback/details/987093/x64-log-function-uses-vpsrlq-avx-instruction-without-regard-to-operating-system-so-it-crashes-on-vista-x64
    // Hotfix: we disable it for MSVC2013.
    #if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
        _set_FMA3_enable(0);
    #endif
}

//---------------------------------------------------------------------------
MediaInfo::~MediaInfo()
{
    delete Internal; //Internal=NULL;
}

//***************************************************************************
// Files
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Open(const String &File_Name_)
{
#if defined(MEDIAINFO_FILE_YES)
    return Internal->Open(File_Name_);
#else //defined(MEDIAINFO_FILE_YES)
    return 0; //Not available
#endif //defined(MEDIAINFO_FILE_YES)
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open (const int8u* Begin, size_t Begin_Size, const int8u* End, size_t End_Size, int64u File_Size)
{
    return Internal->Open(Begin, Begin_Size, End, End_Size, (File_Size<Begin_Size+End_Size)?(Begin_Size+End_Size):File_Size);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Init (int64u File_Size, int64u File_Offset)
{
    return Internal->Open_Buffer_Init(File_Size, File_Offset);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Continue (const int8u* ToAdd, size_t ToAdd_Size)
{
    return Internal->Open_Buffer_Continue(ToAdd, ToAdd_Size).to_ulong();
}

//---------------------------------------------------------------------------
int64u MediaInfo::Open_Buffer_Continue_GoTo_Get ()
{
    return Internal->Open_Buffer_Continue_GoTo_Get();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Finalize ()
{
    return Internal->Open_Buffer_Finalize();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_NextPacket ()
{
    return Internal->Open_NextPacket().to_ulong();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Save()
{
    return 0; //Not yet implemented
}

//---------------------------------------------------------------------------
void MediaInfo::Close()
{
    return Internal->Close();
}

//***************************************************************************
// Get File info
//***************************************************************************

//---------------------------------------------------------------------------
String MediaInfo::Inform(size_t)
{
    return MediaInfo_Internal::Inform(Internal);
}

//---------------------------------------------------------------------------
String MediaInfo::Get(stream_t StreamKind, size_t StreamPos, size_t Parameter, info_t KindOfInfo)
{
    return Internal->Get(StreamKind, StreamPos, Parameter, KindOfInfo);
}

//---------------------------------------------------------------------------
String MediaInfo::Get(stream_t StreamKind, size_t StreamPos, const String &Parameter, info_t KindOfInfo, info_t KindOfSearch)
{
    return Internal->Get(StreamKind, StreamPos, Parameter, KindOfInfo, KindOfSearch);
}

//***************************************************************************
// Set File info
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Set(const String &, stream_t, size_t, size_t, const String &)
{
    return 0; //Not yet implemented
}

//---------------------------------------------------------------------------
size_t MediaInfo::Set(const String &, stream_t, size_t, const String &, const String &)
{
    return 0; //Not yet implemented
}

//***************************************************************************
// Output buffer
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Output_Buffer_Get (const String &Value)
{
    return Internal->Output_Buffer_Get(Value);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Output_Buffer_Get (size_t Pos)
{
    return Internal->Output_Buffer_Get(Pos);
}

//***************************************************************************
// Information
//***************************************************************************

//---------------------------------------------------------------------------
String MediaInfo::Option (const String &Option, const String &Value)
{
    return Internal->Option(Option, Value);
}

//---------------------------------------------------------------------------
String MediaInfo::Option_Static (const String &Option, const String &Value)
{
    MEDIAINFO_DEBUG_STATIC(Debug+=__T("Option_Static, Option=");Debug+=Ztring(Option);Debug+=__T(", Value=");Debug+=Ztring(Value);)
    MediaInfoLib::Config.Init(); //Initialize Configuration

    if (Option==__T("Info_Capacities"))
    {
        return __T("Option disactivated for this version, will come back soon!");
        //MediaInfo_Internal MI;
        //return MI.Option(Option);
    }
    else if (Option==__T("Info_Version"))
    {
        Ztring ToReturn=MediaInfoLib::Config.Info_Version_Get();
        if (MediaInfo_Internal::LibraryIsModified())
            ToReturn+=__T(" modified");
        return ToReturn;
    }
    else
        return MediaInfoLib::Config.Option(Option, Value);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Count_Get (stream_t StreamKind, size_t StreamPos)
{
    return Internal->Count_Get(StreamKind, StreamPos);

}

//---------------------------------------------------------------------------
size_t MediaInfo::State_Get ()
{
    return Internal->State_Get();
}

//***************************************************************************
// MediaInfoA, MediaInfoW, MediaInfoListA, MediaInfoListW
//***************************************************************************

//---------------------------------------------------------------------------
//Strings of these classes to and from String, up to the first NUL character as with the C interface: in the encoding of the locale (the code
//page on Windows), or UTF-8 with the CharSet option, as with the narrow functions of the C interface, or wide strings (the strings with Unicode)
static String In(const std::string& Value, bool UTF8)
{
    return UTF8?Ztring().From_UTF8(Value.c_str()):Ztring().From_Local(Value.c_str());
}

static String In_FileName(const std::string& Value, bool UTF8)
{
    return UTF8?Ztring().From_UTF8(Value.c_str()):FileName_From_Native(Value.c_str());
}

static std::string Out_Narrow(const String& Value, bool UTF8)
{
    return UTF8?Ztring(Value).To_UTF8():Ztring(Value).To_Local();
}

#if defined(UNICODE)
static String In(const std::wstring& Value, bool)
{
    return Value.c_str();
}

static std::wstring Out_Wide(const String& Value, bool)
{
    return Value.c_str();
}
#elif !defined(WSTRING_MISSING)
static String In(const std::wstring& Value, bool)
{
    return Ztring().From_Unicode(Value.c_str());
}

static std::wstring Out_Wide(const String& Value, bool)
{
    return Ztring(Value).To_Unicode();
}
#endif //!defined(WSTRING_MISSING)

#if !defined(WSTRING_MISSING)
static String In_FileName(const std::wstring& Value, bool UTF8)
{
    return In(Value, UTF8);
}
#endif //!defined(WSTRING_MISSING)

//---------------------------------------------------------------------------
//CharSet option of MediaInfoA and MediaInfoListA, UTF-8 strings instead of the encoding of the locale, independent from the one of the C
//interface: Option() for this object, Option_Static() for the next objects and the static functions (the names up to the first NUL character)
static bool CharSet_UTF8=false;

static bool CharSet_Option(const std::string& Option, const std::string& Value, bool& UTF8)
{
    if (strcmp(Option.c_str(), "CharSet"))
        return false;
    UTF8=!strcmp(Value.c_str(), "UTF-8");
    return true;
}

namespace
{
    //Objects of MediaInfoA and MediaInfoListA, with their CharSet option
    class MediaInfoA_Object : public MediaInfo
    {
    public :
        bool UTF8;
        MediaInfoA_Object() : UTF8(CharSet_UTF8) {}
    };

    class MediaInfoListA_Object : public MediaInfoList
    {
    public :
        bool UTF8;
        MediaInfoListA_Object(size_t Count_Init) : MediaInfoList(Count_Init), UTF8(CharSet_UTF8) {}
    };
}

//---------------------------------------------------------------------------
//The functions of MediaInfo and MediaInfoList with the strings of the class _CLASS (_STRING, String from _OUT, UTF-8 strings if _UTF8), its
//object being an _OBJECT, except the options and the opening of a file
#define MEDIAINFO_STRINGS(_CLASS, _STRING, _OUT, _OBJECT, _UTF8) \
    _CLASS::_CLASS() : Object(new _OBJECT) {} \
    _CLASS::~_CLASS() {delete static_cast<_OBJECT*>(Object);} \
    size_t _CLASS::Open(const int8u* Begin, size_t Begin_Size, const int8u* End, size_t End_Size, int64u File_Size) {return Object->Open(Begin, Begin_Size, End, End_Size, File_Size);} \
    size_t _CLASS::Open_Buffer_Init(int64u File_Size, int64u File_Offset) {return Object->Open_Buffer_Init(File_Size, File_Offset);} \
    size_t _CLASS::Open_Buffer_Continue(const int8u* Buffer, size_t Buffer_Size) {return Object->Open_Buffer_Continue(Buffer, Buffer_Size);} \
    int64u _CLASS::Open_Buffer_Continue_GoTo_Get() {return Object->Open_Buffer_Continue_GoTo_Get();} \
    size_t _CLASS::Open_Buffer_Finalize() {return Object->Open_Buffer_Finalize();} \
    size_t _CLASS::Open_NextPacket() {return Object->Open_NextPacket();} \
    size_t _CLASS::Save() {return Object->Save();} \
    void _CLASS::Close() {Object->Close();} \
    _STRING _CLASS::Inform(size_t Reserved) {return _OUT(Object->Inform(Reserved), _UTF8);} \
    _STRING _CLASS::Get(stream_t StreamKind, size_t StreamNumber, size_t Parameter, info_t InfoKind) {return _OUT(Object->Get(StreamKind, StreamNumber, Parameter, InfoKind), _UTF8);} \
    _STRING _CLASS::Get(stream_t StreamKind, size_t StreamNumber, const _STRING &Parameter, info_t InfoKind, info_t SearchKind) {return _OUT(Object->Get(StreamKind, StreamNumber, In(Parameter, _UTF8), InfoKind, SearchKind), _UTF8);} \
    size_t _CLASS::Set(const _STRING &ToSet, stream_t StreamKind, size_t StreamNumber, size_t Parameter, const _STRING &OldValue) {return Object->Set(In(ToSet, _UTF8), StreamKind, StreamNumber, Parameter, In(OldValue, _UTF8));} \
    size_t _CLASS::Set(const _STRING &ToSet, stream_t StreamKind, size_t StreamNumber, const _STRING &Parameter, const _STRING &OldValue) {return Object->Set(In(ToSet, _UTF8), StreamKind, StreamNumber, In(Parameter, _UTF8), In(OldValue, _UTF8));} \
    size_t _CLASS::Output_Buffer_Get(const _STRING &Value) {return Object->Output_Buffer_Get(In(Value, _UTF8));} \
    size_t _CLASS::Output_Buffer_Get(size_t Pos) {return Object->Output_Buffer_Get(Pos);} \
    size_t _CLASS::State_Get() {return Object->State_Get();} \
    size_t _CLASS::Count_Get(stream_t StreamKind, size_t StreamNumber) {return Object->Count_Get(StreamKind, StreamNumber);}

#define MEDIAINFOLIST_STRINGS(_CLASS, _STRING, _OUT, _OBJECT, _UTF8) \
    _CLASS::_CLASS(size_t Count_Init) : Object(new _OBJECT(Count_Init)) {} \
    _CLASS::~_CLASS() {delete static_cast<_OBJECT*>(Object);} \
    size_t _CLASS::Open_Buffer_Init(int64u File_Size, int64u File_Offset) {return Object->Open_Buffer_Init(File_Size, File_Offset);} \
    size_t _CLASS::Open_Buffer_Continue(size_t FilePos, const int8u* Buffer, size_t Buffer_Size) {return Object->Open_Buffer_Continue(FilePos, Buffer, Buffer_Size);} \
    int64u _CLASS::Open_Buffer_Continue_GoTo_Get(size_t FilePos) {return Object->Open_Buffer_Continue_GoTo_Get(FilePos);} \
    size_t _CLASS::Open_Buffer_Finalize(size_t FilePos) {return Object->Open_Buffer_Finalize(FilePos);} \
    size_t _CLASS::Save(size_t FilePos) {return Object->Save(FilePos);} \
    void _CLASS::Close(size_t FilePos) {Object->Close(FilePos);} \
    _STRING _CLASS::Inform(size_t FilePos, size_t Reserved) {return _OUT(Object->Inform(FilePos, Reserved), _UTF8);} \
    _STRING _CLASS::Get(size_t FilePos, stream_t StreamKind, size_t StreamNumber, size_t Parameter, info_t KindOfInfo) {return _OUT(Object->Get(FilePos, StreamKind, StreamNumber, Parameter, KindOfInfo), _UTF8);} \
    _STRING _CLASS::Get(size_t FilePos, stream_t StreamKind, size_t StreamNumber, const _STRING &Parameter, info_t KindOfInfo, info_t KindOfSearch) {return _OUT(Object->Get(FilePos, StreamKind, StreamNumber, In(Parameter, _UTF8), KindOfInfo, KindOfSearch), _UTF8);} \
    size_t _CLASS::Set(const _STRING &ToSet, size_t FilePos, stream_t StreamKind, size_t StreamNumber, size_t Parameter, const _STRING &OldValue) {return Object->Set(In(ToSet, _UTF8), FilePos, StreamKind, StreamNumber, Parameter, In(OldValue, _UTF8));} \
    size_t _CLASS::Set(const _STRING &ToSet, size_t FilePos, stream_t StreamKind, size_t StreamNumber, const _STRING &Parameter, const _STRING &OldValue) {return Object->Set(In(ToSet, _UTF8), FilePos, StreamKind, StreamNumber, In(Parameter, _UTF8), In(OldValue, _UTF8));} \
    size_t _CLASS::State_Get() {return Object->State_Get();} \
    size_t _CLASS::Count_Get(size_t FilePos, stream_t StreamKind, size_t StreamNumber) {return Object->Count_Get(FilePos, StreamKind, StreamNumber);} \
    size_t _CLASS::Count_Get() {return Object->Count_Get();}

MEDIAINFO_STRINGS(MediaInfoA, std::string, Out_Narrow, MediaInfoA_Object, static_cast<MediaInfoA_Object*>(Object)->UTF8)
MEDIAINFOLIST_STRINGS(MediaInfoListA, std::string, Out_Narrow, MediaInfoListA_Object, static_cast<MediaInfoListA_Object*>(Object)->UTF8)
#if !defined(WSTRING_MISSING)
MEDIAINFO_STRINGS(MediaInfoW, std::wstring, Out_Wide, MediaInfo, false)
MEDIAINFOLIST_STRINGS(MediaInfoListW, std::wstring, Out_Wide, MediaInfoList, false)
#endif //!defined(WSTRING_MISSING)

//---------------------------------------------------------------------------
//Opening of a file: except on Windows, a narrow name in the encoding of the locale is the name as the system gives it (e.g. an argument of
//main()), a file which exists with this name is opened with it as it is (see MediaInfo_Internal::Open_Native(); the wide names of Windows
//are given as they are to the system from their text)
size_t MediaInfoA::Open(const std::string &File_Name)
{
    bool UTF8=static_cast<MediaInfoA_Object*>(Object)->UTF8;
    #if !defined(WINDOWS) && defined(MEDIAINFO_FILE_YES)
        if (!UTF8)
            return Object->Internal->Open_Native(File_Name.c_str());
    #endif //!defined(WINDOWS) && defined(MEDIAINFO_FILE_YES)
    return Object->Open(In_FileName(File_Name, UTF8));
}

size_t MediaInfoListA::Open(const std::string &File, const fileoptions_t Options)
{
    bool UTF8=static_cast<MediaInfoListA_Object*>(Object)->UTF8;
    #if !defined(WINDOWS)
        if (!UTF8)
            return Object->Internal->Open_Native(File.c_str(), Options);
    #endif //!defined(WINDOWS)
    return Object->Open(In_FileName(File, UTF8), Options);
}

#if !defined(WSTRING_MISSING)
size_t MediaInfoW::Open(const std::wstring &File_Name)
{
    return Object->Open(In_FileName(File_Name, false));
}

size_t MediaInfoListW::Open(const std::wstring &File, const fileoptions_t Options)
{
    return Object->Open(In_FileName(File, false), Options);
}
#endif //!defined(WSTRING_MISSING)

//---------------------------------------------------------------------------
//Options, with the CharSet option of MediaInfoA and MediaInfoListA
std::string MediaInfoA::Option(const std::string &Option, const std::string &Value)
{
    bool& UTF8=static_cast<MediaInfoA_Object*>(Object)->UTF8;
    if (CharSet_Option(Option, Value, UTF8))
        return std::string();
    return Out_Narrow(Object->Option(In(Option, UTF8), In(Value, UTF8)), UTF8);
}

std::string MediaInfoA::Option_Static(const std::string &Option, const std::string &Value)
{
    if (CharSet_Option(Option, Value, CharSet_UTF8))
        return std::string();
    return Out_Narrow(MediaInfo::Option_Static(In(Option, CharSet_UTF8), In(Value, CharSet_UTF8)), CharSet_UTF8);
}

std::string MediaInfoListA::Option(const std::string &Option, const std::string &Value)
{
    bool& UTF8=static_cast<MediaInfoListA_Object*>(Object)->UTF8;
    if (CharSet_Option(Option, Value, UTF8))
        return std::string();
    return Out_Narrow(Object->Option(In(Option, UTF8), In(Value, UTF8)), UTF8);
}

std::string MediaInfoListA::Option_Static(const std::string &Option, const std::string &Value)
{
    if (CharSet_Option(Option, Value, CharSet_UTF8))
        return std::string();
    return Out_Narrow(MediaInfoList::Option_Static(In(Option, CharSet_UTF8), In(Value, CharSet_UTF8)), CharSet_UTF8);
}

#if !defined(WSTRING_MISSING)
std::wstring MediaInfoW::Option(const std::wstring &Option, const std::wstring &Value)
{
    return Out_Wide(Object->Option(In(Option, false), In(Value, false)), false);
}

std::wstring MediaInfoW::Option_Static(const std::wstring &Option, const std::wstring &Value)
{
    return Out_Wide(MediaInfo::Option_Static(In(Option, false), In(Value, false)), false);
}

std::wstring MediaInfoListW::Option(const std::wstring &Option, const std::wstring &Value)
{
    return Out_Wide(Object->Option(In(Option, false), In(Value, false)), false);
}

std::wstring MediaInfoListW::Option_Static(const std::wstring &Option, const std::wstring &Value)
{
    return Out_Wide(MediaInfoList::Option_Static(In(Option, false), In(Value, false)), false);
}
#endif //!defined(WSTRING_MISSING)

} //NameSpace
