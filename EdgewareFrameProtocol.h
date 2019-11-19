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
#include <cstring>
#include <math.h>
#include <thread>
#include <unistd.h>

#define UNIT_TESTS //Enable or disable the APIs used by the unit tests

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

#define EFP_MAJOR_VERSION 1
#define EFP_MINOR_VERSION 0



namespace EdgewareFrameContentNamespace {
    enum EdgewareFrameContentDefines : uint8_t {
        unknown,                //Standard                      //code
        privateData,            //Any user defined format       //USER
        h264b,                  //ITU-T H.264 Annex B framing   //264B
        h265b,                  //ITU-T H.265 Annex B framing   //265B
        h264a,                  //ITU-T H.264 AVCC framing      //264A
        h265a,                  //ITU-T H.265 AVCC framing      //265A
        adts,                   //Mpeg-4 AAC ADTS framing       //ADTS
        mpegts,                 //ITU-T H.222 188 TS packets    //MPEG
        mpegpes,                //ITU-T H.222 PES packets       //MPES
        jpeg2000,               //ITU-T T.800 Annex M           //J2KV
        jpeg,                   //ITU-T.81                      //JPEG
        jpegxs,                 //ISO/IEC 21122-3               //JPXS
        pcmaudio,               //AES-3 framing                 //AES3
        did_sdid,               //FOURCC format                 //SDID
        sdi                     //FOURCC format                 //SDIV
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
        duplicatePacketRecieved,
        reservedPTSValue,
        reservedCodeValue
    };
}
namespace EdgewareFrameProtocolModeNamespace {
    enum EdgewareFrameProtocolModeDefines : uint8_t {
        unknown,
        packer,
        unpacker,
    };
}

using EdgewareFrameMessages = EdgewareFrameMessagesNamespace::EdgewareFrameMessagesDefines;
using EdgewareFrameContent = EdgewareFrameContentNamespace::EdgewareFrameContentDefines;
using EdgewareFrameMode = EdgewareFrameProtocolModeNamespace::EdgewareFrameProtocolModeDefines;

class EdgewareFrameProtocol {
public:

    class allignedFrameData {
    public:
        size_t frameSize = 0;         //Number of bytes aligned for Field1/Progressive video frames
        uint8_t* framedata = nullptr;   //NV12 formated field1 or the progressive frame

        allignedFrameData(const allignedFrameData&) = delete;
        allignedFrameData & operator=(const allignedFrameData &) = delete;

        allignedFrameData(size_t memAllocSize) {
            posix_memalign((void**)&framedata, 32, memAllocSize);
            if (framedata) frameSize = memAllocSize;
        }

        virtual ~allignedFrameData() {
            //Free if ever allocated
            if (framedata) free(framedata);
        }
    };

    using framePtr = std::shared_ptr<allignedFrameData>;

    EdgewareFrameProtocol(uint16_t setMTU = 0, EdgewareFrameMode mode = EdgewareFrameMode::unpacker);
    virtual ~EdgewareFrameProtocol();

    EdgewareFrameMessages packAndSend(const std::vector<uint8_t> &packet, EdgewareFrameContent dataContent, uint64_t pts, uint32_t code);
    std::function<void(const std::vector<uint8_t> &subPacket)> sendCallback = std::bind(
            &EdgewareFrameProtocol::sendData, this, std::placeholders::_1);

    void startUnpacker(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster);
    void stopUnpacker();
    EdgewareFrameMessages unpack(const std::vector<uint8_t> &subPacket);
    std::function<void(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code)> recieveCallback = std::bind(
            &EdgewareFrameProtocol::gotData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

    // delete copy and move constructors and assign operators
    EdgewareFrameProtocol(EdgewareFrameProtocol const &) = delete;             // Copy construct
    EdgewareFrameProtocol(EdgewareFrameProtocol &&) = delete;                  // Move construct
    EdgewareFrameProtocol &operator=(EdgewareFrameProtocol const &) = delete;  // Copy assign
    EdgewareFrameProtocol &operator=(EdgewareFrameProtocol &&) = delete;      // Move assign

    //Used by unitTests ---------------------
#ifdef UNIT_TESTS
    size_t geType1Size();
    size_t geType2Size();
#endif

private:

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



    //Bucket  ----- START ------
    class Bucket {
    public:
        bool active = false;
        EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
        uint32_t timeout;
        uint16_t fragmentCounter = 0;
        uint16_t ofFragmentNo = 0;
        uint64_t deliveryOrder = 0;
        size_t fragmentSize = 0;
        uint64_t pts = UINT64_MAX;
        uint32_t code = UINT32_MAX;
        std::bitset<UINT16_MAX> haveRecievedPacket;
        framePtr bucketData = nullptr;
    };
    //Bucket ----- END ------

    //Internal buffer management ----- START ------
    struct CandidateToDeliver
    {
        uint64_t deliveryOrder;
        uint8_t bucket;
        bool broken;
        uint64_t pts;
        uint32_t code;
        CandidateToDeliver(uint64_t k, uint8_t s, bool t, uint64_t v, uint32_t l) : deliveryOrder(k), bucket(s), broken(t), pts(v), code(l){}
    };

    struct sortDeliveryOrder
    {
        inline bool operator() (const CandidateToDeliver& struct1, const CandidateToDeliver& struct2) {
            return (struct1.deliveryOrder < struct2.deliveryOrder);
        }
    };
    //Internal buffer management ----- END ------

    //Private methods ----- START ------
    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code);
    EdgewareFrameMessages unpackType1(const std::vector<uint8_t> &subPacket);
    EdgewareFrameMessages unpackType2LastFrame(const std::vector<uint8_t> &subPacket);
    void unpackerWorker(uint32_t timeout);
    uint64_t superFrameRecalculator(uint16_t superframe);
    //Private methods ----- END ------

    //Internal lists and variables ----- START ------
    Bucket bucketList[UINT8_MAX + 1]; //Internal queue
    uint32_t bucketTimeout = 0; //time out passed to reciever
    uint32_t headOfLineBlockingTimeout = 0; //HOL time out passed to reciever
    std::mutex netMtx; //Mutex protecting the queue

    uint32_t currentMTU = 0; //current MTU used by the packer

    //various counters to keep track of the different frames
    uint16_t superFrameNo = 0;
    int64_t oldSuperframeNumber = 0;
    uint64_t superFrameRecalc = 0;
    bool superFrameFirstTime = true;

    //Reciever thread management
    std::atomic_bool isThreadActive;
    std::atomic_bool threadActive;

    //Internal lists and variables ----- END ------
};


#endif //EFP_EDGEWAREFRAMEPROTOCOL_H
