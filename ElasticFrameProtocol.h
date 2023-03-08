//
//
//   ______  _              _    _        ______
//  |  ____|| |            | |  (_)      |  ____|
//  | |__   | |  __ _  ___ | |_  _   ___ | |__  _ __  __ _  _ __ ___    ___
//  |  __|  | | / _` |/ __|| __|| | / __||  __|| '__|/ _` || '_ ` _ \  / _ \
//  | |____ | || (_| |\__ \| |_ | || (__ | |   | |  | (_| || | | | | ||  __/
//  |______||_| \__,_||___/ \__||_| \___||_|   |_|   \__,_||_| |_| |_| \___|
//                                                                  Protocol
// Copyright Edgeware AB 2020, Agile Content 2021-2022
//
// For more information, example usage and plug-ins please see
// https://github.com/agilecontent/efp
//

// Prefixes used
// m class member
// p pointer (*)
// r reference (&)
// h part of header
// l local scope

// Nomenclature used
// SuperFrame == The original data + all associated information about the data
// Fragment == A part (Fragment) of the original data + a header describing what part of the super frame it belongs to
// Bucket == Part of the receiver where the received fragments are put, and the SuperFrame is assembled.

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
#include <any>

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

///Enable or disable the APIs used by the unit tests
#define UNIT_TESTS

///The size of the circular buffer. Must be contiguous set bits defining the size  0b1111111111111 == 8191
#define CIRCULAR_BUFFER_SIZE 0b1111111111111

/// Flag defines used py EFP
#define NO_FLAGS        0b00000000 // Normal operation
#define INLINE_PAYLOAD  0b00010000 // If the frame contains inline payload the flag must be set
#define PRIORITY_P0     0b00000000 // Low priority (not implemented)
#define PRIORITY_P1     0b00100000 // Normal priority (not implemented)
#define PRIORITY_P2     0b01000000 // High priority (not implemented)
#define PRIORITY_P3     0b01100000 // God-mode priority (not implemented)
#define UNDEFINED_FLAG  0b10000000 // TBD

#define EFP_MAJOR_VERSION 0
#define EFP_MINOR_VERSION 4

// Bitwise operations are used on members therefore the namespace is wrapping enum instead of 'enum class'
/// Definition of the data types supported by EFP
namespace ElasticFrameContentNamespace {
    /// Payload data types
    // Payload data defines ----- START ------
    enum ElasticFrameContentDefines : uint8_t {
        unknown     = 0x00, //Unknown content               //code
        privatedata = 0x01, //Any user defined format       //USER (not needed, the 32-bits may be used to define the private data)
        adts        = 0x02, //Mpeg-4 AAC ADTS framing       //ADTS (not needed)
        mpegts      = 0x03, //ITU-T H.222 188byte TS        //TSDT (not needed)
        mpegpes     = 0x04, //ITU-T H.222 PES packets       //MPES (not needed)
        jpeg2000    = 0x05, //ITU-T T.800 Annex M           //J2KV (not needed)
        jpeg        = 0x06, //ITU-T.81                      //JPEG (not needed)
        jpegxs      = 0x07, //ISO/IEC 21122-3               //JPXS (not needed)
        pcmaudio    = 0x08, //AES-3 framing                 //AES3 (not needed)
        ndi         = 0x09, //*TBD*                         //NNDI (not needed)
        json        = 0x0a, //RFC 8259                      //JSON (not needed)

        // Formats defined below (MSB='1') must also use 'code' to define the data format for the super frame

        efpsig  = 0x80, //content format    //JSON / BINR
        didsdid = 0x81, //FOURCC format     //(FOURCC) (Must be the fourcc code for the format used)
        sdi     = 0x82, //FOURCC format     //(FOURCC) (Must be the fourcc code for the format used)
        h264    = 0x83, //ITU-T H.264       //ANXB = Annex B framing / AVCC = AVCC framing
        h265    = 0x84, //ITU-T H.265       //ANXB = Annex B framing / AVCC = AVCC framing
        h266    = 0x85, //ITU-T H.266       //ANXB = Annex B framing / AVCC = AVCC framing
        av1     = 0x86, //AOM AV1           //XOBU = Open Bitstream Units framing
        mp4     = 0x87, //ISO/IEC 14496-12  //(MP4 box name)
        aac     = 0x88, //MPEG-4 pt. 14     //ADTS, XRAW
        opus    = 0x89, //RFC 6716          //OPUS
        flac    = 0x8a  //Xiph.Org          //FLAC

    };

    /// Embedded data types
    enum ElasticFrameEmbeddedContentDefines : uint8_t {
        illegal             = 0x00, //May not be used
        embeddedprivatedata = 0x01, //Private data
        h222pmt             = 0x02, //PMT from h222 PIDs should be truncated to uint8_t leaving the LSB bits only then map to EFP-streams
        mp4fragbox          = 0x03, //All boxes from a mp4 fragment excluding the payload
        lastembeddedcontent = 0x80  //If MSB is set this indicates the last embedded section
        //Data type defines below here do not allow following fragments of embedded data.
    };

    /// Embedded header define
    struct ElasticEmbeddedHeader {
        uint8_t mEmbeddedFrameType = ElasticFrameEmbeddedContentDefines::illegal;
        uint16_t mSize = 0;
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
    duplicatePacketReceived     = 2,  //If the underlying infrastructure is handing EFP duplicate segments the second packet of the duplicate will generate this warning if the
    //The superFrame is still not delivered to the host system. if it has then tooOldFragment will be returned instead.
    //in 1+n duplicatePacketReceived is triggered often. This is normal operation and just an indication the fragment is already received, can be used for statistics.
    tooOldFragment              = 3,  //If the superFrame has been delivered 100% complete or fragments of it due to a timeout and a fragment belonging to the superFrame arrives.
    failedStoppingReceiver      = 5,  //The EFP receiver failed stopping it's resources.
    type0Frame                  = 7,  //Type0 frame
    efpSignalDropped            = 8,  //EFPSignal did drop the content since it's not declared
    contentAlreadyListed        = 9,  //The content is already listed.
    contentNotListed            = 10, //The content is not listed.
    deleteContentFail           = 11  //Failed finding the content to be deleted
};

//Optional context passed to the callbacks
class ElasticFrameProtocolContext {
public:
    std::any mObject = nullptr;         // For safe object lifecycles
    void* mUnsafePointer = nullptr;     // Lightweight alternative for unsafe pointers
    uint64_t mValue = 0;                // Generic 64-bit variable
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
 * \brief ElasticFrameProtocolSender can be used to frame elementary streams to EFP fragments for transport over any network technology
 *
 * \author UnitX
 *
 * Contact: https://github.com/andersc or https://github.com/agilecontent
 *
 */
class ElasticFrameProtocolSender {
public:
    /**
    * ElasticFrameProtocolSender constructor
    *@param lSetMTU The MTU to be used by the sender. Interval 256 - UINT16_MAX
    *@param pCTX optional shared pointer to ElasticFrameProtocolContext passed to the callbacks
    *
    */
    explicit ElasticFrameProtocolSender(uint16_t lSetMTU, std::shared_ptr<ElasticFrameProtocolContext> pCTX = nullptr);

    ///Destructor
    virtual ~ElasticFrameProtocolSender();

    ///Return the version of the current implementation (Uint16)((8 MSB Major) + (8 LSB Minor))
    uint16_t getVersion() { return ((uint16_t)EFP_MAJOR_VERSION << 8) | (uint16_t)EFP_MINOR_VERSION; }

    /**
  * Converts the original data from a vector to EFP packets/fragments
  *
  * @param rPacket The Data to be sent
  * @param lDataContent ElasticFrameContent::x where x is the type of data to be sent.
  * @param lPts the PTS value of the content
  * @param lDts the DTS value of the content
  * @param lCode if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
  * @param lStreamID The EFP-stream ID the data is associated with.
  * @param lFlags signal what flags are used
  * @param rSendFunction optional send function/lambda. Overrides the callback 'sendCallback'
  * @return ElasticFrameMessages
  */
    ElasticFrameMessages
    packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent lDataContent, uint64_t lPts, uint64_t lDts,
                uint32_t lCode,
                uint8_t lStreamID, uint8_t lFlags,
                const std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t streamID)>& rSendFunction = nullptr);

    /**
    * Converts the original data from a pointer to EFP packets/fragments
    *
    * @param pPacket pointer to the data to be sent
    * @param lPacketSize size of the data to be sent
    * @param lDataContent ElasticFrameContent::x where x is the type of data to be sent.
    * @param lPts the PTS value of the content
    * @param lDts the DTS value of the content
    * @param lCode if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * @param lStreamID The EFP-stream ID the data is associated with.
    * @param lFlags signal what flags are used
    * @param rSendFunction optional send function/lambda. Overrides the callback sendCallback
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages
    packAndSendFromPtr(const uint8_t *pPacket, size_t lPacketSize, ElasticFrameContent lDataContent, uint64_t lPts,
                       uint64_t lDts,
                       uint32_t lCode, uint8_t lStreamID, uint8_t lFlags,
                       const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                                uint8_t streamID)>& rSendFunction = nullptr);


    ///WARNING. Zero copy destructivePackAndSendFromPtr is WIP.

    /**
     * Converts the original data from a pointer to EFP packets/fragments destructive
     * That means that the original data is destroyed/polluted and may not be used.
     * This method also requires that 100 bytes prior to the buffer start is free to be used
     * | 100 bytes | data to be fragmented |
     * This is a exclusive C++ method and the lambda (rSendFunction) is the only data output option
     *
     * @param pPacket pointer to the data to be sent
     * @param lPacketSize size of the data to be sent
     * @param lDataContent ElasticFrameContent::x where x is the type of data to be sent.
     * @param lPts the PTS value of the content
     * @param lDts the DTS value of the content
     * @param lCode if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
     * @param lStreamID The EFP-stream ID the data is associated with.
     * @param lFlags signal what flags are used
     * @param rSendFunction send function/lambda. Overrides the callback sendCallback
     * @return ElasticFrameMessages
     */
    ElasticFrameMessages
    destructivePackAndSendFromPtr(uint8_t *pPacket, size_t lPacketSize, ElasticFrameContent lDataContent, uint64_t lPts,
                       uint64_t lDts, uint32_t lCode, uint8_t lStreamID, uint8_t lFlags,
                                  const std::function<void(const uint8_t*, size_t)>& rSendFunction);

    /**
    * Send fragment callback
    *
    * @param rSubPacket The data to send
    * @param lStreamID EFP stream ID
    * @param pCTX optional ElasticFrameProtocolContext pointer (nullptr if not used)
    */
    std::function<void(const std::vector<uint8_t> &rSubPacket, uint8_t lStreamID, ElasticFrameProtocolContext* pCTX)> sendCallback = nullptr;

    /**
    * Send fragment callback (C-API version)
    *
    * @param pData Pointer to the data
    * @param lSize Size of the data
    * @param lStreamID EFP stream ID
    * @param lCtx context
    */
    void (*c_sendCallback)(const uint8_t *pData, size_t lSize, uint8_t lStreamID, void* lCtx);

    //Help methods ----------- START ----------
    /**
    * Add embedded data in front of a superFrame
    * These helper methods should not be used in production code
    * the embedded data should be embedded prior to filling the payload content
    *
    * @param pPacket pointer to packet (superFrame)
    * @param pPrivateData pointer to the private data
    * @param lPrivateDataSize size of private data
    * @param lContent what the private data contains
    * @param lIsLast is the last embedded data
    * @return ElasticFrameMessages
    */
    static ElasticFrameMessages addEmbeddedData(std::vector<uint8_t> *pPacket, void *pPrivateData, size_t lPrivateDataSize,
                                                ElasticEmbeddedFrameContent lContent = ElasticEmbeddedFrameContent::illegal,
                                                bool lIsLast = false);
    //Help methods ----------- END ----------

    ///Delete copy and move constructors and assign operators
    ElasticFrameProtocolSender(ElasticFrameProtocolSender const &) = delete;              // Copy construct
    ElasticFrameProtocolSender(ElasticFrameProtocolSender &&) = delete;                   // Move construct
    ElasticFrameProtocolSender &operator=(ElasticFrameProtocolSender const &) = delete;   // Copy assign
    ElasticFrameProtocolSender &operator=(ElasticFrameProtocolSender &&) = delete;        // Move assign

    //Used by unitTests ----START-----------------
#ifdef UNIT_TESTS
    static size_t getType1Size();
    static size_t getType2Size();
    void setSuperFrameNo(uint16_t lSuperFrameNo);
#endif
    //Used by unitTests ----END-----------------
protected:
    std::shared_ptr<ElasticFrameProtocolContext> mCTX = nullptr; //Place to save the context if provided
private:
    //Private methods ----- START ------
    // Used by the C - API
    void sendData(const std::vector<uint8_t> &rSubPacket, uint8_t lStreamID, ElasticFrameProtocolContext* pCTX);
    //Private methods ----- END ------

    // Internal lists and variables ----- START ------
    std::mutex mSendMtx; //Mutex protecting the send methods
    uint32_t mCurrentMTU = 0; //current MTU used by the sender
    uint16_t mSuperFrameNoGenerator = 0;
    std::vector<uint8_t> mSendBufferFixed; //Fragment buffer the size of MTU given
    std::vector<uint8_t> mSendBufferEnd; //Resized fragment buffer the size of the end fragment

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
 * \brief Class receiving EFP fragments and assembling them to elementary data (super frames)
 *
 * ElasticFrameProtocolReceiver is used for creating elementary data frames from EFP fragments
 *
 * \author UnitX
 *
 * Contact: https://github.com/andersc or https://github.com/agilecontent
 *
 */
class ElasticFrameProtocolReceiver {
public:
    /**
    * \class SuperFrame
    *
    * \brief Contains the data and all parameters associated to that data
    * The data is 32-byte aligned in memory.
    */
    class SuperFrame {
    public:
        size_t mFrameSize = 0;           // Number of bytes in frame
        uint8_t *pFrameData = nullptr;   // Received frame data
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown; // Superframe type
        bool mBroken = true;             // Is the data intact (false) or not (true)
        uint64_t mPts = UINT64_MAX;      // Presentation Time Stamp
        uint64_t mDts = UINT64_MAX;      // Decode Time Stamp
        uint32_t mCode = UINT32_MAX;     // Code as defined by ElasticFrameContentDefines
        uint8_t mStreamID = 0;           // A streamID used for stream separation of same content type (if you got more than one H264 streams for example)
        uint8_t mSource = 0;             // A transparent value 'passed by' the receivedFragment method to separate multiple parallel EFP streams
        uint8_t mFlags = NO_FLAGS;       // Flags used by the frame
        uint16_t mSuperFrameNo = 0;      // The 16 bit super frame counter. Useful for calculating the number of lost frames in HOL mode

        SuperFrame(const SuperFrame &) = delete;

        SuperFrame &operator=(const SuperFrame &) = delete;

        explicit SuperFrame(size_t lMemAllocSize) {
            int lResult = 0;
            //32 byte memory alignment for AVX2 processing.
#ifdef _WIN64
            pFrameData = (uint8_t*)_aligned_malloc(lMemAllocSize, 32);
#else
            lResult = posix_memalign((void **) &pFrameData, 32,
                                    lMemAllocSize);
#endif
            if (pFrameData && !lResult) mFrameSize = lMemAllocSize;
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

    enum class EFPReceiverMode : uint32_t {
        THREADED = 1,
        RUN_TO_COMPLETION = 2
    };

    ///Constructor (defaults to 100ms timeout of not 100% assembled super frames)
    explicit ElasticFrameProtocolReceiver(uint32_t lBucketTimeoutMasterms = 100, uint32_t lHolTimeoutMasterms = 0, std::shared_ptr<ElasticFrameProtocolContext> pCTX = nullptr, EFPReceiverMode lReceiverMode = EFPReceiverMode::THREADED);

    ///Destructor
    virtual ~ElasticFrameProtocolReceiver();

    ///Return the version of the current implementation
    uint16_t getVersion() { return ((uint16_t)EFP_MAJOR_VERSION << 8) | (uint16_t)EFP_MINOR_VERSION; }

    /**
    * Function assembling received fragments from a vector
    *
    * @param rSubPacket The data received
    * @param lFromSource the unique EFP source id. Provided by the user of the EFP protocol
    * @param rReceiveFunction optional lambda may only be used in run to completion mode
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t lFromSource, const std::function<void(pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)>& rReceiveFunction = nullptr);

    /**
    * Function assembling received fragments from a data pointer
    *
    * @param pSubPacket pointer to data
    * @param lPacketSize data size
    * @param lFromSource the unique EFP source id. Provided by the user of the EFP protocol
    * @param rReceiveFunction optional lambda may only be used in run to completion mode
    * @return ElasticFrameMessages
    */
    ElasticFrameMessages receiveFragmentFromPtr(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource, const std::function<void(pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)>& rReceiveFunction = nullptr);

    /**
    * When the EFP receiver is done assembling a super frame or times out data this callback is used.
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
    * @param pCTX Optional pointer to ElasticFrameProtocolContext may be nullptr
    */
    std::function<void(pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)> receiveCallback = nullptr;

    /**
    * Receive data callback (C-API version)
    *
    * @param pData Pointer to the data.
    * @param lSize Size of the data.
    * @param lData_content ElasticFrameContent::x where x is the type of data to be sent.
    * @param lBroken if not 0 the data integrety is broken by the underlying protocol.
    * @param lPts the PTS value of the content
    * @param lDts the DTS value of the content
    * @param lCode if MSB (uint8_t) of ElasticFrameContent is set. Then code is used to further declare the content
    * @param lStream_id The EFP-stream ID the data is associated with.
    * @param lSource The EFP source ID.
    * @param lFlags signal what flags are used
    * @param lCtx context
    */
    void (*c_receiveCallback)(uint8_t *pData,
                              size_t lSize,
                              uint8_t lData_content,
                              uint8_t lBroken,
                              uint64_t lPts,
                              uint64_t lDts,
                              uint32_t lCode,
                              uint8_t lStream_id,
                              uint8_t lSource,
                              uint8_t lFlags,
                              void* lCtx);

    /**
    * Receive embedded data callback (C-API version)
    *
    * If the EFP frame is broken this C-callback will not be triggered since the data integrity is unknown,
    * there will be no attempt to extraxt any embedded data.
    * c_receiveCallback will be triggered with broken set (meaning != 0) and if any embedded data
    * (flags & INLINE_PAYLOAD) it will be in the preamble of the broken EFP-Frame. You may try to
    * extract the data manually.
    *
    * @param pData Pointer to the data.
    * @param lSize Size of the data.
    * @param lData_type ElasticFrameEmbeddedContentDefines::x where x is the type of data received.
    * @param lPts PTS of the frame (Can be used to associate with a EFP frame).
    * @param lCtx context
    */
    void (*c_receiveEmbeddedDataCallback)(uint8_t *pData,
                                          size_t lSize,
                                          uint8_t lData_type,
                                          uint64_t lPts,
                                          void* lCtx);

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
protected:
    std::shared_ptr<ElasticFrameProtocolContext> mCTX = nullptr;
private:
    // A bucket is filled with fragments and is part of the receiver buffer
    // The bucket when finished contains all the data in order for EFP to deliver
    // a super frame. The bucket can also be delivered 'broken' if a time out is
    // triggered.

    //Bucket  ----- START ------
    class Bucket {
    public:
        bool mActive = false; // Is this bucket in use?
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown;
        uint16_t mSavedSuperFrameNo = 0; // The SuperFrameNumber using this bucket.
        int64_t mTimeout = 0;  // A time out counter. Will most likely be changed to a uint64_t and compared to steady_clock
        uint16_t mFragmentCounter = 0; // Current amount of fragments filled in this bucket
        uint16_t mOfFragmentNo = 0; // Number of fragments expected in this bucket before 100% full
        uint64_t mDeliveryOrder = UINT64_MAX; // The super frame counter
        size_t mFragmentSize = 0;   // Size in bytes for fragments
        uint64_t mPts = UINT64_MAX; // Presentation Time Stamp
        uint64_t mDts = UINT64_MAX; // Decode Time Stamp
        uint32_t mCode = UINT32_MAX; // Code as defined by the content type
        uint8_t mStream = 0; // TBD
        uint8_t mSource = 0; // TBD
        uint8_t mFlags = NO_FLAGS; // Flags used
        std::bitset<UINT16_MAX> mHaveReceivedFragment; // Bit-mask representing the fragments received
        pFramePtr mBucketData = nullptr; //Pointer to the super frame data
    };
    //Bucket ----- END ------

    //Stream list ----- START ------
    struct Stream {
        uint32_t mCode = UINT32_MAX;
        ElasticFrameContent mDataContent = ElasticFrameContent::unknown;
    };
    //Stream list ----- END ------

    //Private methods ----- START ------

    // Stop the receiver worker
    ElasticFrameMessages stopReceiver();

    // C-API callback. If C++ is used this is a dummy callback
    void gotData(pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX);

    // Method unpacking Type1 fragments
    ElasticFrameMessages unpackType1(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource);

    // Method unpacking Type2 fragments
    ElasticFrameMessages unpackType2(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource);

    // Method unpacking Type3 fragments
    ElasticFrameMessages unpackType3(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource);

    // The worker thread assembling unpacked fragments and delivering the superFrames to the deliveryWorker()
    void receiverWorker();

    // The worker thread acting as a bridge between EFP and the user
    void deliveryWorker();

    // If EFP is put into 'run to completion' this is the method called to deal with all data in the buffers + new data
    void runToCompletionMethod(const std::function<void(pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)>& rReceiveFunction);

    // Recalculate the 16-bit vector to a 64-bit vector
    uint64_t superFrameRecalculator(uint16_t lSuperFrame);
    // Private methods ----- END ------

    // Internal lists and variables ----- START ------
    Stream mStreams[UINT8_MAX];                 // EFP-Stream information store
    std::map<uint64_t , Bucket*> mBucketMap;    // Sorted (super frame number) pointers to mBucketList items
    Bucket *mBucketList;                        // Internal queue where all fragments are stored and super frames delivered from
    uint32_t mBucketTimeoutms = 0;              // Time out passed to receiver (in milliseconds)
    uint32_t mHeadOfLineBlockingTimeoutms = 0;  // HOL time out passed to receiver (in milliseconds)
    std::mutex mNetMtx;                         // Mutex protecting the bucket queue

    // Various counters to keep track of the different frames
    uint16_t mOldSuperFrameNumber = 0;
    uint64_t mSuperFrameRecalc = 0;
    bool mSuperFrameFirstTime = true;

    // Receiver thread management
    std::atomic_bool mIsWorkerThreadActive = {false};
    std::atomic_bool mIsDeliveryThreadActive = {false};
    std::atomic_bool mThreadActive = {false};

    //Delivery variables
    bool mDeliveryHOLFirstRun = true;
    uint64_t mNextExpectedFrameNumber = 0;

    std::mutex mReceiveMtx;                     //Mutex protecting the receive part
    std::deque<pFramePtr> mSuperFrameQueue;
    std::mutex mSuperFrameMtx;
    std::condition_variable mSuperFrameDeliveryConditionVariable;
    bool mSuperFrameReady = false;
    EFPReceiverMode mCurrentMode;
    // Internal lists and variables ----- END ------
};

#endif //EFP_ELASTICFRAMEPROTOCOL_H
