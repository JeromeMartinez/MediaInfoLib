/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

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
#include "MediaInfo/MediaInfo_Conformance.h"
#include "MediaInfo/MediaInfo_Config.h"
#include "ThirdParty/fmt/format.h"
#include <cstring>
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

namespace conformance
{

//***************************************************************************
// Catalog
//***************************************************************************

#if MEDIAINFO_CONFORMANCE
//---------------------------------------------------------------------------
static const message Messages[]=
{
    {"None", Conformance_Error, "", ""},
    #define CONFORMANCE_MESSAGE(_Id, _Level, _Field, _Text) {#_Id, _Level, _Field, _Text},
    #define CONFORMANCE_MESSAGE_ALWAYS(_Id, _Level, _Field, _Text) {#_Id, _Level, _Field, _Text},
    #include "MediaInfo/MediaInfo_Conformance_List.h"
    #undef CONFORMANCE_MESSAGE_ALWAYS
    #undef CONFORMANCE_MESSAGE
};
static_assert(sizeof(Messages)/sizeof(Messages[0])==Max, "");

//---------------------------------------------------------------------------
const message& Message_Get(id Id)
{
    if (Id>=Max)
        return Messages[None];
    return Messages[Id];
}
#else //MEDIAINFO_CONFORMANCE
//---------------------------------------------------------------------------
// Without conformance support, only the messages also filled without it
struct message_always
{
    id                  Id;
    message             Message;
};
static const message_always Messages[]=
{
    {None, {"None", Conformance_Error, "", ""}},
    #define CONFORMANCE_MESSAGE(_Id, _Level, _Field, _Text)
    #define CONFORMANCE_MESSAGE_ALWAYS(_Id, _Level, _Field, _Text) {_Id, {#_Id, _Level, _Field, _Text}},
    #include "MediaInfo/MediaInfo_Conformance_List.h"
    #undef CONFORMANCE_MESSAGE_ALWAYS
    #undef CONFORMANCE_MESSAGE
};

//---------------------------------------------------------------------------
const message& Message_Get(id Id)
{
    //Same order as the identifiers when all messages are in the catalog
    const auto Pos=(size_t)Id-1;
    if (Id && Pos<sizeof(Messages)/sizeof(*Messages) && Messages[Pos].Id==Id)
        return Messages[Pos].Message;
    for (const auto& Item : Messages)
        if (Item.Id==Id)
            return Item.Message;
    return Messages[0].Message;
}
#endif //MEDIAINFO_CONFORMANCE

//***************************************************************************
// Formatting
//***************************************************************************

//---------------------------------------------------------------------------
std::string arg::To_String() const
{
    switch (Type)
    {
        case Type_Signed    : return std::to_string(Signed);
        case Type_Unsigned  : return std::to_string(Unsigned);
        case Type_Float     : return fmt::format("{}", Float);
        default             : return String;
    }
}

//---------------------------------------------------------------------------
static std::string To_String(const arg& Arg, const std::string& Spec)
{
    if (Spec.empty())
        return Arg.To_String();
    const auto Format="{:"+Spec+'}';
    FMT_TRY
    {
        switch (Arg.Type)
        {
            case arg::Type_Signed   : return fmt::format(fmt::runtime(Format), Arg.Signed);
            case arg::Type_Unsigned : return fmt::format(fmt::runtime(Format), Arg.Unsigned);
            case arg::Type_Float    : return fmt::format(fmt::runtime(Format), Arg.Float);
            default                 : return fmt::format(fmt::runtime(Format), Arg.String);
        }
    }
    FMT_CATCH(...)
    {
    }
    return Arg.To_String(); //Format specification not compatible with the value
}

//---------------------------------------------------------------------------
// Part of a template, returns false if an argument is missing
static bool Format(const char* Template, const char* Template_End, const args& Args, std::string& Result)
{
    bool IsComplete=true;
    for (auto C=Template; C<Template_End; C++)
    {
        if (*C=='[' && C+1<Template_End && C[1]=='?')
        {
            //Optional part, kept only if all its arguments are given
            const auto End=(const char*)std::memchr(C, ']', Template_End-C);
            if (End)
            {
                std::string Part;
                if (Format(C+2, End, Args, Part))
                    Result+=Part;
                C=End;
                continue;
            }
        }
        if (*C=='{')
        {
            if (C+1<Template_End && C[1]=='{')
            {
                Result+='{';
                C++;
                continue;
            }
            const auto End=(const char*)std::memchr(C, '}', Template_End-C);
            if (!End)
            {
                Result.append(C, Template_End);
                break;
            }
            auto Name_End=(const char*)std::memchr(C, ':', End-C);
            std::string Spec;
            if (Name_End)
                Spec.assign(Name_End+1, End);
            else
                Name_End=End;
            const size_t Name_Size=Name_End-(C+1);
            const arg* Arg=nullptr;
            for (const auto& Item : Args)
                if (Item.Name.size()==Name_Size && !Item.Name.compare(0, Name_Size, C+1, Name_Size))
                {
                    Arg=&Item;
                    break;
                }
            if (Arg)
                Result+=To_String(*Arg, Spec);
            else
            {
                Result.append(C, End+1); //Missing argument, the template is kept as is
                IsComplete=false;
            }
            C=End;
            continue;
        }
        if (*C=='}' && C+1<Template_End && C[1]=='}')
            C++;
        Result+=*C;
    }
    return IsComplete;
}

//---------------------------------------------------------------------------
std::string Format(const char* Template, const args& Args)
{
    std::string Result;
    if (Template)
        Format(Template, Template+std::strlen(Template), Args, Result);
    return Result;
}

//---------------------------------------------------------------------------
std::string Field_Get(id Id, const args& Args)
{
    const auto Field=Format(Message_Get(Id).Field, Args);

    //Sub-fields are separated by one space, an empty argument does not create an empty sub-field
    std::string Result;
    Result.reserve(Field.size());
    for (const auto C : Field)
    {
        if (C==' ' && (Result.empty() || Result.back()==' '))
            continue;
        Result+=C;
    }
    if (!Result.empty() && Result.back()==' ')
        Result.pop_back();
    return Result;
}

//---------------------------------------------------------------------------
std::string Text_Get(id Id, const args& Args)
{
    return Format(Message_Get(Id).Text, Args);
}

#if MEDIAINFO_CONFORMANCE
//***************************************************************************
// Structured data
//***************************************************************************

//---------------------------------------------------------------------------
// Serialized form: name, then items starting with "\x1E" and a letter: 't' + text, 'a' + argument name + "\x1F" + argument value for each argument,
// 's' + specification, 'c' + specification of the additional constraint, 'i' + stream ID,
// 'l' + the values of a location separated by "\x1F" for each location, 'm' if some locations are not listed
// Messages sharing a field are separated by "\x1D"
static const char Separator_Message='\x1D';
static const char Separator_Item='\x1E';
static const char Separator_Value='\x1F';

//---------------------------------------------------------------------------
static void Data_Append(std::string& Result, const std::string& Value)
{
    //Line breaks are replaced as in the field values (see File__Analyze::Fill())
    const auto Begin=Result.size();
    const auto CarriageReturnReplace=MediaInfoLib::Config.CarriageReturnReplace_Get().To_UTF8();
    bool IsReplaced=false;
    for (size_t i=0; i<Value.size(); i++)
    {
        const auto C=Value[i];
        if ((C=='\r' || C=='\n') && !CarriageReturnReplace.empty())
        {
            if (C=='\r' && i+1<Value.size() && Value[i+1]=='\n')
                i++;
            Result+=CarriageReturnReplace;
            IsReplaced=true;
            continue;
        }
        Result+=(C==Separator_Message || C==Separator_Item || C==Separator_Value)?'?':C;
    }
    if (IsReplaced && Result.size()-Begin>=CarriageReturnReplace.size() && !Result.compare(Result.size()-CarriageReturnReplace.size(), CarriageReturnReplace.size(), CarriageReturnReplace))
        Result.resize(Result.size()-CarriageReturnReplace.size());
}

//---------------------------------------------------------------------------
std::string Data_Serialize(const data& Data)
{
    std::string Result;
    Data_Append(Result, Data.Name);
    Result+=Separator_Item;
    Result+='t';
    Data_Append(Result, Data.Text);
    for (const auto& Arg : Data.Args)
    {
        Result+=Separator_Item;
        Result+='a';
        Data_Append(Result, Arg.first);
        Result+=Separator_Value;
        Data_Append(Result, Arg.second);
    }
    const auto Append=[&](char Kind, const std::string& Value)
    {
        if (Value.empty())
            return;
        Result+=Separator_Item;
        Result+=Kind;
        Data_Append(Result, Value);
    };
    Append('s', Data.Specification);
    Append('c', Data.AdditionalConstraint);
    Append('i', Data.StreamID);
    for (const auto& Location : Data.Locations)
    {
        Result+=Separator_Item;
        Result+='l';
        Result+=Location.IsConfiguration?'c':'f';
        for (const auto Value : {&Location.Frame, &Location.Frame_Last, &Location.SubFrame, &Location.SubFrame_Last, &Location.Time, &Location.Time_Last, &Location.Offset, &Location.Offset_Last})
        {
            Result+=Separator_Value;
            Data_Append(Result, *Value);
        }
    }
    if (Data.Locations_More)
    {
        Result+=Separator_Item;
        Result+='m';
    }
    return Result;
}

//---------------------------------------------------------------------------
static bool Data_Parse_One(const std::string& Serialized, data& Data)
{
    Data=data();
    if (Serialized.empty())
        return false;
    size_t Begin=0;
    for (;;)
    {
        auto End=Serialized.find(Separator_Item, Begin);
        if (End==std::string::npos)
            End=Serialized.size();
        if (!Begin)
            Data.Name.assign(Serialized, 0, End);
        else if (End>Begin && Serialized[Begin]=='t')
            Data.Text.assign(Serialized, Begin+1, End-(Begin+1));
        else if (End>Begin)
        {
            const auto Kind=Serialized[Begin];
            std::vector<std::string> Values;
            size_t Value_Begin=Begin+1;
            for (;;)
            {
                auto Value_End=Serialized.find(Separator_Value, Value_Begin);
                if (Value_End==std::string::npos || Value_End>End)
                    Value_End=End;
                Values.emplace_back(Serialized, Value_Begin, Value_End-Value_Begin);
                if (Value_End==End)
                    break;
                Value_Begin=Value_End+1;
            }
            switch (Kind)
            {
                case 'a':
                    Values.resize(2);
                    Data.Args.emplace_back(Values[0], Values[1]);
                    break;
                case 's':
                    Data.Specification=Values[0];
                    break;
                case 'c':
                    Data.AdditionalConstraint=Values[0];
                    break;
                case 'i':
                    Data.StreamID=Values[0];
                    break;
                case 'l':
                {
                    Values.resize(9);
                    location Location;
                    Location.IsConfiguration=Values[0]=="c";
                    Location.Frame=Values[1];
                    Location.Frame_Last=Values[2];
                    Location.SubFrame=Values[3];
                    Location.SubFrame_Last=Values[4];
                    Location.Time=Values[5];
                    Location.Time_Last=Values[6];
                    Location.Offset=Values[7];
                    Location.Offset_Last=Values[8];
                    Data.Locations.push_back(std::move(Location));
                    break;
                }
                case 'm':
                    Data.Locations_More=true;
                    break;
                default:;
            }
        }
        if (End==Serialized.size())
            break;
        Begin=End+1;
    }
    return !Data.Name.empty();
}

//---------------------------------------------------------------------------
std::vector<data> Data_Parse(const std::string& Serialized)
{
    std::vector<data> Result;
    size_t Begin=0;
    while (Begin<Serialized.size())
    {
        auto End=Serialized.find(Separator_Message, Begin);
        if (End==std::string::npos)
            End=Serialized.size();
        data Data;
        if (Data_Parse_One(Serialized.substr(Begin, End-Begin), Data))
            Result.push_back(std::move(Data));
        Begin=End+1;
    }
    return Result;
}

//***************************************************************************
// Catalog as text
//***************************************************************************

//---------------------------------------------------------------------------
static void Csv_Append(std::string& Result, const char* Value)
{
    if (!std::strpbrk(Value, ",\"\r\n"))
    {
        Result+=Value;
        return;
    }
    Result+='"';
    for (auto C=Value; *C; C++)
    {
        if (*C=='"')
            Result+='"';
        Result+=*C;
    }
    Result+='"';
}

//---------------------------------------------------------------------------
std::string Catalog_Get()
{
    static const char* Level_Names[]=
    {
        "Error",
        "Warning",
        "Information",
    };
    static_assert(sizeof(Level_Names)/sizeof(Level_Names[0])==Conformance_Max, "");

    std::string Result("Identifier,Level,Field,Text\n");
    for (size_t i=None+1; i<Max; i++)
    {
        const auto& Message=Messages[i];
        Csv_Append(Result, Message.Name);
        Result+=',';
        Csv_Append(Result, Level_Names[Message.Level]);
        Result+=',';
        Csv_Append(Result, Message.Field);
        Result+=',';
        Csv_Append(Result, Message.Text);
        Result+='\n';
    }
    return Result;
}
#endif //MEDIAINFO_CONFORMANCE

} //NameSpace conformance

} //NameSpace
