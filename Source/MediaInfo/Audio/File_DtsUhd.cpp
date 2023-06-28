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
#if defined(MEDIAINFO_DTSUHD_YES)
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Audio/File_DtsUhd.h"
#include "ZenLib/Utils.h"
#include "ZenLib/BitStream.h"
#include "MediaInfo/MediaInfo_Config_MediaInfo.h"
#if MEDIAINFO_EVENTS
    #include "MediaInfo/MediaInfo_Events.h"
#endif //MEDIAINFO_EVENTS
#include <algorithm>
using namespace ZenLib;
using namespace std;
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

#define CHUNK_AUPR_HDR 0x415550522D484452LL
#define CHUNK_DTSHDHDR 0x4454534844484452LL
#define CHUNK_STRMDATA 0x5354524D44415441LL
#define DTSUHD_NONSYNCWORD 0x71C442E8
#define DTSUHD_SYNCWORD    0x40411BF2

/* Return codes from dtsuhd_frame */
enum DTSUHDStatus {
    DTSUHD_OK,
    DTSUHD_INCOMPLETE,    /* Entire frame not in buffer. */
    DTSUHD_INVALID_FRAME, /* Error parsing frame. */
    DTSUHD_NOSYNC,        /* No sync frame prior to non-sync frame. */
    DTSUHD_NULL,          /* Function parameter may not be NULL. */
};

enum RepType {
    REP_TYPE_CH_MASK_BASED,
    REP_TYPE_MTRX2D_CH_MASK_BASED,
    REP_TYPE_MTRX3D_CH_MASK_BASED,
    REP_TYPE_BINAURAL,
    REP_TYPE_AMBISONIC,
    REP_TYPE_AUDIO_TRACKS,
    REP_TYPE_3D_OBJECT_SINGLE_SRC_PER_WF,
    REP_TYPE_3D_MONO_OBJECT_SINGLE_SRC_PER_WF,
};

static int CountBits(int32u Mask)
{
    int Count=0;
    for (; Mask; Mask>>=1)
        Count+=Mask&1;
    return Count;
}

/** Read the indicated number (count) of bits, advancing 'bit' the number of
    bits read.  On an end of buffer condition, return 0 and allow the bit
    position to increment past the buffer end to prevent subsequent calls
    from returning anything other than zero.
*/
int32u File_DtsUhd::ReadBits(const int8u* Buffer, int Size, int* Bit, int Count, const char* Name)
{
    int i;
    int n;
    int64u Value=0;

    if (*Bit+Count<=Size*8)
    {
        /* Read the bytes containing the wanted bits into the result. */
        Buffer+=*Bit>>3;
        for (i=0, n=Count+(*Bit&7); i<n; i+=8)
            Value=(Value<<8)|*Buffer++;

        /* Left shift the unwanted high bits out of the result. */
        Value<<=64-i+(*Bit&7);

        /* Right align result, which now has no unwanted high bits. */
        Value>>=64-Count;
    }
    *Bit+=Count;

    #if MEDIAINFO_TRACE
    if (Trace_Activated)
        Param(Name?Name:"?", Value, Count);
    #endif
    return (int32u)Value;
}

/* Read from the MD01 buffer (if present), falling back to the frame buffer */
int32u File_DtsUhd::ReadBitsMD01(MD01* MD01, int Bits, const char* Name)
{
    if (MD01->Buffer.empty())
        return ReadBits(FrameStart, FrameSize, &FrameBit, Bits, Name);
    return ReadBits(MD01->Buffer.data(), MD01->Buffer.size(), &MD01->Bit, Bits, Name);
}

/** Read a variable number of bits from a buffer.
    In the ETSI TS 103 491 V1.2.1 specification, the pseudo code defaults
    the 'add' parameter to true.  Table 7-30 shows passing an explicit false,
    most other calls do not pass the extractAndAdd parameter.  This function
    is based on code in Table 5-2

*/
int32u File_DtsUhd::ReadBitsVar(const int8u* Buffer, int Size, int *Bit, const uint8_t Table[], const char* Name)
{
    Element_Begin1(Name?Name:"?");
    static const int BitsUsed[8] = {1, 1, 1, 1, 2, 2, 3, 3};
    static const int IndexTable[8] = { 0, 0, 0, 0, 1, 1, 2, 3 };

    int Code = ReadBits(Buffer, Size, Bit, 3, "index"); /* value range is [0, 7] */
    *Bit-=3-BitsUsed[Code];
    int Index=IndexTable[Code];
    uint32_t Value=0;

    if (Table[Index]>0)
    {
        for (int i=0; i<Index; i++)
            Value+=1<<Table[i];
        Value+=ReadBits(Buffer, Size, Bit, Table[Index], "addition");
    }

    Element_Info1(Value);
    Element_End0();
    return Value;
}

void File_DtsUhd::SkipBits(int* Bit, int Count, const char* Name)
{
    *Bit+=Count;
    #if MEDIAINFO_TRACE
    if (Trace_Activated && Count>0)
        Param(Name, Ztring("(")+Ztring::ToZtring(Count)+Ztring(" bits)"));
    #endif
}

void File_DtsUhd::Get_VR(const uint8_t Table[], int32u& Value, const char* Name)
{
    Element_Begin1(Name?Name:"?");
    static const uint8_t BitsUsed[8] = {1, 1, 1, 1, 2, 2, 3, 3};
    static const uint8_t IndexTable[8] = { 0, 0, 0, 0, 1, 1, 2, 3 };

    uint8_t Code;
    Get_S1 (3, Code,                                            "index"); /* value range is [0, 7] */
    auto Index=IndexTable[Code];
    Value=0;
    if (Table[Index]>0)
    {
        for (int i=0; i<Index; i++)
            Value+=1<<Table[i];
        uint32_t Add;
        Get_S4 (Table[Index], Add,                              "addition");
        Value+=Add;
    }

    Element_Info1(Value);
    Element_End0();
}

/* Implied by Table 6-2, MD01 chunk objects appended in for loop */
File_DtsUhd::MD01* File_DtsUhd::ChunkAppendMD01(int Id)
{
    MD01List.push_back(MD01());
    MD01List.back().ChunkId=Id;
    return &MD01List.back();
}

/* Return existing MD01 chunk based on chunkID */
File_DtsUhd::MD01* File_DtsUhd::ChunkFindMD01(int Id)
{
    for (auto& MD01 : MD01List)
        if (Id==MD01.ChunkId)
            return &MD01;
    return nullptr;
}

File_DtsUhd::MDObject* File_DtsUhd::FindDefaultAudio()
{
    for (auto& MD01 : MD01List)
    {
        int ObjIndex=-1;
        for (int i=0; i<257; i++)
        {
            MDObject* Object = MD01.Object+i;
            if (Object->Started && AudPresParam[Object->PresIndex].Selectable)
            {
                if (ObjIndex < 0 || (Object->PresIndex < MD01.Object[ObjIndex].PresIndex))
                    ObjIndex = i;
            }
        }
        if (ObjIndex>=0)
            return MD01.Object+ObjIndex;
    }

    return nullptr;
}

/* Save channel mask, count, and rep type to descriptor info.
   ETSI TS 103 491 Table 7-28 channel activity mask bits
   mapping and SCTE DVS 243-4 Rev. 0.2 DG X Table 4.  Convert activity mask and
   representation type to channel mask and channel counts.
*/
void File_DtsUhd::ExtractObjectInfo(MDObject* Object)
{
    if (!Object)
        return;

    constexpr struct
    {
        int32u ActivityMask, ChannelMask; // ChannelMask is defined by ETSI TS 103 491
    }
    ActivityMap[] = {
        // act mask | chan mask | ffmpeg channel mask
        { 0x000001, 0x00000001 },
        { 0x000002, 0x00000006 },
        { 0x000004, 0x00000018 },
        { 0x000008, 0x00000020 },
        { 0x000010, 0x00000040 },
        { 0x000020, 0x0000A000 },
        { 0x000040, 0x00000180 },
        { 0x000080, 0x00004000 },
        { 0x000100, 0x00080000 },
        { 0x000200, 0x00001800 },
        { 0x000400, 0x00060000 },
        { 0x000800, 0x00000600 },
        { 0x001000, 0x00010000 },
        { 0x002000, 0x00300000 },
        { 0x004000, 0x00400000 },
        { 0x008000, 0x01800000 },
        { 0x010000, 0x02000000 },
        { 0x020000, 0x0C000000 },
        { 0x140000, 0x30000000 },
        { 0x080000, 0xC0000000 },
        { 0 } // Terminator
    };

    for (int i = 0; ActivityMap[i].ActivityMask; i++)
        if (ActivityMap[i].ActivityMask & Object->ChActivityMask)
            FrameDescriptor.ChannelMask |= ActivityMap[i].ChannelMask;

    FrameDescriptor.ChannelCount = CountBits(FrameDescriptor.ChannelMask);
    FrameDescriptor.RepType = Object->RepType;
}

/* Assemble information for MP4 Sample Entry box.  Sample Size is always
   16 bits.  The coding name is the name of the SampleEntry sub-box and is
   'dtsx' unless the version of the bitstream is > 2.
   If DecoderProfile == 2, then MaxPayloadCode will be zero.
*/
void File_DtsUhd::UpdateDescriptor()
{
    FrameDescriptor.ChannelMask=0;
    FrameDescriptor.RepType=0;
    ExtractObjectInfo(FindDefaultAudio());

    /* 6.3.6.9: audio frame duration may be a fraction of metadata frame duration. */
    int Fraction=1;
    for (const auto& Navi : NaviList)
    {
        if (Navi.Present)
        {
            if (Navi.Id==3)
                Fraction=2;
            else if (Navi.Id==4)
                Fraction=4;
        }
    }

    FrameDescriptor.BaseSampleFreqCode=SampleRate==48000;
    FrameDescriptor.ChannelCount=CountBits(FrameDescriptor.ChannelMask);
    FrameDescriptor.DecoderProfileCode=StreamMajorVerNum-2;
    FrameDescriptor.MaxPayloadCode=0+(StreamMajorVerNum>=2);
    FrameDescriptor.NumPresCode=NumAudioPres-1;
    FrameDescriptor.SampleCount=(FrameDuration*SampleRate)/(ClockRateInHz*Fraction);
}

/* Table 6-17 p47 */
int File_DtsUhd::ExtractExplicitObjectsLists(int DepAuPresExplObjListMask, int CurrentAuPresInd)
{
    Element_Begin1("ExtractExplicitObjectsLists");
    constexpr int8u Table[4] = {4, 8, 16, 32};

    for (int i=0; i<CurrentAuPresInd; i++)
        if ((DepAuPresExplObjListMask>>i)&1)
            if (SyncFrameFlag||ReadBits(FrameStart, FrameSize, &FrameBit, 1, "UpdFlag"))
                ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "Temp");

    Element_End0();
    return 0;
}

/* Table 6-15 p44, Table 6-16 p45 */
int File_DtsUhd::ResolveAudPresParams()
{
    Element_Begin1("ResolveAudPresParams");
    constexpr uint8_t Table[4] = {0, 2, 4, 5};

    if (SyncFrameFlag)
    {
        if (FullChannelBasedMixFlag)
            NumAudioPres=1;
        else
            NumAudioPres=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "NumAudioPres") + 1;
        memset(AudPresParam, 0, sizeof(AudPresParam[0])*NumAudioPres);
    }

    for (int AuPresInd=0; AuPresInd<NumAudioPres; AuPresInd++)
    {
        if (SyncFrameFlag)
        {
            if (FullChannelBasedMixFlag)
                AudPresParam[AuPresInd].Selectable=true;
            else
                AudPresParam[AuPresInd].Selectable=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "AudPresParam[AuPresInd].Selectable?");
        }

        if (AudPresParam[AuPresInd].Selectable)
        {
            if (SyncFrameFlag)
            {
                int DepAuPresMask = (AuPresInd>0)?ReadBits(FrameStart, FrameSize, &FrameBit, AuPresInd, "DepAuPresMask") : 0;
                AudPresParam[AuPresInd].DepAuPresMask = 0;
                for (int i = 0; DepAuPresMask; i++, DepAuPresMask>>=1)
                    if (DepAuPresMask&1)
                        AudPresParam[AuPresInd].DepAuPresMask|=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "AudPresParam[AuPresInd].DepAuPresMask") << i;
            }

            if (ExtractExplicitObjectsLists(AudPresParam[AuPresInd].DepAuPresMask, AuPresInd))
                return 1;
        }
        else
            AudPresParam[AuPresInd].DepAuPresMask=0;
    }

    Element_End0();
    return 0;
}

//---------------------------------------------------------------------------
static const int16u CRC_CCIT_Table[256]=
{
    0x0000, 0x2110, 0x4220, 0x6330, 0x8440, 0xA550, 0xC660, 0xE770,
    0x0881, 0x2991, 0x4AA1, 0x6BB1, 0x8CC1, 0xADD1, 0xCEE1, 0xEFF1,
    0x3112, 0x1002, 0x7332, 0x5222, 0xB552, 0x9442, 0xF772, 0xD662,
    0x3993, 0x1883, 0x7BB3, 0x5AA3, 0xBDD3, 0x9CC3, 0xFFF3, 0xDEE3,
    0x6224, 0x4334, 0x2004, 0x0114, 0xE664, 0xC774, 0xA444, 0x8554,
    0x6AA5, 0x4BB5, 0x2885, 0x0995, 0xEEE5, 0xCFF5, 0xACC5, 0x8DD5,
    0x5336, 0x7226, 0x1116, 0x3006, 0xD776, 0xF666, 0x9556, 0xB446,
    0x5BB7, 0x7AA7, 0x1997, 0x3887, 0xDFF7, 0xFEE7, 0x9DD7, 0xBCC7,
    0xC448, 0xE558, 0x8668, 0xA778, 0x4008, 0x6118, 0x0228, 0x2338,
    0xCCC9, 0xEDD9, 0x8EE9, 0xAFF9, 0x4889, 0x6999, 0x0AA9, 0x2BB9,
    0xF55A, 0xD44A, 0xB77A, 0x966A, 0x711A, 0x500A, 0x333A, 0x122A,
    0xFDDB, 0xDCCB, 0xBFFB, 0x9EEB, 0x799B, 0x588B, 0x3BBB, 0x1AAB,
    0xA66C, 0x877C, 0xE44C, 0xC55C, 0x222C, 0x033C, 0x600C, 0x411C,
    0xAEED, 0x8FFD, 0xECCD, 0xCDDD, 0x2AAD, 0x0BBD, 0x688D, 0x499D,
    0x977E, 0xB66E, 0xD55E, 0xF44E, 0x133E, 0x322E, 0x511E, 0x700E,
    0x9FFF, 0xBEEF, 0xDDDF, 0xFCCF, 0x1BBF, 0x3AAF, 0x599F, 0x788F,
    0x8891, 0xA981, 0xCAB1, 0xEBA1, 0x0CD1, 0x2DC1, 0x4EF1, 0x6FE1,
    0x8010, 0xA100, 0xC230, 0xE320, 0x0450, 0x2540, 0x4670, 0x6760,
    0xB983, 0x9893, 0xFBA3, 0xDAB3, 0x3DC3, 0x1CD3, 0x7FE3, 0x5EF3,
    0xB102, 0x9012, 0xF322, 0xD232, 0x3542, 0x1452, 0x7762, 0x5672,
    0xEAB5, 0xCBA5, 0xA895, 0x8985, 0x6EF5, 0x4FE5, 0x2CD5, 0x0DC5,
    0xE234, 0xC324, 0xA014, 0x8104, 0x6674, 0x4764, 0x2454, 0x0544,
    0xDBA7, 0xFAB7, 0x9987, 0xB897, 0x5FE7, 0x7EF7, 0x1DC7, 0x3CD7,
    0xD326, 0xF236, 0x9106, 0xB016, 0x5766, 0x7676, 0x1546, 0x3456,
    0x4CD9, 0x6DC9, 0x0EF9, 0x2FE9, 0xC899, 0xE989, 0x8AB9, 0xABA9,
    0x4458, 0x6548, 0x0678, 0x2768, 0xC018, 0xE108, 0x8238, 0xA328,
    0x7DCB, 0x5CDB, 0x3FEB, 0x1EFB, 0xF98B, 0xD89B, 0xBBAB, 0x9ABB,
    0x754A, 0x545A, 0x376A, 0x167A, 0xF10A, 0xD01A, 0xB32A, 0x923A,
    0x2EFD, 0x0FED, 0x6CDD, 0x4DCD, 0xAABD, 0x8BAD, 0xE89D, 0xC98D,
    0x267C, 0x076C, 0x645C, 0x454C, 0xA23C, 0x832C, 0xE01C, 0xC10C,
    0x1FEF, 0x3EFF, 0x5DCF, 0x7CDF, 0x9BAF, 0xBABF, 0xD98F, 0xF89F,
    0x176E, 0x367E, 0x554E, 0x745E, 0x932E, 0xB23E, 0xD10E, 0xF01E,
};
static int16u CheckCRC(const int8u* Buffer, int Size)
{
    int16u C=0xFFFF;
    const uint8_t *End=Buffer+Size;

    while (Buffer<End)
        C = (C>>8)^CRC_CCIT_Table[((int8u)C)^*Buffer++];

    return C;
}

/* Table 6-12 p 53 */
void File_DtsUhd::DecodeVersion()
{
    int Bits=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ShortRevNum")?3:6; Param_Info1("ShortRevNum=" + std::to_string(Bits));
    StreamMajorVerNum=ReadBits(FrameStart, FrameSize, &FrameBit, Bits, "Temp")+2;
    SkipBits(&FrameBit, Bits, "Temp");
}

/* Table 6-12 p 53 */
int File_DtsUhd::ExtractStreamParams()
{
    Element_Begin1("ExtractStreamParams");
    static const uint32_t TableBaseDuration[4] = {512, 480, 384, 0};
    static const uint32_t TableClockRate[4] = {32000, 44100, 48000, 0};

    if (SyncFrameFlag)
        FullChannelBasedMixFlag=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "FullChannelMixFlag");

    if (SyncFrameFlag||!FullChannelBasedMixFlag)
        if (CheckCRC(FrameStart, FTOCPayloadinBytes))
            return 1;

    if (SyncFrameFlag)
    {
        if (FullChannelBasedMixFlag)
            StreamMajorVerNum=2;
        else
            DecodeVersion();

        FrameDuration=TableBaseDuration[ReadBits(FrameStart, FrameSize, &FrameBit, 2, "BaseDuration")];
        FrameDurationCode=ReadBits(FrameStart, FrameSize, &FrameBit, 3, "FrameDurationCode");
        FrameDuration*=FrameDurationCode+1;
        ClockRateInHz=TableClockRate[ReadBits(FrameStart, FrameSize, &FrameBit, 2, "ClockRateInHz")];
        if (FrameDuration==0 || ClockRateInHz==0)
            return 1; /* bitstream error */

        auto Skip=36*ReadBits(FrameStart, FrameSize, &FrameBit, 1, "TimeStampPresent");
        SkipBits(&FrameBit, Skip, "TimeStamp");
        SampleRateMod=ReadBits(FrameStart, FrameSize, &FrameBit, 2, "SampleRateMod");
        SampleRate=ClockRateInHz*(1<<SampleRateMod);

        if (FullChannelBasedMixFlag)
        {
            InteractObjLimitsPresent=0;
        }
        else
        {
            SkipBits(&FrameBit, 1, "Reserved");
            InteractObjLimitsPresent = ReadBits(FrameStart, FrameSize, &FrameBit, 1, "InteractObjLimitsPresent");
        }
    }

    Element_End0();
    return 0;
}

/* Table 6-24 p52 */
void File_DtsUhd::NaviPurge()
{
    for (auto& Navi : NaviList)
        if (!Navi.Present)
            Navi.Bytes=0;
}

/* Table 6-23 p51.  Return 0 on success, and the index is returned in
   the *listIndex parameter.
*/
int File_DtsUhd::NaviFindIndex(int DesiredIndex, int* ListIndex)
{
    for (auto& Navi : NaviList)
    {
        if (Navi.Index==DesiredIndex)
        {
            Navi.Present=true;
            *ListIndex=Navi.Index;
            return 0;
        }
    }

    int Index=0;
    for (auto& Navi : NaviList)
    {
        if (Navi.Present&&Navi.Bytes==0)
            break;
        Index++;
    }

    if (Index>=NaviList.size())
        NaviList.push_back(NAVI());

    auto& Navi=NaviList[Index];
    Navi.Bytes=0;
    Navi.Present=true;
    Navi.Id=256;
    Navi.Index=Index;
    *ListIndex=Index;

    return 0;
}

/* Table 6-20 p48 */
int File_DtsUhd::ExtractChunkNaviData()
{
    Element_Begin1("ExtractChunkNaviData");
    constexpr int8u Table2468[4] = {2, 4, 6, 8};
    constexpr int8u TableAudioChunkSizes[4] = {9, 11, 13, 16};
    constexpr int8u TableChunkSizes[4] = {6, 9, 12, 15};

    ChunkBytes = 0;
    if (FullChannelBasedMixFlag)
        ChunkList.resize(SyncFrameFlag);
    else
        ChunkList.resize(ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2468, "Num_MD_Chunks"));

    for (auto& Chunk : ChunkList)
    {
        ChunkBytes+=Chunk.Bytes=ReadBitsVar(FrameStart, FrameSize, &FrameBit, TableChunkSizes, "MDChunkSize");
        if (FullChannelBasedMixFlag)
            Chunk.CrcFlag=false;
        else
            Chunk.CrcFlag=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "MDChunkCRCFlag");
    }

    int NumAudioChunks=1;
    if (!FullChannelBasedMixFlag)
        NumAudioChunks=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2468, "Num_Audio_Chunks");

    if (!SyncFrameFlag)
    {
        for (auto& Navi : NaviList)
            Navi.Present=false;
    }
    else
        NaviList.clear();

    for (int i=0; i<NumAudioChunks; i++)
    {
        int Index=0;
        if (!FullChannelBasedMixFlag)
            Index=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2468, "AudioChunkIndex");

        if (NaviFindIndex(Index, &Index))
            return 1;

        bool ACIDPresent=false;
        if (SyncFrameFlag)
            ACIDPresent=true;
        else if (!FullChannelBasedMixFlag)
            ACIDPresent=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ACIDsPresentFlag");
        if (ACIDPresent)
            NaviList[Index].Id=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2468, "AudioChunkID");
        NaviList[Index].Bytes=ReadBitsVar(FrameStart, FrameSize, &FrameBit, TableAudioChunkSizes, "AudioChunkSize");
        ChunkBytes+=NaviList[Index].Bytes;
    }

    NaviPurge();

    Element_End0();
    return 0;
}


/* Table 6-6 */
int File_DtsUhd::ExtractMDChunkObjIDList(MD01* MD01)
{
    Element_Begin1("ExtractMDChunkObjIDList");
    if (FullChannelBasedMixFlag)
    {
        MD01->NumObjects=1;
        MD01->ObjectList[0]=256;
    }
    else
    {
        constexpr int8u Table[4] = {3, 4, 6, 8};
        MD01->NumObjects = ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "NumObjects");
        for (int i=0; i<MD01->NumObjects; i++)
        {
            int n = ReadBits(FrameStart, FrameSize, &FrameBit, 1, "NumBitsforObjID") ? 8 : 4;
            MD01->ObjectList[i]=ReadBits(FrameStart, FrameSize, &FrameBit, n, "ObjectIDList[Obj]");
        }
    }
    Element_End0();

    return 0;
}


/* Table 7-9 */
void File_DtsUhd::ExtractLTLMParamSet(MD01* MD01, bool NominalLD_DescriptionFlag)
{
    Element_Begin1("ExtractLTLMParamSet");
    ReadBitsMD01(MD01, 6, "LoudnessMeasureTableIndex"); /* rLoudness */
    if (!NominalLD_DescriptionFlag)
        ReadBitsMD01(MD01, 5, "AssociatedAssetType");

    int32u BitWidth=ReadBitsMD01(MD01, NominalLD_DescriptionFlag?2:4, "BitWidth"); Param_Info2(BitWidth, " bits");
    Element_End0();
}

/* Table 7-8 */
int File_DtsUhd::ParseStaticMDParams(MD01* MD01, bool OnlyFirst)
{
    bool NominalLD_DescriptionFlag=true;
    int NumLongTermLoudnessMsrmSets=1;

    if (FullChannelBasedMixFlag==false)
        NominalLD_DescriptionFlag=ReadBitsMD01(MD01, 1, "NominalLD_DescriptionFlag");

    if (NominalLD_DescriptionFlag)
    {
        if (!FullChannelBasedMixFlag)
        {
            NumLongTermLoudnessMsrmSets=ReadBitsMD01(MD01, 1, "NumLongTermLoudnessMsrmSets")?3:1;
            Param_Info2(NumLongTermLoudnessMsrmSets, " sets");
        }
    }
    else
        NumLongTermLoudnessMsrmSets=ReadBitsMD01(MD01, 4, "NumLongTermLoudnessMsrmSets")+1;

    for (int i=0; i<NumLongTermLoudnessMsrmSets; i++)
        ExtractLTLMParamSet(MD01, NominalLD_DescriptionFlag);

    if (OnlyFirst)
        return 0;

    if (!NominalLD_DescriptionFlag)
        ReadBitsMD01(MD01, 1, "IsLTLoudnMsrsmOffLine");

    const int NUMDRCOMPRTYPES=3;
    for (int i=0; i<NUMDRCOMPRTYPES; i++) /* Table 7-12 suggest 3 types */
    {
        if (ReadBitsMD01(MD01, 1, "CustomDRCCurveMDPresent"))
        {
            if (ReadBitsMD01(MD01, 4)==15) /* Table 7-14 */
                ReadBitsMD01(MD01, 15);
        }
        if (ReadBitsMD01(MD01, 1, "CustomDRCSmoothMDPresent"))
            ReadBitsMD01(MD01, 6*6, "DRCData");
    }

    if (!FullChannelBasedMixFlag)
        MD01->Bit=MD01->NumStaticMDPackets*MD01->StaticMDPacketByteSize;
    MD01->StaticMDParamsExtracted=true;

    return 0;
}

/* Table 7-7 */
int File_DtsUhd::ExtractMultiFrameDistribStaticMD(MD01* MD01)
{
    Element_Begin1("ExtractMultiFrameDistribStaticMD");
    static const uint8_t Table1[4] = {0, 6, 9, 12};
    static const uint8_t Table2[4] = {5, 7, 9, 11};

    if (SyncFrameFlag)
    {
        MD01->PacketsAcquired=false;
        if (FullChannelBasedMixFlag)
        {
            MD01->NumStaticMDPackets=1;
            MD01->StaticMDPacketByteSize=0;
        }
        else
        {
            MD01->NumStaticMDPackets=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table1, "NumStaticMDPackets")+1;
            MD01->StaticMDPacketByteSize=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2, "StaticMDPacketByteSize")+3;
        }

        MD01->Buffer.resize(MD01->NumStaticMDPackets*MD01->StaticMDPacketByteSize);
        MD01->Bit=0;
        if (MD01->NumStaticMDPackets>1)
            MD01->StaticMetadataUpdtFlag=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "StaticMetadataUpdtFlag");
        else
            MD01->StaticMetadataUpdtFlag=true;
    }

    if (MD01->PacketsAcquired<MD01->NumStaticMDPackets)
    {
        int n=MD01->PacketsAcquired*MD01->StaticMDPacketByteSize;
        for (int i = 0; i < MD01->StaticMDPacketByteSize; i++)
            MD01->Buffer[n+i] = ReadBits(FrameStart, FrameSize, &FrameBit, 8, ("MetadataPacketPayload[" + std::to_string(n+i) + "]").c_str());
        MD01->PacketsAcquired++;

        if (MD01->PacketsAcquired==MD01->NumStaticMDPackets)
        {
            if (MD01->StaticMetadataUpdtFlag||!MD01->StaticMDParamsExtracted)
                if (ParseStaticMDParams(MD01, 0))
                    return 1;
        }
        else if (MD01->PacketsAcquired==1)
        {
            if (MD01->StaticMetadataUpdtFlag||!MD01->StaticMDParamsExtracted)
                if (ParseStaticMDParams(MD01, 1))
                    return 1;
        }
    }

    Element_End0();
    return 0;
}

/* Return 1 if suitable, 0 if not.  Table 7-18.  OBJGROUPIDSTART=224 Sec 7.8.7 p75 */
bool File_DtsUhd::CheckIfMDIsSuitableforImplObjRenderer(MD01* MD01, int ObjectId)
{
    Element_Begin1("CheckIfMDIsSuitableforImplObjRenderer");
    if (ObjectId >= 224 || ReadBits(FrameStart, FrameSize, &FrameBit, 1, "MDUsedByAllRenderersFlag"))
        return true;

    /*  Reject the render and skip the render data. */
    constexpr int8u Table[4] = {8, 10, 12, 14};
    SkipBits(&FrameBit, 1, "RequiredRendererType");
    auto Skip=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "NumBits2Skip")+1;
    SkipBits(&FrameBit, Skip, "data");
    Element_End0();
    return false;
}

/* Table 7-26 */
void File_DtsUhd::ExtractChMaskParams(MD01* MD01, MDObject* Object)
{
    Element_Begin1("ExtractChMaskParams");
    constexpr int MaskTable[14] = /* Table 7-27 */
    {
        0x000001, 0x000002, 0x000006, 0x00000F, 0x00001F, 0x00084B, 0x00002F,
        0x00802F, 0x00486B, 0x00886B, 0x03FBFB, 0x000003, 0x000007, 0x000843,
    };

    const int ChLayoutIndex=Object->RepType==REP_TYPE_BINAURAL?1:ReadBits(FrameStart, FrameSize, &FrameBit, 4, "ChLayoutIndex");
    if (ChLayoutIndex==14)
        Object->ChActivityMask=ReadBits(FrameStart, FrameSize, &FrameBit, 16, "ChActivityMask");
    else if (ChLayoutIndex==15)
        Object->ChActivityMask=ReadBits(FrameStart, FrameSize, &FrameBit, 32, "ChActivityMask");
    else
        Object->ChActivityMask=MaskTable[ChLayoutIndex];
    Element_End0();
}

/* Table 7-22 */
int File_DtsUhd::ExtractObjectMetadata(MD01* MD01, MDObject* Object,
                                     bool ObjStartFrameFlag, int ObjectId)
{
    Element_Begin1("ExtractObjectMetadata");
    if (ObjectId!=256)
        ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ObjActiveFlag");
    if (ObjStartFrameFlag)
    {
        bool ChMaskObjectFlag=false, Object3DMetaDataPresent=false;
        Object->RepType = ReadBits(FrameStart, FrameSize, &FrameBit, 3, "ObjRepresTypeIndex");
        switch (Object->RepType)
        {
            case REP_TYPE_BINAURAL:
            case REP_TYPE_CH_MASK_BASED:
            case REP_TYPE_MTRX2D_CH_MASK_BASED:
            case REP_TYPE_MTRX3D_CH_MASK_BASED:
                ChMaskObjectFlag=true;
                break;

            case REP_TYPE_3D_OBJECT_SINGLE_SRC_PER_WF:
            case REP_TYPE_3D_MONO_OBJECT_SINGLE_SRC_PER_WF:
                Object3DMetaDataPresent=true;
                break;
        }

        if (ChMaskObjectFlag)
        {
            if (ObjectId!=256)
            {
                SkipBits(&FrameBit, 3, "ObjectImportanceLevel");
                if (ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ObjTypeDescrPresent"))
                {
                    auto Skip=ReadBits(FrameStart, FrameSize, &FrameBit, 1, "TypeBitWidth")?3:5;
                    SkipBits(&FrameBit, Skip,"ObjTypeDescrIndex");
                }

                constexpr int8u Table1[4]={1, 4, 4, 8};
                constexpr int8u Table2[4]={3, 3, 4, 8};
                ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table1, "ObjAudioChunkIndex");
                ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table2, "ObjNaviWithinACIndex");

                /* Skip optional Loudness block. */
                if (ReadBits(FrameStart, FrameSize, &FrameBit, 1, "PerObjLTLoudnessMDPresent"))
                    SkipBits(&FrameBit, 8, "PerObjLTLoudnessMD");

                /* Skip optional Object Interactive MD (Table 7-25). */
                if (ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ObjInteractiveFlag") && InteractObjLimitsPresent)
                    if (ReadBits(FrameStart, FrameSize, &FrameBit, 1, "ObjInterLimitsFlag"))
                        SkipBits(&FrameBit, 5+6*Object3DMetaDataPresent, "ObjectInteractMD");
            }

            ExtractChMaskParams(MD01, Object);
        }
    }

    /* Skip rest of object */
    Element_End0();
    return 0;
}

/* Table 7-4 */
int File_DtsUhd::ParseMD01(MD01 *MD01, int AuPresInd)
{

    if (AudPresParam[AuPresInd].Selectable)
    {
        Element_Begin1("ExtractPresScalingParams");
        for (int i = 0; i<4; i++)  /* Table 7-5.  Scaling data. */
        {
            auto Skip=5*ReadBits(FrameStart, FrameSize, &FrameBit, 1, "OutScalePresent");
            SkipBits(&FrameBit, Skip, "OutScale");
        }
        Element_End0();

        if (ReadBits(FrameStart, FrameSize, &FrameBit, 1, "MFDistrStaticMDPresent") && ExtractMultiFrameDistribStaticMD(MD01))
            return 1;
    }

    /* Table 7-16: Object metadata. */
    memset(MD01->Object, 0, sizeof(MD01->Object));
    if (!FullChannelBasedMixFlag)
    {
        auto Skip=11*ReadBits(FrameStart, FrameSize, &FrameBit, 1, "MixStudioParamsPresent");
        SkipBits(&FrameBit, Skip, "MixStudioParams");
    }

    for (int i=0; i<MD01->NumObjects; i++)
    {
        int32u Id = MD01->ObjectList[i];
        if (!CheckIfMDIsSuitableforImplObjRenderer(MD01, Id))
            continue;

        MD01->Object[Id].PresIndex = AuPresInd;
        bool StartFlag=false;
        if (!MD01->Object[Id].Started)
        {
            SkipBits(&FrameBit, Id!=256, "ObjStaticFlag");
            StartFlag=MD01->Object[Id].Started=true;
        }

        if ((Id<224||Id>255) && ExtractObjectMetadata(MD01, MD01->Object+Id, StartFlag, Id))
            return 1;

        break;
    }

    return 0;
}

/* Table 6-2 */
int File_DtsUhd::UnpackMDFrame()
{
    Element_Begin1("UnpackMDFrame");
    for (auto& Chunk : ChunkList)
    {
        int BitNext=FrameBit+Chunk.Bytes*8;
        if (Chunk.CrcFlag && CheckCRC(FrameStart+FrameBit/8, Chunk.Bytes))
            return 1;

        int32u Id=ReadBits(FrameStart, FrameSize, &FrameBit, 8, "MDChunkID");
        if (Id==1)
        {
            constexpr int8u Table[4] = { 0, 2, 4, 4 };
            int AudPresIndex=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "AudPresIndex");
            if (AudPresIndex>255)
            {
                Element_End0();
                return 1;
            }
            MD01* MD01=ChunkFindMD01(Id);
            if (MD01==nullptr)
                MD01=ChunkAppendMD01(Id);
            if (MD01==nullptr)
            {
                Element_End0();
                return 1;
            }
            if (ExtractMDChunkObjIDList(MD01))
            {
                Element_End0();
                return 1;
            }
            if (ParseMD01(MD01, AudPresIndex))
            {
                Element_End0();
                return 1;
            }
        }

        FrameBit=BitNext;
    }

    Element_End0();
    return 0;
}

/** Parse a single DTS:X Profile 2 frame.
    The frame must start at the first byte of the data buffer, and enough
    of the frame must be present to decode the majority of the FTOC.
    From Table 6-11 p52.  A sync frame must be provided.
*/
int File_DtsUhd::DtsUhd_Frame()
{
    if (FrameSize<4)
        return DTSUHD_INCOMPLETE; //Data buffer does not contain the signature.

    FrameBit=0;

    int32u SyncWord=ReadBits(FrameStart, FrameSize, &FrameBit, 32, "SyncWord");
    SyncFrameFlag=SyncWord==DTSUHD_SYNCWORD;
    if (SyncFrameFlag)
        Element_Info1("Key frame");
    if (SyncWord!=DTSUHD_SYNCWORD && SyncWord!=DTSUHD_NONSYNCWORD)
        return DTSUHD_NOSYNC;  //Invalid frame.

    constexpr int8u Table[4]={5, 8, 10, 12};
    FTOCPayloadinBytes=ReadBitsVar(FrameStart, FrameSize, &FrameBit, Table, "FTOCPayloadinBytes") + 1;
    if (FTOCPayloadinBytes<5||FTOCPayloadinBytes>=FrameSize)
        return DTSUHD_INCOMPLETE;  //Data buffer does not contain entire FTOC

    if (ExtractStreamParams())
        return DTSUHD_INVALID_FRAME;

    if (ResolveAudPresParams())
        return DTSUHD_INVALID_FRAME;

    if (ExtractChunkNaviData())  //AudioChunkTypes and payload sizes.
        return DTSUHD_INVALID_FRAME;

    //At this point in the parsing, we can calculate the size of the frame.
    if (FrameSize<FTOCPayloadinBytes+ChunkBytes)
        return DTSUHD_INCOMPLETE;
    FrameSize=FTOCPayloadinBytes+ChunkBytes;

    //Skip PBRSmoothParams (Table 6-26) and align to the chunks immediatelyfollowing the FTOC CRC.
    FrameBit=FTOCPayloadinBytes*8;
    if (UnpackMDFrame())
        return DTSUHD_INVALID_FRAME;
    UpdateDescriptor();

    return DTSUHD_OK;
}

//---------------------------------------------------------------------------
const char* FrameRateTable[8]=
{
    "Not Indicated",
    "23.97",
    "24.00",
    "25.00",
    "29.97 DROP",
    "29.97",
    "30.00 DROP",
    "30.00",
};
const int32u FrequencyCodeTable[2]={44100, 48000};
const char* RepresentationTypeTable[8]=
{
    "Channel Mask Based Representation",
    "Matrix 2D Channel Mask Based Representation",
    "Matrix 3D Channel Mask Based Representation",
    "Binaurally Processed Audio Representation",
    "Ambisonic Representation Representation",
    "Audio Tracks with Mixing Matrix Representation",
    "3D Object with One 3D Source Per Waveform Representation",
    "Mono 3D Object with Multiple 3D Sources Per Waveform Representation",
};

//---------------------------------------------------------------------------
struct DTSUHD_ChannelMaskInfo DTSUHD_DecodeChannelMask(int32u ChannelMask)
{
    DTSUHD_ChannelMaskInfo Info={};

    // ETSI TS 103 491 V1.2.1 Table C-4: Speaker Labels for ChannelMask
    if (ChannelMask)
    {
        if (ChannelMask & 0x00000003)
        {
            Info.ChannelPositionsText+=", Front:";
            if (ChannelMask & 0x00000002)
            {
                Info.ChannelLayoutText+=" L";
                Info.ChannelPositionsText+=" L";
                Info.CountFront++;
            }
            if (ChannelMask & 0x00000001)
            {
                Info.ChannelLayoutText+=" C";
                Info.ChannelPositionsText+=" C";
                Info.CountFront++;
            }
            if (ChannelMask & 0x00000004)
            {
                Info.ChannelLayoutText+=" R";
                Info.ChannelPositionsText+=" R";
                Info.CountFront++;
            }
        }

        if (ChannelMask & 0x00000020)
            Info.ChannelLayoutText+=" LFE";
        if (ChannelMask & 0x00010000)
            Info.ChannelLayoutText+=" LFE2";

        if (ChannelMask & 0x00000618)
        {
            Info.ChannelPositionsText+=", Side:";
            if (ChannelMask & 0x00000008)
            {
                Info.ChannelLayoutText+=" Ls";
                Info.ChannelPositionsText+=" L";
                Info.CountSide++;
            }
            if (ChannelMask & 0x00000010)
            {
                Info.ChannelLayoutText+=" Rs";
                Info.ChannelPositionsText+=" R";
                Info.CountSide++;
            }
            if (ChannelMask & 0x00000200)
            {
                Info.ChannelLayoutText+=" Lss";
                Info.ChannelPositionsText+=" L";
                Info.CountSide++;
            }
            if (ChannelMask & 0x00000400)
            {
                Info.ChannelLayoutText+=" Rss";
                Info.ChannelPositionsText+=" R";
                Info.CountSide++;
            }
        }

        if (ChannelMask & 0x000001C0)
        {
            Info.ChannelPositionsText+=", Rear:";
            if (ChannelMask & 0x00000080)
            {
                Info.ChannelLayoutText+=" Lsr";
                Info.ChannelPositionsText+=" L";
                Info.CountRear++;
            }
            if (ChannelMask & 0x00000040)
            {
                Info.ChannelLayoutText+=" Cs";
                Info.ChannelPositionsText+=" C";
                Info.CountRear++;
            }
            if (ChannelMask & 0x00000100)
            {
                Info.ChannelLayoutText+=" Rsr";
                Info.ChannelPositionsText+=" R";
                Info.CountRear++;
            }
        }

        if (ChannelMask & 0x0E000000)
        {
            Info.ChannelPositionsText+=", LowFront:";
            if (ChannelMask & 0x04000000)
            {
                Info.ChannelLayoutText+=" Lb";
                Info.ChannelPositionsText+=" L";
                Info.CountLows++;
            }
            if (ChannelMask & 0x02000000)
            {
                Info.ChannelLayoutText+=" Cb";
                Info.ChannelPositionsText+=" C";
                Info.CountLows++;
            }
            if (ChannelMask & 0x08000000)
            {
                Info.ChannelLayoutText+=" Rb";
                Info.ChannelPositionsText+=" R";
                Info.CountLows++;
            }
        }

        if (ChannelMask & 0x0000E000)
        {
            Info.ChannelPositionsText+=", High:";
            if (ChannelMask & 0x00002000)
            {
                Info.ChannelLayoutText+=" Lh";
                Info.ChannelPositionsText+=" L";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x00004000)
            {
                Info.ChannelLayoutText+=" Ch";
                Info.ChannelPositionsText+=" C";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x00008000)
            {
                Info.ChannelLayoutText+=" Rh";
                Info.ChannelPositionsText+=" R";
                Info.CountHeights++;
            }
        }

        if (ChannelMask & 0x00060000)
        {
            Info.ChannelPositionsText+=", Wide:";
            if (ChannelMask & 0x00020000)
            {
                Info.ChannelLayoutText+=" Lw";
                Info.ChannelPositionsText+=" L";
                Info.CountFront++;
            }
            if (ChannelMask & 0x00040000)
            {
                Info.ChannelLayoutText+=" Rw";
                Info.ChannelPositionsText+=" R";
                Info.CountFront++;
            }
        }

        if (ChannelMask & 0x30000000)
        {
            Info.ChannelPositionsText+=", TopFront:";
            if (ChannelMask & 0x10000000)
            {
                Info.ChannelLayoutText+=" Ltf";
                Info.ChannelPositionsText+=" L";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x20000000)
            {
                Info.ChannelLayoutText+=" Rtf";
                Info.ChannelPositionsText+=" R";
                Info.CountHeights++;
            }
        }

        if (ChannelMask & 0x00080000)
        {
            Info.ChannelPositionsText+=", TopCtrSrrd";
            Info.ChannelLayoutText+=" Oh";
            Info.CountHeights++;
        }

        if (ChannelMask & 0x00001800)
        {
            Info.ChannelPositionsText+=", Center:";
            if (ChannelMask & 0x00000800)
            {
                Info.ChannelLayoutText+=" Lc";
                Info.ChannelPositionsText+=" L";
                Info.CountFront++;
            }
            if (ChannelMask & 0x00001000)
            {
                Info.ChannelLayoutText+=" Rc";
                Info.ChannelPositionsText+=" R";
                Info.CountFront++;
            }
        }

        if (ChannelMask & 0xC0000000)
        {
            Info.ChannelPositionsText+=", TopRear:";
            if (ChannelMask & 0x40000000)
            {
                Info.ChannelLayoutText+=" Ltr";
                Info.ChannelPositionsText+=" L";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x80000000)
            {
                Info.ChannelLayoutText+=" Rtr";
                Info.ChannelPositionsText+=" R";
                Info.CountHeights++;
            }
        }

        if (ChannelMask & 0x00300000)
        {
            Info.ChannelPositionsText+=", HighSide:";
            if (ChannelMask & 0x00100000)
            {
                Info.ChannelLayoutText+=" Lhs";
                Info.ChannelPositionsText+=" L";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x00200000)
            {
                Info.ChannelLayoutText+=" Rhs";
                Info.ChannelPositionsText+=" R";
                Info.CountHeights++;
            }
        }

        if (ChannelMask & 0x01C00000)
        {
            Info.ChannelPositionsText+=", HighRear:";
            if (ChannelMask & 0x00800000)
            {
                Info.ChannelLayoutText+=" Lhr";
                Info.ChannelPositionsText+=" L";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x00400000)
            {
                Info.ChannelLayoutText+=" Chr";
                Info.ChannelPositionsText+=" C";
                Info.CountHeights++;
            }
            if (ChannelMask & 0x01000000)
            {
                Info.ChannelLayoutText+=" Rhr";
                Info.ChannelPositionsText+=" R";
                Info.CountHeights++;
            }
        }

        if (ChannelMask & 0x00000020)
        {
            Info.ChannelPositionsText+=", LFE";
            Info.CountLFE++;
        }
        if (ChannelMask & 0x00010000)
        {
            Info.ChannelPositionsText+=", LFE2";
            Info.CountLFE++;
        }

        Info.ChannelLayoutText.erase(0, 1);
        Info.ChannelPositionsText.erase(0, 2);
        Info.ChannelPositions2Text = std::to_string(Info.CountFront) + "/" +
                std::to_string(Info.CountSide) + "/" +
                std::to_string(Info.CountRear) + "." +
                std::to_string(Info.CountLFE);
        if (Info.CountHeights)
            Info.ChannelPositions2Text+="." + std::to_string(Info.CountHeights);
        if (Info.CountLows)
            Info.ChannelPositions2Text+="." + std::to_string(Info.CountLows);
    }

    Info.ChannelCount=Info.CountFront+Info.CountSide+Info.CountRear+Info.CountLFE+Info.CountHeights+Info.CountLows;

    return Info;
}

bool File_DtsUhd::CheckCurrentFrame(const int8u* FrameBegin, int32u FrameSize)
{
    //Read length of CRC'd frame data and perform CRC check
    #if MEDIAINFO_TRACE
    auto Trace_Activated_Sav=Trace_Activated;
    Trace_Activated=false; // Do not show trace while trying to sync
    #endif
    constexpr int8u VbitsPayload[4]={5, 8, 10, 12}; //varbits decode table
    int Bit=32;
    int32u FtocSize=ReadBitsVar(FrameBegin, FrameSize, &Bit, VbitsPayload)+1;
    bool SyncFrame=CC4(FrameBegin)==DTSUHD_SYNCWORD;
    if (SyncFrame)
        FullChannelBasedMixFlag=ReadBits(FrameBegin, FrameSize, &Bit, 1);
    #if MEDIAINFO_TRACE
    Trace_Activated=Trace_Activated_Sav;
    #endif
    bool HasCRC=SyncFrame||!FullChannelBasedMixFlag;
    return !HasCRC || CheckCRC(FrameBegin, FtocSize)==0;
    /*
    auto Trace_Activated_Sav = Trace_Activated;
    Trace_Activated=false; // Do not show trace while trying to sync
    constexpr int8u VbitsPayload[4]={5, 8, 10, 12}; //varbits decode table
    auto Buffer_Offset_Sav=Buffer_Offset;
    Buffer_Offset+=4; // SyncWord
    BS_Begin();
    int32u FtocSize;
    Get_VR(VbitsPayload, FtocSize,                              "FTOCPayloadinBytes");
    FtocSize++;
    if (SyncFrame)
        Get_SB(FullChannelBasedMixFlag,                         "FullChannelBasedMixFlag");
    BS_End();
    Trace_Activated=Trace_Activated_Sav;
    Buffer_Offset=Buffer_Offset_Sav;
    bool HasCRC=SyncFrame||FullChannelBasedMixFlag;
    return !HasCRC || CheckCRC(Buffer+Buffer_Offset, FtocSize)==0;
    */

}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
File_DtsUhd::File_DtsUhd()
:File__Analyze()
{
    //Configuration
    ParserName="DtsUhd";
    #if MEDIAINFO_EVENTS
        ParserIDs[0]=MediaInfo_Parser_Dts;
        StreamIDs_Width[0]=0;
    #endif //MEDIAINFO_EVENTS
    #if MEDIAINFO_TRACE
        Trace_Layers_Update(8); //Stream
    #endif //MEDIAINFO_TRACE
    MustSynchronize=true;
    Buffer_TotalBytes_FirstSynched_Max=32*1024;
    PTS_DTS_Needed=true;
    StreamSource=IsStream;

    //In
    Frame_Count_Valid=0;
}

//---------------------------------------------------------------------------
bool File_DtsUhd::FileHeader_Begin()
{
    //Must have enough buffer for having header
    if (Buffer_Size<8)
        return false; //Must wait for more data

    //False positives detection: Detect WAV files, the parser can't detect it easily, there is only 70 bytes of beginning for saying WAV
    switch (CC4(Buffer))
    {
        case 0x52494646 : //"RIFF"
        case 0x000001FD : //MPEG-PS private
                            Finish("DTSUHD");
                            return false;
        default         :   ;
    }

    //DTSHDHDR AUPR-HDR has number of audio frames, needed to compute duration
    if (CC8(Buffer)==CHUNK_DTSHDHDR)
    {
        bool Done=false;
        while (Buffer_Offset+16 < Buffer_Size)
        {
            int64u name = CC8(Buffer+Buffer_Offset);
            int64u size = CC8(Buffer+Buffer_Offset+8);
            Buffer_Offset+=16;
            if (Buffer_Offset+10>=Buffer_Size)
                return false;
            if (name==CHUNK_DTSHDHDR)
                FrameRate=CC1(Buffer+Buffer_Offset+9);
            else if (name==CHUNK_AUPR_HDR)
                FramesTotal=CC4(Buffer+Buffer_Offset+6);
            else if (name==CHUNK_STRMDATA)
                Done=true;
            if (Done)
                break;
            Buffer_Offset+=size;
        }
        if (!Done)
            return false;
    }
    else
    {
        FrameRate=0;
        FramesTotal=0;
    }

    //All should be OK...
    if (!Frame_Count_Valid)
        Frame_Count_Valid=Config->ParseSpeed>=0.3?32:(IsSub?1:2);
    return true;
}

//---------------------------------------------------------------------------
void File_DtsUhd::Streams_Fill()
{
    if (SampleRate==0||FrameDuration==0)
        return;

    DTSUHD_ChannelMaskInfo ChannelMaskInfo = DTSUHD_DecodeChannelMask(FrameDescriptor.ChannelMask);
    int32u FrameDuration=512<<FrameDurationCode;
    int32u MaxPayload=2048<<FrameDescriptor.MaxPayloadCode;
    float32 AudioDuration = 1000.0f * FramesTotal * FrameDescriptor.SampleCount / SampleRate;
    float32 BitRate_Max = 8.0f * MaxPayload * SampleRate / FrameDuration;
    std::string AudioCodec="dtsx", ProfileString="DTS:X P2";
    AudioCodec.back()+=FrameDescriptor.DecoderProfileCode > 0;
    ProfileString.back()+=FrameDescriptor.DecoderProfileCode;

    Fill(Stream_General, 0, General_Format, "DTS-UHD");
    Fill(Stream_General, 0, General_OverallBitRate_Mode, "VBR");

    Stream_Prepare(Stream_Audio);
    Fill(Stream_Audio, 0, Audio_BitRate_Maximum, BitRate_Max, 0, true);
    Fill(Stream_Audio, 0, Audio_BitRate_Mode, "VBR", Unlimited, true, true);
    Fill(Stream_Audio, 0, Audio_Codec, AudioCodec);
    if (AudioDuration>0.0f)
        Fill(Stream_Audio, 0, Audio_Duration, AudioDuration);
    Fill(Stream_Audio, 0, Audio_Format, "DTS-UHD");
    Fill(Stream_Audio, 0, Audio_Format_Commercial_IfAny, ProfileString);
    Fill(Stream_Audio, 0, Audio_Format_Profile, FrameDescriptor.DecoderProfileCode + 2);
    Fill(Stream_Audio, 0, Audio_Format_Settings, RepresentationTypeTable[FrameDescriptor.RepType]);
    if (FramesTotal)
        Fill(Stream_Audio, 0, Audio_FrameCount, FramesTotal);
    if (FrameRate<sizeof(FrameRateTable)/sizeof(FrameRateTable[0]))
        Fill(Stream_Audio, 0, Audio_FrameRate, FrameRateTable[FrameRate]);
    Fill(Stream_Audio, 0, Audio_SamplesPerFrame, FrameDuration);
    Fill(Stream_Audio, 0, Audio_SamplingRate, SampleRate);

    if (FrameDescriptor.ChannelMask)
    {
        Fill(Stream_Audio, 0, Audio_Channel_s_, ChannelMaskInfo.ChannelCount);
        Fill(Stream_Audio, 0, Audio_ChannelLayout, ChannelMaskInfo.ChannelLayoutText);
        Fill(Stream_Audio, 0, Audio_ChannelPositions, ChannelMaskInfo.ChannelPositionsText);
        Fill(Stream_Audio, 0, Audio_ChannelPositions_String2, ChannelMaskInfo.ChannelPositions2Text);
    }
}

//---------------------------------------------------------------------------
bool File_DtsUhd::Synchronize()
{
    while (Buffer_Offset+4<=Buffer_Size)
    {
        if (!FrameSynchPoint_Test(false))
            return false; //Need more data
        if (Synched)
            break;
        Buffer_Offset++;
    }

    return true;
}

//---------------------------------------------------------------------------
bool File_DtsUhd::Synched_Test()
{
     //Quick test of synchro
    if (!FrameSynchPoint_Test(true))
        return false; //Need more data
    if (!Synched)
        return true;

    //We continue
    return true;
}

//---------------------------------------------------------------------------
void File_DtsUhd::Read_Buffer_Unsynched()
{
    FrameInfo=frame_info();
}

//---------------------------------------------------------------------------
void File_DtsUhd::Header_Parse()
{
    Header_Fill_Size(FrameSize);
}

//---------------------------------------------------------------------------
void File_DtsUhd::Data_Parse()
{
    FILLING_BEGIN();
        Element_Name("Frame");
        Element_Info1(Frame_Count);
        if (DtsUhd_Frame()==DTSUHD_OK)
            Skip_XX(FrameSize-(FrameBit+7)/8, "data");
        if (!Status[IsAccepted])
            Accept("DTS-UHD");
        Frame_Count++;
        if (Frame_Count>=Frame_Count_Valid)
        {
            Fill("DTS-UHD");
            Finish("DTS-UHD");
        }
    FILLING_END();
}

//---------------------------------------------------------------------------
bool File_DtsUhd::FrameSynchPoint_Test(bool AcceptNonSync)
{
    FrameStart=nullptr;
    FrameStart=0;

    if (Buffer_Offset+16>Buffer_Size)
        return false; //Must wait for more data

    int32u SyncWord=CC4(Buffer+Buffer_Offset);
    if (SyncWord!=DTSUHD_SYNCWORD && (!AcceptNonSync || SyncWord!=DTSUHD_NONSYNCWORD))
    {
        Synched=false;
        return true; //Not a syncpoint, done
    }

    Synched=CheckCurrentFrame(Buffer+Buffer_Offset,Buffer_Size-Buffer_Offset);

    if (Synched)
    {
        FrameStart=Buffer+Buffer_Offset;
        if (IsSub)
        {
            FrameSize=Element_Size;
            return true; // We trust the container enough
        }
        FrameSize=4;
        while (Buffer_Offset+FrameSize+4<=Buffer_Size)
        {
            int32u SyncWord=CC4(FrameStart+FrameSize);
            if (SyncWord==DTSUHD_SYNCWORD||SyncWord==DTSUHD_NONSYNCWORD)
            {
                Buffer_Offset+=FrameSize;
                bool IsOk=CheckCurrentFrame(FrameStart+FrameSize, Buffer_Size-Buffer_Offset-FrameSize);
                Buffer_Offset-=FrameSize;
                if (IsOk)
                    return true;
            }
            FrameSize++;
        }
    }

    return true;
}

//---------------------------------------------------------------------------
} //NameSpace

#endif //MEDIAINFO_DTSUHD_YES
