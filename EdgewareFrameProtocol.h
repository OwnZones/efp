//
// Created by Anders Cedronius on 2019-11-11.
//

#ifndef EFP_EDGEWAREFRAMEPROTOCOL_H
#define EFP_EDGEWAREFRAMEPROTOCOL_H


#include <cstdint>
#include <vector>
#include <iostream>
#include <sstream>
#include <climits>
#include <math.h>
#include <thread>
#include <unistd.h>

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


#define MAJOR_VERSION 1
#define MINOR_VERSION 0

namespace EdgewareFrameContentNamespace {
    enum EdgewareFrameContentDefines : uint8_t {
        unknown,
        privateData,
        h264,
        hevc,
        adts
    };
}

namespace EdgewareFrameMessagesNamespace {
    enum EdgewareFrameMessagesDefines : uint8_t {
        noError,
        tooLargeFrame,
        notImplemented,
        unknownFrametype,
        framesizeMismatch,
        internalCalculationError,
        endOfPacketError,
        bufferOutOfBounds,
        duplicatePacketRecieved
    };
}

using EdgewareFrameMessages = EdgewareFrameMessagesNamespace::EdgewareFrameMessagesDefines;
using EdgewareFrameContent = EdgewareFrameContentNamespace::EdgewareFrameContentDefines;

class EdgewareFrameProtocol {
public:

    EdgewareFrameProtocol(uint32_t setMTU = 0);
    virtual ~EdgewareFrameProtocol();

    EdgewareFrameMessages packAndSend(const std::vector<uint8_t> &packet, EdgewareFrameContent dataContent);
    std::function<void(const std::vector<uint8_t> &subPacket)> sendCallback = std::bind(
            &EdgewareFrameProtocol::sendData, this, std::placeholders::_1);

    void startUnpacker(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster);
    void stopUnpacker();
    EdgewareFrameMessages unpack(const std::vector<uint8_t> &subPacket);
    std::function<void(const std::vector<uint8_t> &packet, EdgewareFrameContent content, bool broken)> recieveCallback = std::bind(
            &EdgewareFrameProtocol::gotData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    // delete copy and move constructors and assign operators
    EdgewareFrameProtocol(EdgewareFrameProtocol const &) = delete;             // Copy construct
    EdgewareFrameProtocol(EdgewareFrameProtocol &&) = delete;                  // Move construct
    EdgewareFrameProtocol &operator=(EdgewareFrameProtocol const &) = delete;  // Copy assign
    EdgewareFrameProtocol &operator=(EdgewareFrameProtocol &&) = delete;      // Move assign




private:

    enum Frametype : uint8_t {
        type0,
        type1,
        type2
    };

    /*
    * <uint8_t> frameType
    * - 0x00 illegal - discard
    * - 0x01 frame is larger than packet
    * - 0x02 frame is less than packet
    * <uint16_t> sizeOfData (optional if frameType is 0x02)
    * <uint8_t> superFrameNo
    * <uint16_t> fragmentNo
    * <uint16_t> ofFragmentNo
    * <uint8_t> dataContent
    *  - EdgewareFrameContent
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
        EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
    };

    class Bucket {
    public:
        bool active = false;
        EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
        uint32_t timeout;
        uint16_t fragmentCounter = 0;
        uint16_t ofFragmentNo = 0;
        uint64_t deliveryOrder = 0;
        uint32_t headOfLineBlockingTimeout = 0;
        size_t fragmentSize = 0;
        std::bitset<UINT16_MAX> haveRecievedPacket;
        std::vector<uint8_t> bucketData;

        virtual ~Bucket() {

        }
    };

    struct CandidateToDeliver
    {
        uint64_t deliveryOrder;
        uint8_t bucket;
        bool broken;
        CandidateToDeliver(uint64_t k, uint8_t s, bool t) : deliveryOrder(k), bucket(s), broken(t){}
    };

    struct sortDeliveryOrder
    {
        inline bool operator() (const CandidateToDeliver& struct1, const CandidateToDeliver& struct2)
        {
            return (struct1.deliveryOrder < struct2.deliveryOrder);
        }
    };

    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(const std::vector<uint8_t> &packet, EdgewareFrameContent content, bool broken);
    EdgewareFrameMessages unpackType1(const std::vector<uint8_t> &subPacket);
    EdgewareFrameMessages unpackType2LastFrame(const std::vector<uint8_t> &subPacket);
    void unpackerWorker(uint32_t timeout);
    uint64_t superFrameRecalculator(uint16_t superframe);


    Bucket bucketList[UINT8_MAX+1];
    uint32_t bucketTimeout = 0;
    uint32_t headOfLineBlockingTimeout = 0;
    std::mutex netMtx;

    uint32_t currentMTU = 0;
    uint16_t superFrameNo = 0;
    int64_t oldSuperframeNumber = 0;
    uint64_t superFrameRecalc = 0;
    bool superFrameFirstTime = true;

    std::atomic_bool isThreadActive;
    std::atomic_bool threadActive;
};


#endif //EFP_EDGEWAREFRAMEPROTOCOL_H
