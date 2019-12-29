//
// Created by Anders Cedronius on 2019-11-19.
//

#ifndef EFP_ELASTICINTERNAL_H
#define EFP_ELASTICINTERNAL_H

// GLobal Logger -- Start
#define LOGG_NOTIFY (unsigned)1
#define LOGG_WARN (unsigned)2
#define LOGG_ERROR (unsigned)4
#define LOGG_FATAL (unsigned)8
#define LOGG_MASK   LOGG_WARN | LOGG_ERROR | LOGG_FATAL //What to logg?
#define DEBUG  //Turn logging on/off

#ifdef DEBUG
#define LOGGER(l, g, f) \
{ \
std::ostringstream a; \
if (g == (LOGG_NOTIFY & (LOGG_MASK))) {a << "Notification: ";} \
else if (g == (LOGG_WARN & (LOGG_MASK))) {a << "Warning: ";} \
else if (g == (LOGG_ERROR & (LOGG_MASK))) {a << "Error: ";} \
else if (g == (LOGG_FATAL & (LOGG_MASK))) {a << "Fatal: ";} \
if (a.str().length()) { \
if (l) {a << __FILE__ << " " << __LINE__ << " ";} \
a << f << std::endl; \
std::cout << a.str(); \
} \
}
#else
#define LOGGER(l,g,f)
#endif
// GLobal Logger -- End

//Internal buffer management ----- START ------
struct CandidateToDeliver
{
    uint64_t deliveryOrder;
    uint64_t bucket;
    CandidateToDeliver(uint64_t k, uint64_t s) : deliveryOrder(k), bucket(s){}
};

struct sortDeliveryOrder
{
    inline bool operator() (const CandidateToDeliver& struct1, const CandidateToDeliver& struct2) {
        return (struct1.deliveryOrder < struct2.deliveryOrder);
    }
};
//Internal buffer management ----- END ------

//Packet header part ----- START ------

//Type 0,1,2 aso. are static from when defined. For new protocol functions/features add new types.
//type15 is the maximum type number 4 bits used
//* - 0x00 private fragment
//* - 0x01 frame is larger than MTU
//* - 0x02 frame is less than MTU
//* - 0x03 The reminder of the data does not fit a type2 packet
//Type1 and Type3 must be the same size
enum Frametype : uint8_t { //The 4 LSB are used! (The 4 MSB are the flags)
    type0 = 0,
    type1,
    type2,
    type3
};

struct ElasticFrameType0 {
    uint8_t hFrameType = Frametype::type0;
};

struct ElasticFrameType1 {
    uint8_t hFrameType = Frametype::type1;
    uint8_t  hStream = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hFragmentNo = 0;
    uint16_t hOfFragmentNo = 0;
};

struct ElasticFrameType2 {
    uint8_t hFrameType  = Frametype::type2;
    uint8_t  hStream = 0;
    ElasticFrameContent hDataContent = ElasticFrameContent::unknown;
    uint16_t hSizeOfData = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hOfFragmentNo = 0;
    uint16_t hType1PacketSize = 0;
    uint64_t hPts = UINT64_MAX;
    uint64_t hDts = UINT64_MAX;
    uint32_t hCode = UINT32_MAX;
};

struct ElasticFrameType3 {
    uint8_t hFrameType = Frametype::type3;
    uint8_t  hStream = 0;
    uint16_t hSuperFrameNo = 0;
    uint16_t hType1PacketSize = 0;
    uint16_t hOfFragmentNo = 0;
};
//Packet header part ----- END ------


#endif //EFP_ELASTICINTERNAL_H
