/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Conformance messages: identifiers, arguments, catalog access
//
// Each message has an identifier, a level, a field and a text, all listed
// in MediaInfo_Conformance.cpp (the catalog). The field and the text are
// templates in the fmtlib syntax with named arguments, e.g.
// "File size is less than expected size (actual {actual}, expected {expected})".
// Parsers provide the identifier and the values of the arguments, the library
// builds the text output from the templates and keeps the identifier and the
// arguments for the structured (XML, JSON) output.
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#ifndef MediaInfo_ConformanceH
#define MediaInfo_ConformanceH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
#include "ZenLib/Ztring.h"
#include <string>
#include <type_traits>
#include <vector>
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//---------------------------------------------------------------------------
enum conformance_type {
    Conformance_Error,
    Conformance_Warning,
    Conformance_Information,
    Conformance_Max,
};

namespace conformance
{

//***************************************************************************
// Message identifiers
//***************************************************************************

// Naming: <Format>_<Subject>_<Problem>, see the rules in MediaInfo_Conformance_List.h
enum id : ZenLib::int16u
{
    None,
    #define CONFORMANCE_MESSAGE(_Id, _Level, _Field, _Text) _Id,
    #define CONFORMANCE_MESSAGE_ALWAYS(_Id, _Level, _Field, _Text) _Id,
    #include "MediaInfo/MediaInfo_Conformance_List.h"
    #undef CONFORMANCE_MESSAGE_ALWAYS
    #undef CONFORMANCE_MESSAGE
    Max
};

//***************************************************************************
// Arguments
//***************************************************************************

// Named value used by the field or text template of a message
struct arg
{
    enum type : ZenLib::int8u
    {
        Type_String,
        Type_Signed,
        Type_Unsigned,
        Type_Float,
    };

    std::string         Name;
    type                Type;
    ZenLib::int64s      Signed=0;
    ZenLib::int64u      Unsigned=0;
    ZenLib::float64     Float=0;
    std::string         String;

    arg(std::string Name_, const char* Value) : Name(std::move(Name_)), Type(Type_String), String(Value?Value:"") {}
    arg(std::string Name_, const std::string& Value) : Name(std::move(Name_)), Type(Type_String), String(Value) {}
    arg(std::string Name_, std::string&& Value) : Name(std::move(Name_)), Type(Type_String), String(std::move(Value)) {}
    arg(std::string Name_, const ZenLib::Ztring& Value) : Name(std::move(Name_)), Type(Type_String), String(Value.To_UTF8()) {}
    arg(std::string Name_, char Value) : Name(std::move(Name_)), Type(Type_String), String(1, Value) {}
    template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type=0>
    arg(std::string Name_, T Value) : Name(std::move(Name_)), Type(Type_Signed), Signed((ZenLib::int64s)Value) {}
    template<typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_signed<T>::value, int>::type=0>
    arg(std::string Name_, T Value) : Name(std::move(Name_)), Type(Type_Unsigned), Unsigned((ZenLib::int64u)Value) {}
    template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type=0>
    arg(std::string Name_, T Value) : Name(std::move(Name_)), Type(Type_Float), Float((ZenLib::float64)Value) {}

    // Value without format specification, also used by the structured output
    std::string         To_String() const;
};
typedef std::vector<arg> args;

//***************************************************************************
// Catalog
//***************************************************************************

// Without conformance support, the catalog has only the messages also filled without it (CONFORMANCE_MESSAGE_ALWAYS)
struct message
{
    const char*         Name;   // Identifier as text, e.g. "General_FileSize_LessThanExpected"
    conformance_type    Level;  // Default level
    const char*         Field;  // Template of the field, sub-fields separated by a space
    const char*         Text;   // Template of the text
};

// Description of a message
const message&          Message_Get(id Id);

// Template with the arguments, "{name}" or "{name:format_spec}" (fmtlib syntax) replaced by the value of the argument,
// "[?...]" is an optional part, kept only if all its arguments are given
std::string             Format(const char* Template, const args& Args);

// Field of a message, without the spaces of empty arguments
std::string             Field_Get(id Id, const args& Args);

// Text of a message
std::string             Text_Get(id Id, const args& Args);

#if MEDIAINFO_CONFORMANCE
// Structured data attached to a conformance field: identifier, text, arguments, context and locations of a message
struct location
{
    bool                IsConfiguration=false; // The message is about the configuration of the stream, not about a frame
    std::string         Frame;          // Frame number, first one if the location is a range of frames
    std::string         Frame_Last;     // Last frame of the range
    std::string         SubFrame;       // Position inside the frame, e.g. of a frame carried by the frame
    std::string         SubFrame_Last;
    std::string         Time;           // Seconds, first one if the location is a range
    std::string         Time_Last;
    std::string         Offset;         // Bytes, first one if the location is a range
    std::string         Offset_Last;
};
struct data
{
    std::string         Name;           // Identifier as text
    std::string         Text;           // Text of the message, without the items below
    std::vector<std::pair<std::string, std::string> > Args;
    std::string         Specification;  // Specification the message comes from, when the stream is checked against several ones (e.g. a profile)
    std::string         AdditionalConstraint; // Specification adding this constraint to the main one
    std::string         StreamID;       // ID of the stream the message is about, when it is not in the stream of the field
    std::vector<location> Locations;
    bool                Locations_More=false; // Some locations are not listed
};
std::string             Data_Serialize(const data& Data);
std::vector<data>       Data_Parse(const std::string& Serialized); // Several messages may share a field

// Catalog as text (one line per message: identifier, level, field, text), for the "Info_Conformance" option
std::string             Catalog_Get();
#endif //MEDIAINFO_CONFORMANCE

} //NameSpace conformance

} //NameSpace

#endif
