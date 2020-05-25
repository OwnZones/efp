//
// UnitX Edgeware AB 2020
//

#ifndef EFP_ELASTICINTERNAL_H
#define EFP_ELASTICINTERNAL_H

//Packet header part ----- START ------

//Type 0,1,2 aso. are static from when defined. For new protocol functions/features add new types or flags
//type15 is the maximum type number 4 bits used
//* - 0x00 private fragment
//* - 0x01 frame is larger than MTU
//* - 0x02 frame is less than MTU
//* - 0x03 The reminder of the data does not fit a type2 packet
//* - 0x04 Minimalistic type2-type frame used when static EFP stream and reciever signaled known stream.

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
