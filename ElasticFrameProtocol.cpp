//
// UnitX Edgeware AB 2020
//

#include "ElasticFrameProtocol.h"
#include "ElasticInternal.h"
#include "logger.h"

#define WORKER_THREAD_SLEEP_US 1000 * 10

//---------------------------------------------------------------------------------------------------------------------
//
//
// ElasticFrameProtocolReceiver
//
//
//---------------------------------------------------------------------------------------------------------------------

ElasticFrameProtocolReceiver::ElasticFrameProtocolReceiver(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster) {

    //Throw if you can't reserve the data.
    mBucketList = new Bucket[CIRCULAR_BUFFER_SIZE + 1];

    c_recieveCallback = nullptr;
    c_recieveEmbeddedDataCallback = nullptr;
    receiveCallback = std::bind(&ElasticFrameProtocolReceiver::gotData, this, std::placeholders::_1);

    mBucketTimeout = bucketTimeoutMaster;
    mHeadOfLineBlockingTimeout = holTimeoutMaster;
    mThreadActive = true;
    mIsWorkerThreadActive = true;
    mIsDeliveryThreadActive = true;
    std::thread(std::bind(&ElasticFrameProtocolReceiver::receiverWorker, this)).detach();
    std::thread(std::bind(&ElasticFrameProtocolReceiver::deliveryWorker, this)).detach();
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol constructed")
}

ElasticFrameProtocolReceiver::~ElasticFrameProtocolReceiver() {

    //We allocated so this cant be a nullptr
    delete[] mBucketList;

    // If our worker is active we need to stop it.
    if (mThreadActive) {
        if (stopReceiver() != ElasticFrameMessages::noError) {
            EFP_LOGGER(true, LOGG_ERROR, "Failed stopping worker thread.")
        }
    }
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol destruct")
}

// C API callback. Dummy callback if C++
void ElasticFrameProtocolReceiver::gotData(ElasticFrameProtocolReceiver::pFramePtr &rPacket) {
    if (c_recieveCallback) {
        size_t payloadDataPosition = 0;
        if (c_recieveEmbeddedDataCallback && (rPacket->mFlags & (uint8_t)INLINE_PAYLOAD) && !rPacket->mBroken) {
            std::vector<std::vector<uint8_t>> embeddedData;
            std::vector<uint8_t> embeddedContentFlag;

            //This method is not optimal since it moves data.. and there is no need to move any data. FIXME.
            ElasticFrameMessages info = extractEmbeddedData(rPacket, &embeddedData, &embeddedContentFlag,
                                                                      &payloadDataPosition);
            if (info != ElasticFrameMessages::noError) {
                EFP_LOGGER(true, LOGG_ERROR, "extractEmbeddedData fail")
                return;
            }
            for (int x = 0; x<embeddedData.size(); x++) {
                c_recieveEmbeddedDataCallback(embeddedData[x].data(), embeddedData[x].size(), embeddedContentFlag[x], rPacket->mPts);
            }
            //Adjust the pointers for the payload callback
            if (rPacket->mFrameSize < payloadDataPosition) {
                EFP_LOGGER(true, LOGG_ERROR, "extractEmbeddedData out of bounds")
                return;
            }
        }
        c_recieveCallback(rPacket->pFrameData + payloadDataPosition, //compensate for the embedded data
                          rPacket->mFrameSize - payloadDataPosition, //compensate for the embedded data
                          rPacket->mDataContent,
                          (uint8_t) rPacket->mBroken,
                          rPacket->mPts,
                          rPacket->mDts,
                          rPacket->mCode,
                          rPacket->mStreamID,
                          rPacket->mSource,
                          rPacket->mFlags);
    } else {
        EFP_LOGGER(true, LOGG_ERROR, "Implement the recieveCallback method for the protocol to work.")
    }
}

// This method is generating a uint64_t counter from the uint16_t counter
// The maximum count-gap this calculator can handle is ((about) INT16_MAX / 2)
uint64_t ElasticFrameProtocolReceiver::superFrameRecalculator(uint16_t superFrame) {
    if (mSuperFrameFirstTime) {
        mOldSuperFrameNumber = superFrame;
        mSuperFrameRecalc = superFrame;
        mSuperFrameFirstTime = false;
        return mSuperFrameRecalc;
    }

    int16_t lChangeValue = (int16_t) superFrame - (int16_t) mOldSuperFrameNumber;
    auto lCval = (int64_t) lChangeValue;
    mOldSuperFrameNumber = superFrame;

    if (lCval > INT16_MAX) {
        lCval -= (UINT16_MAX - 1);
        mSuperFrameRecalc = mSuperFrameRecalc - lCval;
    } else {
        mSuperFrameRecalc = mSuperFrameRecalc + lCval;
    }
    return mSuperFrameRecalc;
}

// Unpack method for type1 packets. Type1 packets are the parts of superFrames larger than the MTU
ElasticFrameMessages
ElasticFrameProtocolReceiver::unpackType1(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    ElasticFrameType1 *lType1Frame = (ElasticFrameType1 *) pSubPacket;
    Bucket *pThisBucket = &mBucketList[lType1Frame->hSuperFrameNo & (uint16_t)CIRCULAR_BUFFER_SIZE];
    //EFP_LOGGER(false, LOGG_NOTIFY, "superFrameNo1-> " << unsigned(type1Frame.superFrameNo))

    // Is this entry in the buffer active? If no, create a new else continue filling the bucket with fragments.
    if (!pThisBucket->mActive) {
        //EFP_LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType1Frame->hSuperFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }

        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        mBucketMap[pThisBucket->mDeliveryOrder] = pThisBucket;
        pThisBucket->mActive = true;
        pThisBucket->mSource = fromSource;
        pThisBucket->mFlags = lType1Frame->hFrameType & (uint8_t)0xf0;
        pThisBucket->mStream = lType1Frame->hStream;
        Stream *pThisStream = &mStreams[lType1Frame->hStream];
        pThisBucket->mDataContent = pThisStream->dataContent;
        pThisBucket->mCode = pThisStream->code;
        pThisBucket->mSavedSuperFrameNo = lType1Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mDts = UINT64_MAX;
        pThisBucket->mHaveReceivedPacket[lType1Frame->hFragmentNo] = true;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType1Frame->hOfFragmentNo;
        pThisBucket->mFragmentSize = (packetSize - sizeof(ElasticFrameType1));
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lType1Frame->hFragmentNo;
        pThisBucket->mBucketData = std::make_unique<SuperFrame>(
                pThisBucket->mFragmentSize * ((size_t) lType1Frame->hOfFragmentNo + 1));
        pThisBucket->mBucketData->mFrameSize = pThisBucket->mFragmentSize * lType1Frame->hOfFragmentNo;

        if (pThisBucket->mBucketData->pFrameData == nullptr) {
            mBucketMap.erase(pThisBucket->mDeliveryOrder);
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        std::copy_n(pSubPacket + sizeof(ElasticFrameType1), packetSize - sizeof(ElasticFrameType1), pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    // There is a gap in receiving the packets. Increase the bucket size list.. if the
    // bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (lType1Frame->hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    // I'm getting a packet with data larger than the expected size
    // this can be generated by wraparound in the bucket bucketList
    // The notification about more than 50% buffer full level should already
    // be triggered by now.
    // I invalidate this bucket. The user

    if (pThisBucket->mOfFragmentNo < lType1Frame->hFragmentNo ||
        lType1Frame->hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        EFP_LOGGER(true, LOGG_FATAL, "bufferOutOfBounds")
        mBucketMap.erase(pThisBucket->mDeliveryOrder);
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    // Have I already received this packet before? (duplicate/1+n where n > 0, n can be fractional)
    if (pThisBucket->mHaveReceivedPacket[lType1Frame->hFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lType1Frame->hFragmentNo] = true;
    }

    // Let's re-set the timout and let also add +1 to the fragment counter
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mFragmentCounter++;

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // lInsertDataPointer will point to the fragment start above and fill with the incoming data

    size_t lInsertDataPointer = pThisBucket->mFragmentSize * lType1Frame->hFragmentNo;
    std::copy_n(pSubPacket + sizeof(ElasticFrameType1), packetSize - sizeof(ElasticFrameType1), pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    return ElasticFrameMessages::noError;
}

// Unpack method for type2 packets. Where we know there is also type 1 packets involved and possibly type3.
// Type2 packets are also parts of frames smaller than the MTU
// The data IS the last data of a sequence

ElasticFrameMessages ElasticFrameProtocolReceiver::unpackType2(const uint8_t *pSubPacket, size_t packetSize,
                                                               uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    ElasticFrameType2 *lType2Frame = (ElasticFrameType2 *) pSubPacket;

    if (packetSize < ((sizeof(ElasticFrameType2) + lType2Frame->hSizeOfData))) {
        return ElasticFrameMessages::type2FrameOutOfBounds;
    }

    Bucket *pThisBucket = &mBucketList[lType2Frame->hSuperFrameNo & (uint16_t)CIRCULAR_BUFFER_SIZE];

    if (!pThisBucket->mActive) {
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType2Frame->hSuperFrameNo);
        //Is this a old fragment where we already delivered the super frame?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }

        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        mBucketMap[pThisBucket->mDeliveryOrder] = pThisBucket;
        pThisBucket->mActive = true;
        pThisBucket->mSource = fromSource;
        pThisBucket->mFlags = lType2Frame->hFrameType & (uint8_t)0xf0;
        pThisBucket->mStream = lType2Frame->hStreamID;
        Stream *pThisStream = &mStreams[lType2Frame->hStreamID];
        pThisStream->dataContent = lType2Frame->hDataContent;
        pThisStream->code = lType2Frame->hCode;
        pThisBucket->mDataContent = pThisStream->dataContent;
        pThisBucket->mCode = pThisStream->code;
        pThisBucket->mSavedSuperFrameNo = lType2Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = lType2Frame->hPts;

        if (lType2Frame->hDtsPtsDiff == UINT32_MAX) {
            pThisBucket->mDts = UINT64_MAX;
        } else {
            pThisBucket->mDts = lType2Frame->hPts - (uint64_t) lType2Frame->hDtsPtsDiff;
        }

        pThisBucket->mHaveReceivedPacket[lType2Frame->hOfFragmentNo] = true;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mOfFragmentNo = lType2Frame->hOfFragmentNo;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mFragmentSize = lType2Frame->hType1PacketSize;
        size_t lReserveThis = ((pThisBucket->mFragmentSize * lType2Frame->hOfFragmentNo) +
                               lType2Frame->hSizeOfData);
        pThisBucket->mBucketData = std::make_unique<SuperFrame>(lReserveThis);
        if (pThisBucket->mBucketData->pFrameData == nullptr) {
            mBucketMap.erase(pThisBucket->mDeliveryOrder);
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        size_t lInsertDataPointer = (size_t) lType2Frame->hType1PacketSize * (size_t) lType2Frame->hOfFragmentNo;
        std::copy_n(pSubPacket + sizeof(ElasticFrameType2), lType2Frame->hSizeOfData, pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    if (lType2Frame->hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    if (pThisBucket->mOfFragmentNo < lType2Frame->hOfFragmentNo ||
        lType2Frame->hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        EFP_LOGGER(true, LOGG_FATAL, "bufferOutOfBounds")
        mBucketMap.erase(pThisBucket->mDeliveryOrder);
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    if (pThisBucket->mHaveReceivedPacket[lType2Frame->hOfFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lType2Frame->hOfFragmentNo] = true;
    }

    // Type 2 frames contains the pts and code. If for some reason the type2 packet is missing or the frame is delivered
    // Before the type2 frame arrives PTS,DTS and CODE are set to it's respective 'illegal' value. meaning you cant't use them.
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mPts = lType2Frame->hPts;

    if (lType2Frame->hDtsPtsDiff == UINT32_MAX) {
        pThisBucket->mDts = UINT64_MAX;
    } else {
        pThisBucket->mDts = lType2Frame->hPts - (uint64_t) lType2Frame->hDtsPtsDiff;
    }

    pThisBucket->mCode = lType2Frame->hCode;
    pThisBucket->mFlags = lType2Frame->hFrameType & (uint8_t)0xf0;
    pThisBucket->mFragmentCounter++;

    //set the content type
    pThisBucket->mStream = lType2Frame->hStreamID;
    Stream *thisStream = &mStreams[lType2Frame->hStreamID];
    thisStream->dataContent = lType2Frame->hDataContent;
    thisStream->code = lType2Frame->hCode;
    pThisBucket->mDataContent = thisStream->dataContent;
    pThisBucket->mCode = thisStream->code;

    // When the type2 frames are received only then is the actual size to be delivered known... Now set the real size for the bucketData
    if (lType2Frame->hSizeOfData) {
        pThisBucket->mBucketData->mFrameSize =
                (pThisBucket->mFragmentSize * lType2Frame->hOfFragmentNo) + lType2Frame->hSizeOfData;
        // Type 2 is always at the end and is always the highest number fragment
        size_t lInsertDataPointer = (size_t) lType2Frame->hType1PacketSize * (size_t) lType2Frame->hOfFragmentNo;
        std::copy_n(pSubPacket + sizeof(ElasticFrameType2), lType2Frame->hSizeOfData, pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    }

    return ElasticFrameMessages::noError;
}

// Unpack method for type3 packets. Type3 packets are the parts of frames where the reminder data does not fit a type2 packet. Then a type 3 is added
// in front of a type2 packet to catch the data overshoot.
// Type 3 frames MUST be the same header size as type1 headers (FIXME part of the opportunistic data discussion)
ElasticFrameMessages
ElasticFrameProtocolReceiver::unpackType3(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    ElasticFrameType3 *lType3Frame = (ElasticFrameType3 *) pSubPacket;
    Bucket *pThisBucket = &mBucketList[lType3Frame->hSuperFrameNo & (uint16_t)CIRCULAR_BUFFER_SIZE];

    // If there is a type3 frame it's the second last frame
    uint16_t lThisFragmentNo = lType3Frame->hOfFragmentNo - 1;

    // Is this entry in the buffer active? If no, create a new else continue filling the bucket with data.
    if (!pThisBucket->mActive) {
        //EFP_LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType3Frame->hSuperFrameNo);
        //Is this a old fragment where we already delivered the super frame?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }

        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        mBucketMap[pThisBucket->mDeliveryOrder] = pThisBucket;
        pThisBucket->mActive = true;
        pThisBucket->mSource = fromSource;
        pThisBucket->mFlags = lType3Frame->hFrameType & (uint8_t)0xf0;
        pThisBucket->mStream = lType3Frame->hStreamID;
        Stream *thisStream = &mStreams[lType3Frame->hStreamID];
        pThisBucket->mDataContent = thisStream->dataContent;
        pThisBucket->mCode = thisStream->code;
        pThisBucket->mSavedSuperFrameNo = lType3Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mDts = UINT64_MAX;
        pThisBucket->mHaveReceivedPacket[lThisFragmentNo] = true;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType3Frame->hOfFragmentNo;
        pThisBucket->mFragmentSize = lType3Frame->hType1PacketSize;
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
        size_t lReserveThis = ((pThisBucket->mFragmentSize * (lType3Frame->hOfFragmentNo - 1)) +
                               (packetSize - sizeof(ElasticFrameType3)));
        pThisBucket->mBucketData = std::make_unique<SuperFrame>(lReserveThis);

        if (pThisBucket->mBucketData->pFrameData == nullptr) {
            mBucketMap.erase(pThisBucket->mDeliveryOrder);
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        std::copy_n(pSubPacket + sizeof(ElasticFrameType3),packetSize - sizeof(ElasticFrameType3), pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    // There is a gap in receiving the packets. Increase the bucket size list.. if the
    // bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (lType3Frame->hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    // I'm getting a packet with data larger than the expected size
    // this can be generated by wraparound in the bucket bucketList
    // The notification about more than 50% buffer full level should already
    // be triggered by now.
    // I invalidate this bucket to save me but the user should be notified somehow about this state. FIXME

    if (pThisBucket->mOfFragmentNo < lThisFragmentNo || lType3Frame->hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        EFP_LOGGER(true, LOGG_FATAL, "bufferOutOfBounds")
        mBucketMap.erase(pThisBucket->mDeliveryOrder);
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    // Have I already received this packet before? (duplicate?)
    if (pThisBucket->mHaveReceivedPacket[lThisFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lThisFragmentNo] = true;
    }

    // Let's re-set the timout and let also add +1 to the fragment counter
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mFragmentCounter++;

    pThisBucket->mBucketData->mFrameSize =
            (pThisBucket->mFragmentSize * (lType3Frame->hOfFragmentNo - 1)) +
            (packetSize - sizeof(ElasticFrameType3));

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // lInsertDataPointer will point to the fragment start above and fill with the incoming data

    size_t lInsertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
    std::copy_n(pSubPacket + sizeof(ElasticFrameType3), packetSize - sizeof(ElasticFrameType3), pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    return ElasticFrameMessages::noError;
}

//This thread is delivering the super frames to the host
void ElasticFrameProtocolReceiver::deliveryWorker() {
    while (mThreadActive) {
        pFramePtr lSuperframe = nullptr;
        {
            std::unique_lock<std::mutex> lk(mSuperFrameMtx);
            if (!mSuperFrameReady)
                mSuperFrameDeliveryConditionVariable.wait(lk,
                                                          [this] { return mSuperFrameReady; }); //if mSuperFrameReady == true we already got data no need to wait for signal
            // We got a signal a frame is ready

            // pop one frame
            if (!mSuperFrameQueue.empty()) {
                lSuperframe = std::move(mSuperFrameQueue.front());
                mSuperFrameQueue.pop_front();
            }
            // If there is more to pop don't close the semaphore else do.
            if (mSuperFrameQueue.empty()) {
                mSuperFrameReady = false;
            }
        }

        if (lSuperframe) {
            receiveCallback(lSuperframe);
        }
    }
    mIsDeliveryThreadActive = false;
}

// This is the thread going trough the buckets to see if they should be delivered to
// the 'user'
void ElasticFrameProtocolReceiver::receiverWorker() {
    //Set the defaults. meaning the thread is running and there is no head of line blocking action going on.
    bool lFoundHeadOfLineBlocking = false;
    bool lFistDelivery = mHeadOfLineBlockingTimeout ==
                         0; //if HOL is used then we must receive at least two packets first to know where to start counting.
    uint32_t lHeadOfLineBlockingCounter = 0;
    uint64_t lHeadOfLineBlockingTail = 0;
    uint64_t lExpectedNextFrameToDeliver = 0;
    uint64_t lOldestFrameDelivered = 0;
    uint64_t lSavedPTS = 0;
    int64_t lTimeReference = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

    std::vector<Bucket*> lCandidates;
    lCandidates.reserve(CIRCULAR_BUFFER_SIZE); //Reserve our maximum possible number of candidates
    
//    uint32_t lTimedebuggerPointer = 0;
//    int64_t lTimeDebugger[100];

    while (mThreadActive) {
        lTimeReference += WORKER_THREAD_SLEEP_US;
        int64_t lTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t lTimeCompensation = lTimeReference - lTimeNow;

//        lTimeDebugger[lTimedebuggerPointer++]=lTimeCompensation;
//        if (! (lTimedebuggerPointer % 100)) {
//            std::cout << "Time Debug ->  ";
//            lTimedebuggerPointer=0;
//            int64_t averageTime=0;
//            for (int g = 0; g < 100; g++) {
//                averageTime += WORKER_THREAD_SLEEP_US-lTimeDebugger[g];
//                std::cout << int64_t(WORKER_THREAD_SLEEP_US-lTimeDebugger[g]) << " ";
//            }
//            std::cout << std::endl;
//            averageTime = averageTime / 100;
//            std::cout << "Average -> " << signed(averageTime) << std::endl;
//        }

        if (lTimeCompensation < 0) {
            EFP_LOGGER(true, LOGG_WARN, "Worker thread overloaded by " << signed(lTimeCompensation) << " us")
            lTimeReference = lTimeNow;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(lTimeCompensation));
        }

        mNetMtx.lock();
        uint32_t lActiveCount = mBucketMap.size();
        if (!lActiveCount) {
            mNetMtx.unlock();
            continue; //Nothing to process
        }

        bool lTimeOutTrigger = false;
        uint64_t lDeliveryOrderOldest = UINT64_MAX;

        // The default mode is not to clear any buckets
        bool lClearHeadOfLineBuckets = false;
        // If I'm in head of blocking garbage collect mode.
        if (lFoundHeadOfLineBlocking) {
            // If some one instructed me to timeout then let's timeout first
            if (lHeadOfLineBlockingCounter) {
                lHeadOfLineBlockingCounter--;
                // EFP_LOGGER(true, LOGG_NOTIFY, "Flush head countdown " << unsigned(headOfLineBlockingCounter))
            } else {
                // EFP_LOGGER(true, LOGG_NOTIFY, "Flush trigger " << unsigned(headOfLineBlockingCounter))
                // Timeout triggered.. Let's garbage collect the head.
                lClearHeadOfLineBuckets = true;
                lFoundHeadOfLineBlocking = false;
            }
        }

        lCandidates.clear();

        // Scan trough all active buckets
        for (const auto &rBucket : mBucketMap) {
            // Are we cleaning out old buckets and did we found a head to timout?
            if ((rBucket.second->mDeliveryOrder < lHeadOfLineBlockingTail) && lClearHeadOfLineBuckets) {
                //EFP_LOGGER(true, LOGG_NOTIFY, "BOOM clear-> " << unsigned(n.second->mDeliveryOrder))
                rBucket.second->mTimeout = 1;
            }
            rBucket.second->mTimeout--;
            // If the bucket is ready to be delivered or is the bucket timeout?
            if (!rBucket.second->mTimeout) {
                lTimeOutTrigger = true;
                lCandidates.emplace_back(rBucket.second);
                rBucket.second->mTimeout = 1; //We want to timeout this again if head of line blocking is on
            } else if (rBucket.second->mFragmentCounter == rBucket.second->mOfFragmentNo) {
                lCandidates.emplace_back(rBucket.second);
            }
        }

        size_t lNumCandidatesToDeliver = lCandidates.size();
        if (lNumCandidatesToDeliver) {
            lDeliveryOrderOldest = lCandidates[0]->mDeliveryOrder;
        }

        if ((!lFistDelivery && lNumCandidatesToDeliver >= 2) || lTimeOutTrigger) {
            lFistDelivery = true;
            lExpectedNextFrameToDeliver = lDeliveryOrderOldest;
        }

        // Do we got any timed out buckets or finished buckets?
        if (lNumCandidatesToDeliver && lFistDelivery) {

            //FIXME - we could implement fast HOL clearing here

            //Fast HOL candidate
            //We're not clearing buckets and we have found HOL
//            if (foundHeadOfLineBlocking && !clearHeadOfLineBuckets && headOfLineBlockingTimeout) {
//                uint64_t thisCandidate=candidates[0].deliveryOrder;
//                if (thisCandidate == )
//                for (auto &x: candidates) { //DEBUG-Keep for now
//
//                }
//            }

            //if we're waiting for a time out but all candidates are already to be delivered

            //for (auto &x: candidates) { //DEBUG-Keep for now
            //    std::cout << ">>>" << unsigned(x.deliveryOrder) << std::endl;
            //}



            // So ok we have cleared the head send it all out
            if (lClearHeadOfLineBuckets) {
                //EFP_LOGGER(true, LOGG_NOTIFY, "FLUSH HEAD!")

                uint64_t lAndTheNextIs = lCandidates[0]->mDeliveryOrder;

                for (auto &rBucket: lCandidates) {
                    if (lOldestFrameDelivered <= rBucket->mDeliveryOrder) {

                        // Here we introduce a new concept..
                        // If we are cleaning out the HOL. Only go soo far to either a gap (counter) or packet "non time out".
                        // If you remove the 'if' below HOL will clean out all super frames from the top of the buffer to the bottom of the buffer no matter the
                        // Status of the packets in between. So HOL cleaning just wipes out all waiting. This might be a wanted behaviour to avoid time-stall
                        // However packets in queue are lost since they will 'falsely' be seen as coming late and then discarded.

                        // FIXME
                        // If for example candidates.size() is larger than a certain size then maybe just flush to the end to avoid a blocking HOL situation
                        // If for example every second packet is lost then we will build a large queue

                        if (lAndTheNextIs != rBucket->mDeliveryOrder) {
                            // We did not expect this. is the bucket timed out .. then continue...
                            if (rBucket->mTimeout > 1) {
                                break;
                            }
                        }
                        lAndTheNextIs = rBucket->mDeliveryOrder + 1;

                        lOldestFrameDelivered = mHeadOfLineBlockingTimeout ? rBucket->mDeliveryOrder : 0;

                        //Create a scope for the lock
                        {
                            std::lock_guard<std::mutex> lk(mSuperFrameMtx);
                            rBucket->mBucketData->mDataContent = rBucket->mDataContent;
                            rBucket->mBucketData->mBroken =
                                    rBucket->mFragmentCounter != rBucket->mOfFragmentNo;
                            rBucket->mBucketData->mPts = rBucket->mPts;
                            rBucket->mBucketData->mDts = rBucket->mDts;
                            rBucket->mBucketData->mCode = rBucket->mCode;
                            rBucket->mBucketData->mStreamID = rBucket->mStream;
                            rBucket->mBucketData->mSource = rBucket->mSource;
                            rBucket->mBucketData->mFlags = rBucket->mFlags;
                            mSuperFrameQueue.push_back(std::move(rBucket->mBucketData));
                            mSuperFrameReady = true;
                        }
                        mSuperFrameDeliveryConditionVariable.notify_one();
                    }
                    lExpectedNextFrameToDeliver = rBucket->mDeliveryOrder + 1;
                    // std::cout << " (y) " << unsigned(expectedNextFrameToDeliver) << std::endl;
                    lSavedPTS = rBucket->mPts;
                    mBucketMap.erase(rBucket->mDeliveryOrder);
                    rBucket->mActive = false;
                    rBucket->mBucketData = nullptr;
                }
            } else {

                // In this run we have not cleared the head.. is there a head to clear?
                // We can't be in waiting for timout and we can't have a 0 time-out
                // A 0 timout means out of order delivery else we-re here.
                // So in out of order delivery we time out the buckets instead of flushing the head.

                // Check for head of line blocking only if HOL-time out is set
                if (lExpectedNextFrameToDeliver < lCandidates[0]->mDeliveryOrder &&
                    mHeadOfLineBlockingTimeout &&
                    !lFoundHeadOfLineBlocking) {

                    //for (auto &x: candidates) { //DEBUG-Keep for now
                    //    std::cout << ">>>" << unsigned(x.deliveryOrder) << " is broken " << x.broken << std::endl;
                    //}

                    lFoundHeadOfLineBlocking = true; //Found hole
                    lHeadOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                    lHeadOfLineBlockingTail = lCandidates[0]->mDeliveryOrder; //This is the tail
                    //EFP_LOGGER(true, LOGG_NOTIFY, "HOL " << unsigned(expectedNextFrameToDeliver) << " "
                    //<< unsigned(bucketList[candidates[0].bucket].deliveryOrder)
                    //<< " tail " << unsigned(headOfLineBlockingTail)
                    //<< " savedPTS " << unsigned(savedPTS))
                }

                //Deliver only when head of line blocking is cleared and we're back to normal
                if (!lFoundHeadOfLineBlocking) {
                    for (auto &rBucket: lCandidates) {

                        if (lExpectedNextFrameToDeliver != rBucket->mDeliveryOrder && mHeadOfLineBlockingTimeout) {
                            lFoundHeadOfLineBlocking = true; //Found hole
                            lHeadOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                            lHeadOfLineBlockingTail =
                                    rBucket->mDeliveryOrder; //So we basically give the non existing data a chance to arrive..
                            //EFP_LOGGER(true, LOGG_NOTIFY, "HOL2 " << unsigned(expectedNextFrameToDeliver) << " " << unsigned(x.deliveryOrder) << " tail " << unsigned(headOfLineBlockingTail))
                            break;
                        }
                        lExpectedNextFrameToDeliver = rBucket->mDeliveryOrder + 1;

                        //std::cout << unsigned(oldestFrameDelivered) << " " << unsigned(x.deliveryOrder) << std::endl;
                        if (lOldestFrameDelivered <= rBucket->mDeliveryOrder) {
                            lOldestFrameDelivered = mHeadOfLineBlockingTimeout ? rBucket->mDeliveryOrder : 0;
                            //Create a scope the lock
                            {
                                std::lock_guard<std::mutex> lk(mSuperFrameMtx);
                                rBucket->mBucketData->mDataContent = rBucket->mDataContent;
                                rBucket->mBucketData->mBroken =
                                        rBucket->mFragmentCounter != rBucket->mOfFragmentNo;
                                rBucket->mBucketData->mPts = rBucket->mPts;
                                rBucket->mBucketData->mDts = rBucket->mDts;
                                rBucket->mBucketData->mCode = rBucket->mCode;
                                rBucket->mBucketData->mStreamID = rBucket->mStream;
                                rBucket->mBucketData->mSource = rBucket->mSource;
                                rBucket->mBucketData->mFlags = rBucket->mFlags;
                                mSuperFrameQueue.push_back(std::move(rBucket->mBucketData));
                                mSuperFrameReady = true;
                            }
                            mSuperFrameDeliveryConditionVariable.notify_one();
                        }
                        lSavedPTS = rBucket->mPts;
                        mBucketMap.erase(rBucket->mDeliveryOrder);
                        rBucket->mActive = false;
                        rBucket->mBucketData = nullptr;
                    }
                }
            }
        }
        mNetMtx.unlock();

        // Is more than 75% of the buffer used. //FIXME notify the user in some way
        if (lActiveCount > (CIRCULAR_BUFFER_SIZE / 4) * 3) {
            EFP_LOGGER(true, LOGG_WARN, "Current active buckets are more than 75% of the circular buffer.")
        }
    }
    mIsWorkerThreadActive = false;
}

// Stop receiver worker thread
ElasticFrameMessages ElasticFrameProtocolReceiver::stopReceiver() {
    std::lock_guard<std::mutex> lock(mReceiveMtx);

    //Set the semaphore to stop thread
    mThreadActive = false;
    uint32_t lLockProtect = 1000;

    {
        std::lock_guard<std::mutex> lk(mSuperFrameMtx);
        mSuperFrameReady = true;
    }
    mSuperFrameDeliveryConditionVariable.notify_one();

    //check for it to actually stop
    while (mIsWorkerThreadActive || mIsDeliveryThreadActive) {
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
        if (!--lLockProtect) {
            //we gave it a second now exit anyway
            EFP_LOGGER(true, LOGG_FATAL, "Threads not stopping. Now crash and burn baby!!")
            return ElasticFrameMessages::failedStoppingReceiver;
        }
    }
    return ElasticFrameMessages::noError;
}

ElasticFrameMessages
ElasticFrameProtocolReceiver::receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource) {
    return receiveFragmentFromPtr(rSubPacket.data(), rSubPacket.size(), fromSource);
}

// Unpack method. We received a fragment of data or a full frame. Lets unpack it
ElasticFrameMessages
ElasticFrameProtocolReceiver::receiveFragmentFromPtr(const uint8_t *pSubPacket, size_t packetSize, uint8_t fromSource) {
    // Type 0 packet. Discard and continue
    // Type 0 packets can be used to fill with user data outside efp protocol packets just put a uint8_t = Frametype::type0 at position 0 and then any data.
    // Type 1 are frames larger than MTU
    // Type 2 are frames smaller than MTU
    // Type 2 packets are also used at the end of Type 1 packet superFrames
    // Type 3 frames carry the reminder of data when it's too large for type2 to carry.

    std::lock_guard<std::mutex> lock(mReceiveMtx);

    if (!(mIsWorkerThreadActive & mIsDeliveryThreadActive)) {
        EFP_LOGGER(true, LOGG_ERROR, "Receiver not running")
        return ElasticFrameMessages::receiverNotRunning;
    }

    if ((pSubPacket[0] & (uint8_t)0x0f) == Frametype::type0) {
        return ElasticFrameMessages::type0Frame;
    } else if ((pSubPacket[0] & (uint8_t)0x0f) == Frametype::type1) {
        if (packetSize < sizeof(ElasticFrameType1)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        return unpackType1(pSubPacket, packetSize, fromSource);
    } else if ((pSubPacket[0] & (uint8_t)0x0f) == Frametype::type2) {
        if (packetSize < sizeof(ElasticFrameType2)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        return unpackType2(pSubPacket, packetSize, fromSource);
    } else if ((pSubPacket[0] & (uint8_t)0x0f) == Frametype::type3) {
        if (packetSize < sizeof(ElasticFrameType3)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        return unpackType3(pSubPacket, packetSize, fromSource);
    }

    // Did not catch anything I understand
    return ElasticFrameMessages::unknownFrameType;
}

ElasticFrameMessages ElasticFrameProtocolReceiver::extractEmbeddedData(ElasticFrameProtocolReceiver::pFramePtr &rPacket,
                                                                       std::vector<std::vector<uint8_t>> *pEmbeddedDataList,
                                                                       std::vector<uint8_t> *pDataContent,
                                                                       size_t *pPayloadDataPosition) {
    bool lMoreData;
    size_t lHeaderSize = sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader);
    do {
        ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader =
                *(ElasticFrameContentNamespace::ElasticEmbeddedHeader *) (rPacket->pFrameData + *pPayloadDataPosition);
        if (lEmbeddedHeader.embeddedFrameType == ElasticEmbeddedFrameContent::illegal) {
            return ElasticFrameMessages::illegalEmbeddedData;
        }
        pDataContent->emplace_back((lEmbeddedHeader.embeddedFrameType & (uint8_t)0x7f));
        std::vector<uint8_t> lEmbeddedData(lEmbeddedHeader.size);
        std::copy_n(rPacket->pFrameData + lHeaderSize + *pPayloadDataPosition, lEmbeddedHeader.size, lEmbeddedData.data());
        pEmbeddedDataList->emplace_back(lEmbeddedData);
        lMoreData = lEmbeddedHeader.embeddedFrameType & (uint8_t)0x80;
        *pPayloadDataPosition += (lEmbeddedHeader.size + lHeaderSize);
        if (*pPayloadDataPosition >= rPacket->mFrameSize) {
            return ElasticFrameMessages::bufferOutOfBounds;
        }
    } while (!lMoreData);
    return ElasticFrameMessages::noError;
}


//---------------------------------------------------------------------------------------------------------------------
//
//
// ElasticFrameProtocolSender
//
//
//---------------------------------------------------------------------------------------------------------------------


// Constructor setting the MTU
// Limit the MTU to uint16_t MAX and UINT8_MAX min.
// The lower limit is actually type2frameSize+1, keep it at 255 for now
ElasticFrameProtocolSender::ElasticFrameProtocolSender(uint16_t setMTU) {
    c_sendCallback = nullptr;
    mSendBufferEnd.reserve(setMTU);
    mSendBufferFixed.resize(setMTU);

    if (setMTU < UINT8_MAX) {
        EFP_LOGGER(true, LOGG_ERROR, "MTU lower than " << unsigned(UINT8_MAX) << " is not accepted.")
        mCurrentMTU = UINT8_MAX;
    } else {
        mCurrentMTU = setMTU;
    }

    sendCallback = std::bind(&ElasticFrameProtocolSender::sendData, this, std::placeholders::_1, std::placeholders::_2);
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocolSender constructed")
}

ElasticFrameProtocolSender::~ElasticFrameProtocolSender() {
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocolSender destruct")
}

// Dummy callback for transmitter
void ElasticFrameProtocolSender::sendData(const std::vector<uint8_t> &rSubPacket, uint8_t streamID) {
    if (c_sendCallback) {
        c_sendCallback(rSubPacket.data(), rSubPacket.size(), streamID);
    } else {
        EFP_LOGGER(true, LOGG_ERROR, "Implement the sendCallback method for the protocol to work.")
    }
}

// Pack data method. Fragments the data and calls the sendCallback method at the host level.
ElasticFrameMessages
ElasticFrameProtocolSender::packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent,
                                        uint64_t pts,
                                        uint64_t dts,
                                        uint32_t code, uint8_t streamID, uint8_t flags,
                                        const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                                           uint8_t streamID)>& sendFunction) {
    return packAndSendFromPtr(rPacket.data(), rPacket.size(), dataContent, pts, dts, code, streamID, flags,
                              sendFunction);

}

// Pack data method. Fragments the data and calls the sendCallback method at the host level.
ElasticFrameMessages
ElasticFrameProtocolSender::packAndSendFromPtr(const uint8_t *rPacket, size_t packetSize,
                                               ElasticFrameContent dataContent,
                                               uint64_t pts, uint64_t dts,
                                               uint32_t code, uint8_t streamID, uint8_t flags,
                                               const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                                                  uint8_t streamID)>& sendFunction) {

    std::lock_guard<std::mutex> lock(mSendMtx);

    if (sizeof(ElasticFrameType1) != sizeof(ElasticFrameType3)) {
        return ElasticFrameMessages::type1And3SizeError;
    }

    if (pts == UINT64_MAX) {
        return ElasticFrameMessages::reservedPTSValue;
    }

    if (dts == UINT64_MAX) {
        return ElasticFrameMessages::reservedDTSValue;
    }

    if (code == UINT32_MAX) {
        return ElasticFrameMessages::reservedCodeValue;
    }

    if (streamID == 0 && dataContent != ElasticFrameContent::efpsig) {
        return ElasticFrameMessages::reservedStreamValue;
    }

    uint64_t lPtsDtsDiff = pts - dts;
    if (lPtsDtsDiff >= UINT32_MAX) {
        return ElasticFrameMessages::dtsptsDiffToLarge;
    }

    flags &= (uint8_t)0xf0;

    // Will the data fit?
    // We know that we can send USHRT_MAX (65535) packets
    // The last packet will be a type2 packet.. so check against current MTU multiplied with USHRT_MAX subtracting the space the protocol needs for the headers
    if (packetSize
        > (((mCurrentMTU - sizeof(ElasticFrameType1)) * (USHRT_MAX - 1)) + (mCurrentMTU - sizeof(ElasticFrameType2)))) {
        return ElasticFrameMessages::tooLargeFrame;
    }

    if ((packetSize + sizeof(ElasticFrameType2)) <= mCurrentMTU) {
        mSendBufferEnd.resize(sizeof(ElasticFrameType2) + packetSize);
        ElasticFrameType2 *pType2Frame = (ElasticFrameType2 *)mSendBufferEnd.data();
        pType2Frame->hFrameType  = Frametype::type2 | flags;
        pType2Frame->hStreamID = streamID;
        pType2Frame->hDataContent = dataContent;
        pType2Frame->hSizeOfData = (uint16_t) packetSize;
        pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType2Frame->hOfFragmentNo = 0;
        pType2Frame->hType1PacketSize = (uint16_t) packetSize;
        pType2Frame->hPts = pts;
        pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
        pType2Frame->hCode = code;
        std::copy_n(rPacket, packetSize, mSendBufferEnd.data() + sizeof(ElasticFrameType2));
        if (sendFunction) {
            sendFunction(mSendBufferEnd, streamID);
        } else {
            sendCallback(mSendBufferEnd, streamID);
        }
        mSuperFrameNoGenerator++;
        return ElasticFrameMessages::noError;
    }

    uint16_t lFragmentNo = 0;

    // The size is known for type1 packets no need to write it in any header.
    size_t lDataPayloadType1 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    size_t lDataPayloadType2 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType2));

    uint64_t lDataPointer = 0;
    uint16_t lOfFragmentNo = (uint16_t) floor(
            (double) (packetSize) / (double) (mCurrentMTU - sizeof(ElasticFrameType1)));
    uint16_t lOfFragmentNoType1 = lOfFragmentNo;
    bool lType3needed = false;
    size_t lReminderData = packetSize - (lOfFragmentNo * lDataPayloadType1);
    if (lReminderData > lDataPayloadType2) {
        // We need a type3 frame. The reminder is too large for a type2 frame
        lType3needed = true;
        lOfFragmentNo++;
    }

    ElasticFrameType1 *pType1Frame = (ElasticFrameType1*)mSendBufferFixed.data();
    pType1Frame->hFrameType = Frametype::type1 | flags;
    pType1Frame->hStream = streamID;
    pType1Frame->hSuperFrameNo = mSuperFrameNoGenerator;
    pType1Frame->hOfFragmentNo = lOfFragmentNo;

    while (lFragmentNo < lOfFragmentNoType1) {
        pType1Frame->hFragmentNo = lFragmentNo++;
        std::copy_n(rPacket + lDataPointer, lDataPayloadType1, mSendBufferFixed.data() + sizeof(ElasticFrameType1));
        lDataPointer += lDataPayloadType1;
        if (sendFunction) {
            sendFunction(mSendBufferFixed, streamID);
        } else {
            sendCallback(mSendBufferFixed, streamID);
        }
    }

    if (lType3needed) {
        lFragmentNo++;
        mSendBufferEnd.resize(sizeof(ElasticFrameType3) + lReminderData);
        ElasticFrameType3 *pType3Frame = (ElasticFrameType3*)mSendBufferEnd.data();
        pType3Frame->hFrameType = Frametype::type3 | flags;
        pType3Frame->hStreamID = streamID;
        pType3Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType3Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
        pType3Frame->hOfFragmentNo = lOfFragmentNo;
        std::copy_n(rPacket + lDataPointer, lReminderData, mSendBufferEnd.data() + sizeof(ElasticFrameType3));
        lDataPointer += lReminderData;
        if (lDataPointer != packetSize) {
            return ElasticFrameMessages::internalCalculationError;
        }

        if (sendFunction) {
            sendFunction(mSendBufferEnd, streamID);
        } else {
            sendCallback(mSendBufferEnd, streamID);
        }
    }

    // Create the last type2 packet
    size_t lDataLeftToSend = packetSize - lDataPointer;

    //Debug me for calculation errors
    if (lType3needed && lDataLeftToSend != 0) {
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lDataLeftToSend + sizeof(ElasticFrameType2) > mCurrentMTU) {
        EFP_LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << unsigned(packetSize))
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lOfFragmentNo != lFragmentNo) {
        return ElasticFrameMessages::internalCalculationError;
    }

    mSendBufferEnd.resize(sizeof(ElasticFrameType2) + lDataLeftToSend);
    ElasticFrameType2 *pType2Frame = (ElasticFrameType2 *)mSendBufferEnd.data();
    pType2Frame->hFrameType  = Frametype::type2 | flags;
    pType2Frame->hStreamID = streamID;
    pType2Frame->hDataContent = dataContent;
    pType2Frame->hSizeOfData = (uint16_t) lDataLeftToSend;
    pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
    pType2Frame->hOfFragmentNo = lOfFragmentNo;
    pType2Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    pType2Frame->hPts = pts;
    pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
    pType2Frame->hCode = code;
    std::copy_n(rPacket + lDataPointer, lDataLeftToSend, mSendBufferEnd.data() + sizeof(ElasticFrameType2));
    if (sendFunction) {
        sendFunction(mSendBufferEnd, streamID);
    } else {
        sendCallback(mSendBufferEnd, streamID);
    }
    mSuperFrameNoGenerator++;
    return ElasticFrameMessages::noError;
}

// Helper methods for embedding/extracting data in the payload part. It's not recommended to use these methods in production code as it's better to build the
// frames externally to avoid insert and copy of data.
ElasticFrameMessages ElasticFrameProtocolSender::addEmbeddedData(std::vector<uint8_t> *pPacket,
                                                                 void *pPrivateData,
                                                                 size_t privateDataSize,
                                                                 ElasticEmbeddedFrameContent content,
                                                                 bool isLast) {
    if (privateDataSize > UINT16_MAX) {
        return ElasticFrameMessages::tooLargeEmbeddedData;
    }
    ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader;
    lEmbeddedHeader.size = (uint16_t) privateDataSize;
    lEmbeddedHeader.embeddedFrameType = content;
    if (isLast)
        lEmbeddedHeader.embeddedFrameType |= ElasticEmbeddedFrameContent::lastembeddedcontent;
    pPacket->insert(pPacket->begin(), (uint8_t *) pPrivateData, (uint8_t *) pPrivateData + privateDataSize);
    pPacket->insert(pPacket->begin(), (uint8_t *) &lEmbeddedHeader,
                    (uint8_t *) &lEmbeddedHeader + sizeof(lEmbeddedHeader));
    return ElasticFrameMessages::noError;
}

// Used by the unit tests
size_t ElasticFrameProtocolSender::geType1Size() {
    return sizeof(ElasticFrameType1);
}

size_t ElasticFrameProtocolSender::geType2Size() {
    return sizeof(ElasticFrameType2);
}

void ElasticFrameProtocolSender::setSuperFrameNo(uint16_t superFrameNo) {
    mSuperFrameNoGenerator = superFrameNo;
}


// ****************************************************
//
//                      C API
//
// ****************************************************

#include <utility>

std::map<uint64_t, std::shared_ptr<ElasticFrameProtocolReceiver>> efp_receive_base_map;
std::map<uint64_t, std::shared_ptr<ElasticFrameProtocolSender>> efp_send_base_map;
uint64_t c_object_handle = {1};
std::mutex efp_send_mutex;
std::mutex efp_receive_mutex;

uint64_t efp_init_send(uint64_t mtu, void (*f)(const uint8_t *, size_t, uint8_t)) {
    efp_send_mutex.lock();
    uint64_t local_c_object_handle = c_object_handle;
    auto result = efp_send_base_map.insert(std::make_pair(local_c_object_handle,
                                                          std::make_shared<ElasticFrameProtocolSender>(mtu)));
    if (!result.first->second) {
        efp_send_mutex.unlock();
        return 0;
    }
    result.first->second->c_sendCallback = f;
    c_object_handle++;
    efp_send_mutex.unlock();
    return local_c_object_handle;
}

uint64_t efp_init_receive(uint32_t bucketTimeout,
                          uint32_t holTimeout,
                          void (*f)(uint8_t *,
                                    size_t,
                                    uint8_t,
                                    uint8_t,
                                    uint64_t,
                                    uint64_t,
                                    uint32_t,
                                    uint8_t,
                                    uint8_t,
                                    uint8_t),
                          void (*g)(uint8_t *,
                                    size_t,
                                    uint8_t,
                                    uint64_t)
                                    ) {
    efp_receive_mutex.lock();
    uint64_t local_c_object_handle = c_object_handle;
    auto result = efp_receive_base_map.insert(
            std::make_pair(local_c_object_handle, std::make_shared<ElasticFrameProtocolReceiver>(bucketTimeout, holTimeout)));
    if (!result.first->second) {
        efp_receive_mutex.unlock();
        return 0;
    }
    result.first->second->c_recieveCallback = f;
    result.first->second->c_recieveEmbeddedDataCallback = g;
    c_object_handle++;
    efp_receive_mutex.unlock();
    return local_c_object_handle;
}

int16_t efp_send_data(uint64_t efp_object,
                      const uint8_t *data,
                      size_t size,
                      uint8_t dataContent,
                      uint64_t pts,
                      uint64_t dts,
                      uint32_t code,
                      uint8_t streamID,
                      uint8_t flags) {
    efp_send_mutex.lock();
    auto efp_base = efp_send_base_map.find(efp_object)->second;
    efp_send_mutex.unlock();
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    return (int16_t) efp_base->packAndSendFromPtr(data,
                                                  size,
                                                  (ElasticFrameContent) dataContent,
                                                  pts,
                                                  dts,
                                                  code,
                                                  streamID,
                                                  flags);
}

//This is a helper method for embedding data.
//The preferred way of embedding data is to do that when assembling the frame to avoid memory copy
size_t efp_add_embedded_data(uint8_t *pDst, uint8_t *pESrc, uint8_t *pDSrc, size_t embeddedDatasize, size_t dataSize, uint8_t type, uint8_t isLast) {

    if (pDst == nullptr) {
        return (sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader) + embeddedDatasize + dataSize);
    }

    ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader;
    lEmbeddedHeader.size = (uint16_t)embeddedDatasize;
    if (isLast) {
        type |= ElasticEmbeddedFrameContent::lastembeddedcontent;
    }
    lEmbeddedHeader.embeddedFrameType = type;
    
    //Copy the header
    std::copy_n((uint8_t*)&lEmbeddedHeader, sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader), pDst);
    //Copy the embedded data
    std::copy_n(pESrc, embeddedDatasize, pDst + sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader));
    //Copy the data payoad
    std::copy_n(pDSrc, dataSize, pDst + sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader) + embeddedDatasize);
    return 0;
}

int16_t efp_receive_fragment(uint64_t efp_object,
                             const uint8_t *pSubPacket,
                             size_t packetSize,
                             uint8_t fromSource) {
    efp_receive_mutex.lock();
    auto efp_base = efp_receive_base_map.find(efp_object)->second;
    efp_receive_mutex.unlock();
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    return (int16_t) efp_base->receiveFragmentFromPtr(pSubPacket, packetSize, fromSource);
}

int16_t efp_end_send(uint64_t efp_object) {
    efp_send_mutex.lock();
    auto efp_base = efp_send_base_map.find(efp_object)->second;
    efp_send_mutex.unlock();
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    efp_send_mutex.lock();
    auto num_deleted = efp_send_base_map.erase(efp_object);
    efp_send_mutex.unlock();
    if (num_deleted) {
        return (int16_t) ElasticFrameMessages::noError;
    }
    return (int16_t) ElasticFrameMessages::efpCAPIfailure;
}

int16_t efp_end_receive(uint64_t efp_object) {
    efp_receive_mutex.lock();
    auto efp_base = efp_receive_base_map.find(efp_object)->second;
    efp_receive_mutex.unlock();
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    efp_receive_mutex.lock();
    auto num_deleted = efp_receive_base_map.erase(efp_object);
    efp_receive_mutex.unlock();
    if (num_deleted) {
        return (int16_t) ElasticFrameMessages::noError;
    }
    return (int16_t) ElasticFrameMessages::efpCAPIfailure;
}

uint16_t efp_get_version() {
    return ((uint16_t)EFP_MAJOR_VERSION << 8) | (uint16_t)EFP_MINOR_VERSION;
}




