//
// Created by Anders Cedronius on 2019-11-19.
//

#ifndef EFP_EDGEWAREINTERNAL_H
#define EFP_EDGEWAREINTERNAL_H

// GLobal Logger -- Start
#define LOGG_NOTIFY 1
#define LOGG_WARN 2
#define LOGG_ERROR 4
#define LOGG_FATAL 8
#define LOGG_MASK  LOGG_NOTIFY | LOGG_WARN | LOGG_ERROR | LOGG_FATAL //What to logg?
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

//No version control.
//Type 0,1,2 aso. are static from when defined. For new protocol functions/features add new types.

enum Frametype : uint8_t {
    type0,
    type1,
    type2
};

/*
* <uint8_t> frameType
* - 0x00 illegal - discard
* - 0x01 frame is larger than MTU
* - 0x02 frame is less than MTU
* <uint16_t> sizeOfData (optional if frameType is 0x02)
* <uint16_t> superFrameNo
* <uint16_t> fragmentNo
* <uint16_t> ofFragmentNo
* <EdgewareFrameContent> dataContent
 * Where the datacontent is (uint64_t)pts+(uint64_t)FOURCC+(uint8_t[])data
*/

struct EdgewareFrameType0 {
    Frametype frameType = Frametype::type0;
};

struct EdgewareFrameType1 {
    Frametype frameType = Frametype::type1;
    uint16_t superFrameNo = 0;
    uint16_t fragmentNo = 0;
    uint16_t ofFragmentNo = 0;
    EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
};

struct EdgewareFrameType2 {
    Frametype frameType = Frametype::type2;
    uint16_t sizeOfData = 0;
    uint16_t superFrameNo = 0;
    uint16_t fragmentNo = 0;
    uint16_t ofFragmentNo = 0;
    uint16_t type1PacketSize = 0;
    uint64_t pts = UINT64_MAX;
    uint32_t code = UINT32_MAX;
    EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
};
//Packet header part ----- END ------

#endif //EFP_EDGEWAREINTERNAL_H
