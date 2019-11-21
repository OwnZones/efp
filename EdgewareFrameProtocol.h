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

#define CIRCULAR_BUFFER_SIZE 0b1111111111111 //must be a continious set of set bits from LSB to MSB
//0b1111111111111 == 8191

#define EFP_MAJOR_VERSION 1
#define EFP_MINOR_VERSION 0

namespace EdgewareFrameContentNamespace {
    enum EdgewareFrameContentDefines : uint8_t {
        unknown,                //Standard                      //code
        privateData,            //Any user defined format       //USER (not needed)
        adts,                   //Mpeg-4 AAC ADTS framing       //ADTS (not needed)
        mpegts,                 //ITU-T H.222 188byte TS        //MPEG (not needed)
        mpegpes,                //ITU-T H.222 PES packets       //MPES (not needed)
        jpeg2000,               //ITU-T T.800 Annex M           //J2KV (not needed)
        jpeg,                   //ITU-T.81                      //JPEG (not needed)
        jpegxs,                 //ISO/IEC 21122-3               //JPXS (not needed)
        pcmaudio,               //AES-3 framing                 //AES3 (not needed)

        //Formats defined below (MSB='1') also uses code to define the data format in the superframe

        didsdid=0x80,           //FOURCC format                 //(FOURCC) (Must be the fourcc code for the format used)
        sdi,                    //FOURCC format                 //(FOURCC) (Must be the fourcc code for the format used)
        h264,                   //ITU-T H.264                   //ANXB = Annex B framing / AVCC = AVCC framing
        h265                    //ITU-T H.265                   //ANXB = Annex B framing / AVCC = AVCC framing
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
        bufferOutOfResources,
        duplicatePacketRecieved,
        reservedPTSValue,
        reservedCodeValue,
        memoryAllocationError,
        unpackerAlreadyStarted,
        failedStoppingUnpacker,
        parameterError
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
        size_t frameSize = 0;         //Number of bytes in frame
        uint8_t* framedata = nullptr;   //recieved frame data

        allignedFrameData(const allignedFrameData&) = delete;
        allignedFrameData & operator=(const allignedFrameData &) = delete;

        allignedFrameData(size_t memAllocSize) {
            posix_memalign((void**)&framedata, 32, memAllocSize);   //32 byte memory alignment for AVX2 processing //Winboze needs some other code.
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
    std::function<void(const std::vector<uint8_t> &subPacket)> sendCallback = nullptr;

    EdgewareFrameMessages startUnpacker(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster);
    EdgewareFrameMessages stopUnpacker();
    EdgewareFrameMessages unpack(const std::vector<uint8_t> &subPacket);
    std::function<void(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code)> recieveCallback = nullptr;

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
    //Bucket  ----- START ------
    class Bucket {
    public:
        bool active = false;
        EdgewareFrameContent dataContent = EdgewareFrameContent::unknown;
        uint16_t savedSuperFrameNo = 0; //the SuperFrameNumber using this bucket.
        uint32_t timeout = 0;
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

    //Private methods ----- START ------
    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code);
    EdgewareFrameMessages unpackType1(const std::vector<uint8_t> &subPacket);
    EdgewareFrameMessages unpackType2LastFrame(const std::vector<uint8_t> &subPacket);
    void unpackerWorker(uint32_t timeout);
    uint64_t superFrameRecalculator(uint16_t superFrame);
    //Private methods ----- END ------

    //Internal lists and variables ----- START ------

    Bucket bucketList[CIRCULAR_BUFFER_SIZE + 1]; //Internal queue
    uint32_t bucketTimeout = 0; //time out passed to reciever
    uint32_t headOfLineBlockingTimeout = 0; //HOL time out passed to reciever
    std::mutex netMtx; //Mutex protecting the queue
    uint32_t currentMTU = 0; //current MTU used by the packer
    //various counters to keep track of the different frames
    uint16_t superFrameNoGenerator = 0;
    uint16_t oldSuperframeNumber = 0;
    uint64_t superFrameRecalc = 0;
    bool superFrameFirstTime = true;
    //Reciever thread management
    std::atomic_bool isThreadActive;
    std::atomic_bool threadActive;
    //Internal lists and variables ----- END ------
};


#endif //EFP_EDGEWAREFRAMEPROTOCOL_H
