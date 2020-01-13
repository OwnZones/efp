// ElasticFrameProtocol
//
// UnitX Edgeware AB 2020
//

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// h part of header
// l local scope

#ifndef EFP_ELASTICFRAMEPROTOCOL_H
#define EFP_ELASTICFRAMEPROTOCOL_H

#include <cstdint>
#include <vector>
#include <iostream>
#include <sstream>
#include <climits>
#include <cstring>
#include <cmath>
#include <thread>

#ifndef _WIN64

#include <unistd.h>

#endif

#include <functional>
#include <bitset>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <deque>
#include <condition_variable>
#include <chrono>

///Generate the uint32_t 'code' out of 4 characters provided
#define EFP_CODE(c0, c1, c2, c3) (((c0)<<24) | ((c1)<<16) | ((c2)<<8) | (c3))

///Enable or disable the APIs used by the unit tests
#define UNIT_TESTS

///The size of the circular buffer. Must be contiguous set bits defining the size  0b1111111111111 == 8191
#define CIRCULAR_BUFFER_SIZE 0b1111111111111

/// Flag defines used py EFP
#define NO_FLAGS 0b00000000
#define INLINE_PAYLOAD 0b00010000
#define UNDEFINED_FLAG1 0b00100000
#define UNDEFINED_FLAG2 0b01000000
#define UNDEFINED_FLAG3 0b10000000

#define EFP_MAJOR_VERSION 0
#define EFP_MINOR_VERSION 1

//bitwise operations are used on members therefore the namespace is wrapping enum instead of 'enum class'
/// Definition of the data types supported by EFP
namespace ElasticFrameContentNamespace {
    ///Payload data types
    //Payload data defines ----- START ------
    enum ElasticFrameContentDefines : uint8_t {
        unknown,                //Standard                      //code
        privatedata,            //Any user defined format       //USER (not needed)
        adts,                   //Mpeg-4 AAC ADTS framing       //ADTS (not needed)
        mpegts,                 //ITU-T H.222 188byte TS        //TSDT (not needed)
        mpegpes,                //ITU-T H.222 PES packets       //MPES (not needed)
        jpeg2000,               //ITU-T T.800 Annex M           //J2KV (not needed)
        jpeg,                   //ITU-T.81                      //JPEG (not needed)
        jpegxs,                 //ISO/IEC 21122-3               //JPXS (not needed)
        pcmaudio,               //AES-3 framing                 //AES3 (not needed)
        ndi,                    //*TBD*                         //NNDI (not needed)

        //Formats defined below (MSB='1') must also use 'code' to define the data format in the superframe

        didsdid = 0x80,           //FOURCC format                 //(FOURCC) (Must be the fourcc code for the format used)
        sdi,                    //FOURCC format                 //(FOURCC) (Must be the fourcc code for the format used)
        h264,                   //ITU-T H.264                   //ANXB = Annex B framing / AVCC = AVCC framing
        h265                    //ITU-T H.265                   //ANXB = Annex B framing / AVCC = AVCC framing
    };

    ///Embedded data types
    //Embedded data defines ----- START ------
    enum ElasticFrameEmbeddedContentDefines : uint8_t {
        illegal,                //may not be used
        embeddedprivatedata,    //private data
        h222pmt,                //pmt from h222 pids should be truncated to uint8_t leaving the LSB bits only then map to streams
        mp4fragbox,             //All boxes from a mp4 fragment excluding the payload
        lastembeddedcontent = 0x80
        //defines below here do not allow following embedded data.
    };

    ///Embedded header define
    //Embedded data header ----- START ------
    struct ElasticEmbeddedHeader {
        uint8_t embeddedFrameType = ElasticFrameEmbeddedContentDefines::illegal;
        uint16_t size = 0;
    };
}
using ElasticFrameContent = ElasticFrameContentNamespace::ElasticFrameContentDefines;
using ElasticEmbeddedFrameContent = ElasticFrameContentNamespace::ElasticFrameEmbeddedContentDefines;

// EFP Messages
// Negative numbers are errors
// 0 == No error
// Positive numbers are informative
/// ElasticFrameMessages definitions
enum class ElasticFrameMessages : int16_t {
    tooLargeFrame = -10000,     //The frame is to large for EFP sender to handle
    tooLargeEmbeddedData,       //The embedded data frame is too large.
    unknownFrameType,           //The frame type is unknown by EFP receiver
    frameSizeMismatch,          //The receiver received data less than the header size
    internalCalculationError,   //The sender encountered a condition it can't handle
    endOfPacketError,           //The receiver received a type2 fragment not saying it was the last
    bufferOutOfBounds,          //The receiver circular buffer has wrapped around and all data in the buffer is from now untrusted also data prior to this may have been wrong.
    //This error can be triggered if there is a super high data rate data coming in with a large gap/loss of the incoming fragments in the flow
            bufferOutOfResources,       //This error is indicating there are no more buffer resources. In the unlikely event where all frames miss fragment(s) and the timeout is set high
    //then broken superFrames will be buffered and new incoming data will claim buffers. When there are no more buffers to claim this error will be triggered.
            reservedPTSValue,           //UINT64_MAX is a EFP reserved value
    reservedDTSValue,           //UINT64_MAX is a EFP reserved value
    reservedCodeValue,          //UINT32_MAX is a EFP reserved value
    reservedStreamValue,        //0 is a EFP reserved value for signaling manifests
    memoryAllocationError,      //Failed allocating system memory. This is fatal and results in unknown behaviour.
    illegalEmbeddedData,        //illegal embedded data
    type1And3SizeError,         //Type1 and Type3 must have the same header size
    wrongMode,                  //mode is set to receiver when using the class as sender or the other way around
    receiverNotRunning,         //The EFP receiver is not running
    dtsptsDiffToLarge,          //PTS - DTS > UINT32_MAX

    noError = 0,

    notImplemented,             //feature/function/level/method/system aso. not implemented.
    duplicatePacketReceived,    //If the underlying infrastructure is handing EFP duplicate segments the second packet of the duplicate will generate this error if the
    //the superFrame is still not delivered to the host system. if it has then tooOldFragment will be returned instead.
            tooOldFragment,             //if the superFrame has been delivered 100% complete or fragments of it due to a timeout and a fragment belonging to the superFrame arrives then it's
    //discarded and the tooOldFragment is triggered.
            receiverAlreadyStarted,     //The EFP receiver is already started no need to start it again. (Stop it and start it again to change parameters)
    failedStoppingReceiver,     //The EFP receiver failed stopping it's resources.
    parameterError,             //When starting the receiver the parameters given where not valid.
    type0Frame                  //Type0 frame
};

///The mode set when constructing the class
enum class ElasticFrameMode : uint8_t {
    unknown,
    sender,
    receiver,
};

/**
 * \class ElasticFrameProtocol
 *
 * \brief Class for framing media on top of transport protocols
 *
 * ElasticFrameProtocol can be used to frame elementary streams on top of network protocols such as UDP, TCP, RIST and SRT
 *
 * \author UnitX
 *
 * Contact: bitbucket:andersced
 *
 */
class ElasticFrameProtocol {
public:
    /**
    * \class SuperFrame
    *
    * \brief Reserve frame-data aligned 32-byte addresses in memory
    */
    class SuperFrame {
    public:
        size_t mFrameSize = 0;           // Number of bytes in frame
        uint8_t *pFrameData = nullptr;   // Received frame data
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown; // Superframe type
        bool mBroken = true;
        uint64_t mPts = UINT64_MAX;
        uint64_t mDts = UINT64_MAX; //Should we implement this?
        uint32_t mCode = UINT32_MAX;
        uint8_t mStream = 0;
        uint8_t mSource = 0;
        uint8_t mFlags = NO_FLAGS;

        SuperFrame(const SuperFrame &) = delete;

        SuperFrame &operator=(const SuperFrame &) = delete;

        explicit SuperFrame(size_t memAllocSize) {

            int result = 0;

            //32 byte memory alignment for AVX2 processing.

#ifdef _WIN64
            pFrameData = (uint8_t*)_aligned_malloc(memAllocSize, 32);
#else
            result = posix_memalign((void **) &pFrameData, 32,
                                    memAllocSize);
#endif

            if (pFrameData && !result) mFrameSize = memAllocSize;
        }

        virtual ~SuperFrame() {
            //Free if allocated
            if (pFrameData)
#ifdef _WIN64
                _aligned_free(pFrameData);
#else
                free(pFrameData);
#endif
        }
    };

    using pFramePtr = std::unique_ptr<SuperFrame>;

    ///Constructor
    explicit ElasticFrameProtocol(uint16_t setMTU = 0, ElasticFrameMode mode = ElasticFrameMode::receiver);

    ///Destructor
    virtual ~ElasticFrameProtocol();

    ///Return the version of the current implementation
    uint16_t getVersion() { return (EFP_MAJOR_VERSION << 8) | EFP_MINOR_VERSION; }

    /**
    * Segments data and calls the send callback
    *
    * @param rPacket The Data to be sent
    * @param dataContent ElasticFrameContent::x where x is the type of data to be sent.
    * @param pts the pts value of the content
    * @param dts the dts value of the content
    * @param code if msb (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * @param stream The EFP-stream number the data is associated with.
    * @param flags signal what flags are used
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages
    packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                uint32_t code,
                uint8_t stream, uint8_t flags);


    /**
    * Send packet callback
    *
    * @param rSubPacket The data to send
    */
    std::function<void(const std::vector<uint8_t> &rSubPacket)> sendCallback = nullptr;

    /**
    * Start the receiver worker
    *
    * @param bucketTimeoutMaster The time in bucketTimeoutMaster x 10m to wait for missing fragments
    * @param holTimeoutMaster The time in holTimeoutMaster x 10m to wait for missing superFrames
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages startReceiver(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster);

    /// Stop the reciever worker
    ElasticFrameMessages stopReceiver();

    /**
    * Method to feed the network fragments recieved
    *
    * @param rSubPacket The data recieved
    * @param fromSource the unique EFP source id. Provided by the user of the EFP protocol
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource);

    /**
    * Recieve data from the EFP worker thread
    *
    * @param rPacket superframe recieved
    * rPacket conatins
    * -> mCcontent ElasticFrameContent::x where x is the type of data to be sent.
    * -> mBbroken if true the data integrety is broken by the underlying protocol.
    * -> mPts the pts value of the content
    * -> mDts the pts value of the content
    * -> mCcode if msb (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * -> mStream The EFP-stream number the data is associated with.
    * -> mFlags signal what flags are used
    */
    std::function<void(ElasticFrameProtocol::pFramePtr &rPacket)> receiveCallback = nullptr;

    ///Delete copy and move constructors and assign operators
    ElasticFrameProtocol(ElasticFrameProtocol const &) = delete;              // Copy construct
    ElasticFrameProtocol(ElasticFrameProtocol &&) = delete;                   // Move construct
    ElasticFrameProtocol &operator=(ElasticFrameProtocol const &) = delete;   // Copy assign
    ElasticFrameProtocol &operator=(ElasticFrameProtocol &&) = delete;        // Move assign

    //Help methods ----------- START ----------
    /**
    * Add embedded data infront of a superFrame
    * These helper methods should not be used in production code
    * the embedded data should be embedded prior to filling the payload content
    *
    * @param pPacket pointer to packet (superFrame)
    * @param pPrivateData pointer to the private data
    * @param privateDataSize size of private data
    * @param content what the private data contains
    * @param isLast is the last embedded data
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages addEmbeddedData(std::vector<uint8_t> *pPacket, void *pPrivateData, size_t privateDataSize,
                                         ElasticEmbeddedFrameContent content = ElasticEmbeddedFrameContent::illegal,
                                         bool isLast = false);

    /**
    * Add embedded data infront of a superFrame
    * These helper methods should not be used in production code
    * the embedded data should be embedded prior to filling the payload content
    *
    * @param rPacket pointer to packet (superFrame)
    * @param pEmbeddedDataList pointer to the private data 2D array
    * @param pDataContent 1D array of the corresponding type to the extracted data (pEmbeddedDataList)
    * @param pPayloadDataPosition pointer to location of payload relative superFrame start.
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages extractEmbeddedData(pFramePtr &rPacket, std::vector<std::vector<uint8_t>> *pEmbeddedDataList,
                                             std::vector<uint8_t> *pDataContent, size_t *pPayloadDataPosition);
    //Help methods ----------- END ----------

    //Used by unitTests ----START-----------------
#ifdef UNIT_TESTS

    size_t geType1Size();

    size_t geType2Size();

#endif
    //Used by unitTests ----END-----------------

private:
    //Bucket  ----- START ------
    class Bucket {
    public:
        bool mActive = false;
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown;
        uint16_t mSavedSuperFrameNo = 0; //the SuperFrameNumber using this bucket.
        uint32_t mTimeout = 0;
        uint16_t mFragmentCounter = 0;
        uint16_t mOfFragmentNo = 0;
        uint64_t mDeliveryOrder = UINT64_MAX;
        size_t mFragmentSize = 0;
        uint64_t mPts = UINT64_MAX;
        uint64_t mDts = UINT64_MAX;
        uint32_t mCode = UINT32_MAX;
        uint8_t mStream = 0;
        uint8_t mSource = 0;
        uint8_t mFlags = NO_FLAGS;
        std::bitset<UINT16_MAX> mHaveReceivedPacket;
        pFramePtr mBucketData = nullptr;
    };
    //Bucket ----- END ------

    //Stream list ----- START ------
    struct Stream {
        uint32_t code = UINT32_MAX;
        ElasticFrameContent dataContent = ElasticFrameContent::unknown;
    };
    //Stream list ----- END ------

    //Private methods ----- START ------

    // Dummy callback
    void sendData(const std::vector<uint8_t> &rSubPacket);

    // Dummy callback
    void gotData(ElasticFrameProtocol::pFramePtr &rPacket);

    // Method dissecting Type1 fragments
    ElasticFrameMessages unpackType1(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource);

    // Method dissecting Type2 fragments
    ElasticFrameMessages unpackType2LastFrame(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource);

    // Method dissecting Type3 fragments
    ElasticFrameMessages unpackType3(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource);

    // The worker thread assembling fragments and delivering the superFrames
    void receiverWorker(uint32_t timeout);

    void deliveryWorker();

    // Recalculate the 16-bit vector to a 64-bit vector
    uint64_t superFrameRecalculator(uint16_t superFrame);
    // Private methods ----- END ------

    // Internal lists and variables ----- START ------
    Stream mStreams[UINT8_MAX]; //EFP-Stream information store
    Bucket mBucketList[
            CIRCULAR_BUFFER_SIZE + 1]; // Internal queue where all fragments are stored and superframes delivered from
    uint32_t mBucketTimeout = 0; // Time out passed to receiver
    uint32_t mHeadOfLineBlockingTimeout = 0; // HOL time out passed to receiver
    std::mutex mNetMtx; //Mutex protecting the bucket queue
    uint32_t mCurrentMTU = 0; //current MTU used by the sender
    // Various counters to keep track of the different frames
    uint16_t mSuperFrameNoGenerator = 0;
    uint16_t mOldSuperFrameNumber = 0;
    uint64_t mSuperFrameRecalc = 0;
    bool mSuperFrameFirstTime = true;
    // Receiver thread management
    std::atomic_bool mIsWorkerThreadActive;
    std::atomic_bool mIsDeliveryThreadActive;
    std::atomic_bool mThreadActive;
    // Mutex for thread safety
    std::mutex mSendMtx; //Mutex protecting the send part
    std::mutex mReceiveMtx; //Mutex protecting the recieve part
    // Current mode
    ElasticFrameMode mCurrentMode = ElasticFrameMode::unknown;
    //
    std::deque<pFramePtr> mSuperFrameQueue;
    std::mutex mSuperFrameMtx;
    std::condition_variable mSuperFrameDeliveryConditionVariable;
    bool mSuperFrameReady = false;
    // Internal lists and variables ----- END ------
};

#endif //EFP_ELASTICFRAMEPROTOCOL_H
