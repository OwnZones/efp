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
#include <cmath>
#include <thread>
#include <map>

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

//Generate the C - API
#ifdef __cplusplus
extern "C" {
#endif
#include "efp_c_api/elastic_frame_protocol_c_api.h"
#ifdef __cplusplus
}
#endif

//Generate the uint32_t 'code' out of 4 characters provided
// It's already defined in elastic_frame_protocol_c_api.h -> #define EFP_CODE(c0, c1, c2, c3) (((c0)<<24) | ((c1)<<16) | ((c2)<<8) | (c3))

///Enable or disable the APIs used by the unit tests
#define UNIT_TESTS

///The size of the circular buffer. Must be contiguous set bits defining the size  0b1111111111111 == 8191
#define CIRCULAR_BUFFER_SIZE 0b1111111111111

/// Flag defines used py EFP
#define NO_FLAGS        0b00000000
#define INLINE_PAYLOAD  0b00010000
#define PRIORITY_P0     0b00000000
#define PRIORITY_P1     0b00100000
#define PRIORITY_P2     0b01000000
#define PRIORITY_P3     0b01100000
#define UNDEFINED_FLAG  0b10000000

#define EFP_MAJOR_VERSION 0
#define EFP_MINOR_VERSION 2

//bitwise operations are used on members therefore the namespace is wrapping enum instead of 'enum class'
/// Definition of the data types supported by EFP
namespace ElasticFrameContentNamespace {
    ///Payload data types
    //Payload data defines ----- START ------
    enum ElasticFrameContentDefines : uint8_t {
        unknown     = 0x00, //Unknown content               //code
        privatedata = 0x01, //Any user defined format       //USER (not needed)
        adts        = 0x02, //Mpeg-4 AAC ADTS framing       //ADTS (not needed)
        mpegts      = 0x03, //ITU-T H.222 188byte TS        //TSDT (not needed)
        mpegpes     = 0x04, //ITU-T H.222 PES packets       //MPES (not needed)
        jpeg2000    = 0x05, //ITU-T T.800 Annex M           //J2KV (not needed)
        jpeg        = 0x06, //ITU-T.81                      //JPEG (not needed)
        jpegxs      = 0x07, //ISO/IEC 21122-3               //JPXS (not needed)
        pcmaudio    = 0x08, //AES-3 framing                 //AES3 (not needed)
        ndi         = 0x09, //*TBD*                         //NNDI (not needed)

        //Formats defined below (MSB='1') must also use 'code' to define the data format in the superframe

        efpsig  = 0x80, //content format    //JSON / BINR
        didsdid = 0x81, //FOURCC format     //(FOURCC) (Must be the fourcc code for the format used)
        sdi     = 0x82, //FOURCC format     //(FOURCC) (Must be the fourcc code for the format used)
        h264    = 0x83, //ITU-T H.264       //ANXB = Annex B framing / AVCC = AVCC framing
        h265    = 0x84  //ITU-T H.265       //ANXB = Annex B framing / AVCC = AVCC framing
    };

    ///Embedded data types
    enum ElasticFrameEmbeddedContentDefines : uint8_t {
        illegal             = 0x00, //May not be used
        embeddedprivatedata = 0x01, //Private data
        h222pmt             = 0x02, //PMT from h222 PIDs should be truncated to uint8_t leaving the LSB bits only then map to EFP-streams
        mp4fragbox          = 0x03, //All boxes from a mp4 fragment excluding the payload
        lastembeddedcontent = 0x80  //If MSB is set this indicates the last embedded section
        //Data type defines below here do not allow following fragments of embedded data.
    };

    ///Embedded header define
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
    dmsgSourceMissing           = -25, //The sender handle is missing DMSG can't control the sender
    versionNotSupported         = -24, //The received version is not supported
    tooHighversion              = -23, //The received version number is too high
    lessDataThanExpected        = -22, //The data provided is less than expected
    noDataForKey                = -21, //No data found for the key value given
    dataNotJSON                 = -20, //The data is not JSON
    tooLargeFrame               = -19, //The frame is to large for EFP sender to handle
    tooLargeEmbeddedData        = -18, //The embedded data frame is too large.
    unknownFrameType            = -17, //The frame type is unknown by EFP receiver
    frameSizeMismatch           = -16, //The receiver received data less than the header size
    internalCalculationError    = -15, //The sender encountered a condition it can't handle
    notDefinedError             = -14, //Not defined
    bufferOutOfBounds           = -13, //The receiver circular buffer has wrapped around and all data in the buffer is from now untrusted also data prior to this may have been wrong.
                                       //This error can be triggered if there is a super high data rate data coming in with a large gap/loss of the incoming fragments in the flow
                                       //Then broken superFrames will be buffered and new incoming data will claim buffers. When there are no more buffers to claim this error will be triggered.
    bufferOutOfResources        = -12, //This error is indicating there are no more buffer resources. In the unlikely event where all frames miss fragment(s) and the timeout is set high
    reservedPTSValue            = -11, //UINT64_MAX is a EFP reserved value
    reservedDTSValue            = -10, //UINT64_MAX is a EFP reserved value
    reservedCodeValue           = -9,  //UINT32_MAX is a EFP reserved value
    reservedStreamValue         = -8,  //0 is a EFP reserved value for signaling manifests
    memoryAllocationError       = -7,  //Failed allocating system memory. This is fatal and results in unknown behaviour.
    illegalEmbeddedData         = -6,  //Illegal embedded data
    type1And3SizeError          = -5,  //Type1 and Type3 must have the same header size
    receiverNotRunning          = -4,  //The EFP receiver is not running
    dtsptsDiffToLarge           = -3,  //PTS - DTS > UINT32_MAX
    type2FrameOutOfBounds       = -2,  //The user provided a packet with type2 data but the size of the packet is smaller than the declared content
    efpCAPIfailure              = -1,  //Failure in the C-API

    noError                     = 0,  //No error or information

    notImplemented              = 1,  //Feature/function/level/method/system aso. not implemented.
    duplicatePacketReceived     = 2,  //If the underlying infrastructure is handing EFP duplicate segments the second packet of the duplicate will generate this error if the
                                      //The superFrame is still not delivered to the host system. if it has then tooOldFragment will be returned instead.
                                      //Discarded and the tooOldFragment is triggered.
    tooOldFragment              = 3,  //If the superFrame has been delivered 100% complete or fragments of it due to a timeout and a fragment belonging to the superFrame arrives then it's
    failedStoppingReceiver      = 5,  //The EFP receiver failed stopping it's resources.
    type0Frame                  = 7,  //Type0 frame
    efpSignalDropped            = 8,  //EFPSignal did drop the content since it's not declared
    contentAlreadyListed        = 9,  //The content is already noted as listed.
    contentNotListed            = 10, //The content is not noted as listed.
    deleteContentFail           = 11  //Failed finding the content to be deleted
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// ElasticFrameProtocolSender
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class ElasticFrameProtocolSender
 *
 * \brief
 *
 * ElasticFrameProtocolSender can be used to frame elementary streams to EFP fragments for transport over any network technology
 *
 * \author UnitX
 *
 * Contact: bitbucket:andersced
 *
 */
class ElasticFrameProtocolSender {
public:

    /**
    * ElasticFrameProtocolSender constructor
    *@param setMTU The MTU to be used by the sender. Interval 256 - UINT16_MAX
    *
    */
    explicit ElasticFrameProtocolSender(uint16_t setMTU);

    ///Destructor
    virtual ~ElasticFrameProtocolSender();

    ///Return the version of the current implementation (Uint16)((8 MSB Major) + (8 LSB Minor))
    uint16_t getVersion() { return ((uint16_t)EFP_MAJOR_VERSION << 8) | (uint16_t)EFP_MINOR_VERSION; }

    /**
  * Segments data and call the send callback when the data is a vector
  *
  * @param rPacket The Data to be sent
  * @param dataContent ElasticFrameContent::x where x is the type of data to be sent.
  * @param pts the PTS value of the content
  * @param dts the DTS value of the content
  * @param code if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
  * @param streamID The EFP-stream ID the data is associated with.
  * @param flags signal what flags are used
  * @param sendFunction optional send function/lambda. Overrides the callback sendCallback
  * @return ElasticFrameMessages
  */
    ElasticFrameMessages
    packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts, uint64_t dts,
                uint32_t code,
                uint8_t streamID, uint8_t flags,
                const std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t streamID)>& sendFunction = nullptr);

    /**
    * Segments data and call the send callback when the data is a pointer
    *
    * @param pPacket pointer to the data to be sent
    * @param packetSize size of the data to be sent
    * @param dataContent ElasticFrameContent::x where x is the type of data to be sent.
    * @param pts the PTS value of the content
    * @param dts the DTS value of the content
    * @param code if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * @param streamID The EFP-stream ID the data is associated with.
    * @param flags signal what flags are used
    * @param sendFunction optional send function/lambda. Overrides the callback sendCallback
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages
    packAndSendFromPtr(const uint8_t *pPacket, size_t packetSize, ElasticFrameContent dataContent, uint64_t pts,
                       uint64_t dts,
                       uint32_t code, uint8_t streamID, uint8_t flags,
                       const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                          uint8_t streamID)>& sendFunction = nullptr);


    /**
    * Send packet callback
    *
    * @param rSubPacket The data to send
    * @streamID EFP stream ID
    */
    std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t streamID)> sendCallback = nullptr;

    /**
    * Send packet callback (C-API version)
    *
    * @data Pointer to the data
    * @size Size of the data
    * @stream_id EFP stream ID
    */
    void (*c_sendCallback)(const uint8_t *data, size_t size, uint8_t stream_id);

    //Help methods ----------- START ----------
    /**
    * Add embedded data in front of a superFrame
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
    static ElasticFrameMessages addEmbeddedData(std::vector<uint8_t> *pPacket, void *pPrivateData, size_t privateDataSize,
                                         ElasticEmbeddedFrameContent content = ElasticEmbeddedFrameContent::illegal,
                                         bool isLast = false);
    //Help methods ----------- END ----------

    ///Delete copy and move constructors and assign operators
    ElasticFrameProtocolSender(ElasticFrameProtocolSender const &) = delete;              // Copy construct
    ElasticFrameProtocolSender(ElasticFrameProtocolSender &&) = delete;                   // Move construct
    ElasticFrameProtocolSender &operator=(ElasticFrameProtocolSender const &) = delete;   // Copy assign
    ElasticFrameProtocolSender &operator=(ElasticFrameProtocolSender &&) = delete;        // Move assign

    //Used by unitTests ----START-----------------
#ifdef UNIT_TESTS

    static size_t geType1Size();

    static size_t geType2Size();

    void setSuperFrameNo(uint16_t superFrameNo);

#endif
    //Used by unitTests ----END-----------------

private:

    //Private methods ----- START ------
    // Used by the C - API
    void sendData(const std::vector<uint8_t> &rSubPacket, uint8_t streamID);
    //Private methods ----- END ------

    // Internal lists and variables ----- START ------
    std::mutex mSendMtx; //Mutex protecting the send part
    uint32_t mCurrentMTU = 0; //current MTU used by the sender
    uint16_t mSuperFrameNoGenerator = 0;
    std::vector<uint8_t> mSendBufferFixed;
    std::vector<uint8_t> mSendBufferEnd;
    // Internal lists and variables ----- END -----
};

//---------------------------------------------------------------------------------------------------------------------
//
//
// ElasticFrameProtocolReceiver
//
//
//---------------------------------------------------------------------------------------------------------------------

/**
 * \class ElasticFrameProtocolReceiver
 *
 * \brief Class for receiving EFP fragments and assembling them to elementary data
 *
 * ElasticFrameProtocolReceiver is used for creating elementary data frames from EFP fragments
 *
 * \author UnitX
 *
 * Contact: bitbucket:andersced
 *
 */
class ElasticFrameProtocolReceiver {
public:
    /**
    * \class SuperFrame
    *
    * \brief Contains the data and all parameters acosiated to that data
     * The data is 32-byte aligned in memory. 
    */
    class SuperFrame {
    public:
        size_t mFrameSize = 0;           // Number of bytes in frame
        uint8_t *pFrameData = nullptr;   // Received frame data
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown; // Superframe type
        bool mBroken = true;
        uint64_t mPts = UINT64_MAX;
        uint64_t mDts = UINT64_MAX;
        uint32_t mCode = UINT32_MAX;
        uint8_t mStreamID = 0;
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

    ///Constructor (defaults to 100ms timeout)
    explicit ElasticFrameProtocolReceiver(uint32_t bucketTimeoutMaster = 10, uint32_t holTimeoutMaster = 0);

    ///Destructor
    virtual ~ElasticFrameProtocolReceiver();

    ///Return the version of the current implementation
    uint16_t getVersion() { return ((uint16_t)EFP_MAJOR_VERSION << 8) | (uint16_t)EFP_MINOR_VERSION; }

    /**
    * Method to feed the network fragments received when the data is a vector
    *
    * @param rSubPacket The data received
    * @param fromSource the unique EFP source id. Provided by the user of the EFP protocol
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource);

    /**
    * Method to feed the network fragments received when the data is a pointer
    *
    * @param rSubPacket The data received
    * @param fromSource the unique EFP source id. Provided by the user of the EFP protocol
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages receiveFragmentFromPtr(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource);

    /**
    * Receive data from the EFP worker thread
    *
    * @param rPacket superframe received
    * rPacket contains
    * -> pFrameData Pointer to the data.
    * -> mFrameSize Size of the data.
    * -> mCcontent ElasticFrameContent::x where x is the type of data to be sent.
    * -> mBbroken if true the data integrity is broken by the underlying protocol.
    * -> mPts the PTS value of the content
    * -> mDts the DTS value of the content
    * -> mCcode if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * -> mStreamID The EFP-stream ID the data is associated with.
    * -> mFlags signal what flags are used
    */
    std::function<void(pFramePtr &rPacket)> receiveCallback = nullptr;

    /**
    * Recieve data callback (C-API version)
    *
    * @data Pointer to the data.
    * @size Size of the data.
    * @data_content ElasticFrameContent::x where x is the type of data to be sent.
    * @broken if not 0 the data integrety is broken by the underlying protocol.
    * @pts the PTS value of the content
    * @dts the DTS value of the content
    * @code if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * @stream_id The EFP-stream ID the data is associated with.
    * @source The EFP source ID.
    * @flags signal what flags are used
    */
    void (*c_recieveCallback)(uint8_t *data,
                              size_t size,
                              uint8_t data_content,
                              uint8_t broken,
                              uint64_t pts,
                              uint64_t dts,
                              uint32_t code,
                              uint8_t stream_id,
                              uint8_t source,
                              uint8_t flags);

    /**
    * Receive embedded data callback (C-API version)
    *
    * If the EFP frame is broken this callback will not be triggered since the data integrity is unknown.
    * c_recieveCallback will be triggered with broken set (meaning != 0) and if any embedded data
    * (flags & INLINE_PAYLOAD) it will be in the preamble of the broken EFP-Frame.
    *
    * @data Pointer to the data.
    * @size Size of the data.
    * @data_type ElasticFrameEmbeddedContentDefines::x where x is the type of data received.
    * @pts PTS of the frame (Can be used to associate with a EFP frame).
    */
    void (*c_recieveEmbeddedDataCallback)(uint8_t *data,
                              size_t size,
                              uint8_t data_type,
                              uint64_t pts);

    ///Delete copy and move constructors and assign operators
    ElasticFrameProtocolReceiver(ElasticFrameProtocolReceiver const &) = delete;              // Copy construct
    ElasticFrameProtocolReceiver(ElasticFrameProtocolReceiver &&) = delete;                   // Move construct
    ElasticFrameProtocolReceiver &operator=(ElasticFrameProtocolReceiver const &) = delete;   // Copy assign
    ElasticFrameProtocolReceiver &operator=(ElasticFrameProtocolReceiver &&) = delete;        // Move assign


    //Help methods ----------- START ----------
    /**
    * Add embedded data in front of a superFrame
    * These helper methods should not be used in production code
    * the embedded data should be embedded prior to filling the payload content
    *
    * @param rPacket pointer to packet (superFrame)
    * @param pEmbeddedDataList pointer to the private data 2D array
    * @param pDataContent 1D array of the corresponding type to the extracted data (pEmbeddedDataList)
    * @param pPayloadDataPosition pointer to location of payload relative superFrame start.
    * @return ElasticFrameMessages
    */
    static ElasticFrameMessages extractEmbeddedData(pFramePtr &rPacket, std::vector<std::vector<uint8_t>> *pEmbeddedDataList,
                                             std::vector<uint8_t> *pDataContent, size_t *pPayloadDataPosition);
    //Help methods ----------- END ----------

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

    // Stop the reciever worker
    ElasticFrameMessages stopReceiver();

    // C-API callback if C++ mode it's a Dummy callback
    void gotData(pFramePtr &rPacket);

    // Method assembling Type1 fragments
    ElasticFrameMessages unpackType1(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource);

    // Method assembling Type2 fragments
    ElasticFrameMessages unpackType2(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource);

    // Method assembling Type3 fragments
    ElasticFrameMessages unpackType3(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource);

    // The worker thread assembling fragments and delivering the superFrames to the deliveryWorker()
    void receiverWorker();

    // The worker thread acting as a bridge between EFP and the user
    void deliveryWorker();

    // Recalculate the 16-bit vector to a 64-bit vector
    uint64_t superFrameRecalculator(uint16_t superFrame);
    // Private methods ----- END ------

    // Internal lists and variables ----- START ------
    Stream mStreams[UINT8_MAX]; //EFP-Stream information store
    std::map<uint64_t , Bucket*> mBucketMap; //Sorted (super frame number) pointers to mBucketList items
    Bucket *mBucketList; // Internal queue where all fragments are stored and superframes delivered from
    uint32_t mBucketTimeout = 0; // Time out passed to receiver
    uint32_t mHeadOfLineBlockingTimeout = 0; // HOL time out passed to receiver
    std::mutex mNetMtx; //Mutex protecting the bucket queue
    // Various counters to keep track of the different frames
    uint16_t mOldSuperFrameNumber = 0;
    uint64_t mSuperFrameRecalc = 0;
    bool mSuperFrameFirstTime = true;
    // Receiver thread management
    std::atomic_bool mIsWorkerThreadActive{};
    std::atomic_bool mIsDeliveryThreadActive{};
    std::atomic_bool mThreadActive{};
    // Mutex for thread safety
    std::mutex mReceiveMtx; //Mutex protecting the recieve part
    std::deque<pFramePtr> mSuperFrameQueue;
    std::mutex mSuperFrameMtx;
    std::condition_variable mSuperFrameDeliveryConditionVariable;
    bool mSuperFrameReady = false;
    // Internal lists and variables ----- END ------
};

#endif //EFP_ELASTICFRAMEPROTOCOL_H
