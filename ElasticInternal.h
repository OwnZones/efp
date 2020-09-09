//
//
//   ______  _              _    _        ______
//  |  ____|| |            | |  (_)      |  ____|
//  | |__   | |  __ _  ___ | |_  _   ___ | |__  _ __  __ _  _ __ ___    ___
//  |  __|  | | / _` |/ __|| __|| | / __||  __|| '__|/ _` || '_ ` _ \  / _ \
//  | |____ | || (_| |\__ \| |_ | || (__ | |   | |  | (_| || | | | | ||  __/
//  |______||_| \__,_||___/ \__||_| \___||_|   |_|   \__,_||_| |_| |_| \___|
//                                                                  Protocol
// UnitX @ Edgeware AB 2020
//
// For more information, example usage and plug-ins please see
// https://github.com/Unit-X/efp
//

#ifndef EFP_ELASTICINTERNAL_H
#define EFP_ELASTICINTERNAL_H

//Packet header part ----- START ------

// Type 0,1,2 aso. are static from when defined in this header file.
// For new protocol functions/features add new types and/or flags
// type15 is the maximum type number 4 bits used
// * - 0x00 private fragment
// * - 0x01 frame is larger than MTU
// * - 0x02 frame is less than MTU or the tail of a larger superframe
// * - 0x03 The reminder of the data does not fit a type2 packet but its the tail of the data.
// * - 0x04 minimalistic type2-type frame used when static EFP stream and reciever signaled known stream.

enum Frametype : uint8_t { //The 4 LSB are used! (The 4 MSB are the flags)
    type0 = 0,
    type1,
    type2,
    type3,
    type4
};

struct ElasticFrameType0 {
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
    uint8_t hFrameType = Frametype::type0;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
}
#ifdef __ANDROID__
__attribute__((packed))
#endif
;

struct ElasticFrameType1 {
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
    uint8_t hFrameType = Frametype::type1;
    uint8_t  hStream = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hFragmentNo = 0;
    uint16_t hOfFragmentNo = 0;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
}
#ifdef __ANDROID__
__attribute__((packed))
#endif
;

struct ElasticFrameType2 {
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
    uint8_t hFrameType  = Frametype::type2;
    uint8_t  hStreamID = 0;
    ElasticFrameContent hDataContent = ElasticFrameContent::unknown;
    uint16_t hSizeOfData = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hOfFragmentNo = 0;
    uint16_t hType1PacketSize = 0;
    uint64_t hPts = UINT64_MAX;
    uint32_t hDtsPtsDiff = UINT32_MAX;
    uint32_t hCode = UINT32_MAX;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
}
#ifdef __ANDROID__
__attribute__((packed))
#endif
;

struct ElasticFrameType3 {
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
    uint8_t hFrameType = Frametype::type3;
    uint8_t  hStreamID = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hType1PacketSize = 0;
    uint16_t hOfFragmentNo = 0;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
}
#ifdef __ANDROID__
__attribute__((packed))
#endif
;

//Proposal of new minimalistic end-frame
struct ElasticFrameType4 {
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
    uint8_t hFrameType  = Frametype::type4;
    uint8_t  hStreamID = 0;
    uint16_t hSizeOfData = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hOfFragmentNo = 0;
    uint16_t hType1PacketSize = 0;
    uint64_t hPts = UINT64_MAX;
    uint32_t hDtsPtsDiff = UINT32_MAX;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
}
#ifdef __ANDROID__
__attribute__((packed))
#endif
;
//Packet header part ----- END ------


#endif //EFP_ELASTICINTERNAL_H
