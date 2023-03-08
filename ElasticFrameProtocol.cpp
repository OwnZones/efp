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

ElasticFrameProtocolReceiver::ElasticFrameProtocolReceiver(uint32_t lBucketTimeoutMasterms,
                                                           uint32_t lHolTimeoutMasterms,
                                                           std::shared_ptr<ElasticFrameProtocolContext> pCTX,
                                                           EFPReceiverMode lReceiverMode) {
    //Throw if you can't reserve the data.
    mBucketList = new Bucket[CIRCULAR_BUFFER_SIZE + 1];

    mCTX = std::move(pCTX);
    c_receiveCallback = nullptr;
    c_receiveEmbeddedDataCallback = nullptr;
    receiveCallback = std::bind(&ElasticFrameProtocolReceiver::gotData, this, std::placeholders::_1,
                                std::placeholders::_2);

    mBucketTimeoutms = lBucketTimeoutMasterms;
    mHeadOfLineBlockingTimeoutms = lHolTimeoutMasterms;

    mCurrentMode = lReceiverMode;
    if (mCurrentMode == EFPReceiverMode::THREADED) {
        mThreadActive = true;
        mIsWorkerThreadActive = true;
        mIsDeliveryThreadActive = true;
        std::thread(std::bind(&ElasticFrameProtocolReceiver::receiverWorker, this)).detach();
        std::thread(std::bind(&ElasticFrameProtocolReceiver::deliveryWorker, this)).detach();
    }
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol constructed")
}

ElasticFrameProtocolReceiver::~ElasticFrameProtocolReceiver() {
    // If our worker is active we need to stop it.
    if (mThreadActive) {
        if (stopReceiver() != ElasticFrameMessages::noError) {
            EFP_LOGGER(true, LOGG_ERROR, "Failed stopping worker thread.")
        }
    }
    //We allocated so this cant be a nullptr
    delete[] mBucketList;
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol destruct")
}

// C API callback. Dummy callback if C++
void ElasticFrameProtocolReceiver::gotData(ElasticFrameProtocolReceiver::pFramePtr &rPacket,
                                           ElasticFrameProtocolContext *pCTX) {
    if (c_receiveCallback) {
        size_t payloadDataPosition = 0;
        if (c_receiveEmbeddedDataCallback && (rPacket->mFlags & (uint8_t) INLINE_PAYLOAD) && !rPacket->mBroken) {
            std::vector<std::vector<uint8_t>> embeddedData;
            std::vector<uint8_t> embeddedContentFlag;

            //This method is not optimal since it moves data.. and there is no need to move any data. FIXME.
            ElasticFrameMessages info = extractEmbeddedData(rPacket, &embeddedData, &embeddedContentFlag,
                                                            &payloadDataPosition);
            if (info != ElasticFrameMessages::noError) {
                EFP_LOGGER(true, LOGG_ERROR, "extractEmbeddedData fail")
                return;
            }
            for (size_t x = 0; x < embeddedData.size(); x++) {
                c_receiveEmbeddedDataCallback(embeddedData[x].data(), embeddedData[x].size(), embeddedContentFlag[x],
                                              rPacket->mPts, mCTX->mUnsafePointer);
            }
            //Adjust the pointers for the payload callback
            if (rPacket->mFrameSize < payloadDataPosition) {
                EFP_LOGGER(true, LOGG_ERROR, "extractEmbeddedData out of bounds")
                return;
            }
        }
        c_receiveCallback(rPacket->pFrameData + payloadDataPosition, //compensate for the embedded data
                          rPacket->mFrameSize - payloadDataPosition, //compensate for the embedded data
                          rPacket->mDataContent,
                          (uint8_t) rPacket->mBroken,
                          rPacket->mPts,
                          rPacket->mDts,
                          rPacket->mCode,
                          rPacket->mStreamID,
                          rPacket->mSource,
                          rPacket->mFlags,
                          mCTX->mUnsafePointer);
    } else {
        EFP_LOGGER(true, LOGG_ERROR, "Implement the receiveCallback method for the protocol to work.")
    }
}

// This method is generating a uint64_t counter from the uint16_t counter
// The maximum count-gap this calculator can handle is INT16_MAX
// It's not sure this is enough in all situations keep an eye on this
uint64_t ElasticFrameProtocolReceiver::superFrameRecalculator(uint16_t lSuperFrame) {
    if (mSuperFrameFirstTime) {
        mOldSuperFrameNumber = lSuperFrame;
        mSuperFrameRecalc = lSuperFrame;
        mSuperFrameFirstTime = false;
        return mSuperFrameRecalc;
    }
    int16_t lChangeValue = (int16_t) lSuperFrame - (int16_t) mOldSuperFrameNumber;
    mOldSuperFrameNumber = lSuperFrame;
    mSuperFrameRecalc = mSuperFrameRecalc + (int64_t) lChangeValue;
    return mSuperFrameRecalc;
}

// Unpack method for type1 packets. Type1 packets are the parts of superFrames larger than the MTU
ElasticFrameMessages
ElasticFrameProtocolReceiver::unpackType1(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    auto *lType1Frame = (ElasticFrameType1 *) pSubPacket;
    Bucket *pThisBucket = &mBucketList[lType1Frame->hSuperFrameNo & (uint16_t) CIRCULAR_BUFFER_SIZE];
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
        pThisBucket->mSource = lFromSource;
        pThisBucket->mFlags = lType1Frame->hFrameType & (uint8_t) 0xf0;
        pThisBucket->mStream = lType1Frame->hStream;
        Stream *pThisStream = &mStreams[lType1Frame->hStream];
        pThisBucket->mDataContent = pThisStream->mDataContent;
        pThisBucket->mCode = pThisStream->mCode;
        pThisBucket->mSavedSuperFrameNo = lType1Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedFragment.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mDts = UINT64_MAX;
        pThisBucket->mHaveReceivedFragment[lType1Frame->hFragmentNo] = true;
        pThisBucket->mTimeout = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() + (mBucketTimeoutms * 1000);
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType1Frame->hOfFragmentNo;
        pThisBucket->mFragmentSize = (lPacketSize - sizeof(ElasticFrameType1));
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lType1Frame->hFragmentNo;
        pThisBucket->mBucketData = std::make_unique<SuperFrame>(
                pThisBucket->mFragmentSize * ((size_t) lType1Frame->hOfFragmentNo + 1));
        pThisBucket->mBucketData->mFrameSize = pThisBucket->mFragmentSize * lType1Frame->hOfFragmentNo;

        if (pThisBucket->mBucketData->pFrameData == nullptr) {
            mBucketMap.erase(pThisBucket->mDeliveryOrder);
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        std::copy_n(pSubPacket + sizeof(ElasticFrameType1), lPacketSize - sizeof(ElasticFrameType1),
                    pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
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
    if (pThisBucket->mHaveReceivedFragment[lType1Frame->hFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedFragment[lType1Frame->hFragmentNo] = true;
    }

    // Increment the fragment counter
    pThisBucket->mFragmentCounter++;

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // lInsertDataPointer will point to the fragment start above and fill with the incoming data

    size_t lInsertDataPointer = pThisBucket->mFragmentSize * lType1Frame->hFragmentNo;
    std::copy_n(pSubPacket + sizeof(ElasticFrameType1), lPacketSize - sizeof(ElasticFrameType1),
                pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    return ElasticFrameMessages::noError;
}

// Unpack method for type2 packets. Where we know there is also type 1 packets involved and possibly type3.
// Type2 packets are also parts of frames smaller than the MTU
// The data IS the last data of a sequence

ElasticFrameMessages
ElasticFrameProtocolReceiver::unpackType2(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    auto *lType2Frame = (ElasticFrameType2 *) pSubPacket;

    if (lPacketSize < ((sizeof(ElasticFrameType2) + lType2Frame->hSizeOfData))) {
        return ElasticFrameMessages::type2FrameOutOfBounds;
    }

    Bucket *pThisBucket = &mBucketList[lType2Frame->hSuperFrameNo & (uint16_t) CIRCULAR_BUFFER_SIZE];

    if (!pThisBucket->mActive) {
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType2Frame->hSuperFrameNo);
        //Is this a old fragment where we already delivered the super frame?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }

        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        mBucketMap[pThisBucket->mDeliveryOrder] = pThisBucket;
        pThisBucket->mActive = true;
        pThisBucket->mSource = lFromSource;
        pThisBucket->mFlags = lType2Frame->hFrameType & (uint8_t) 0xf0;
        pThisBucket->mStream = lType2Frame->hStreamID;
        Stream *pThisStream = &mStreams[lType2Frame->hStreamID];
        pThisStream->mDataContent = lType2Frame->hDataContent;
        pThisStream->mCode = lType2Frame->hCode;
        pThisBucket->mDataContent = pThisStream->mDataContent;
        pThisBucket->mCode = pThisStream->mCode;
        pThisBucket->mSavedSuperFrameNo = lType2Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedFragment.reset();
        pThisBucket->mPts = lType2Frame->hPts;

        if (lType2Frame->hDtsPtsDiff == UINT32_MAX) {
            pThisBucket->mDts = UINT64_MAX;
        } else {
            pThisBucket->mDts = lType2Frame->hPts - (uint64_t) lType2Frame->hDtsPtsDiff;
        }

        pThisBucket->mHaveReceivedFragment[lType2Frame->hOfFragmentNo] = true;
        pThisBucket->mTimeout = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() + (mBucketTimeoutms * 1000);
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
        std::copy_n(pSubPacket + sizeof(ElasticFrameType2), lType2Frame->hSizeOfData,
                    pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
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

    if (pThisBucket->mHaveReceivedFragment[lType2Frame->hOfFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedFragment[lType2Frame->hOfFragmentNo] = true;
    }

    // Type 2 frames contains the pts and code. If for some reason the type2 packet is missing or the frame is delivered
    // Before the type2 frame arrives PTS,DTS and CODE are set to it's respective 'illegal' value. meaning you can't use them.
    pThisBucket->mPts = lType2Frame->hPts;

    if (lType2Frame->hDtsPtsDiff == UINT32_MAX) {
        pThisBucket->mDts = UINT64_MAX;
    } else {
        pThisBucket->mDts = lType2Frame->hPts - (uint64_t) lType2Frame->hDtsPtsDiff;
    }

    pThisBucket->mCode = lType2Frame->hCode;
    pThisBucket->mFlags = lType2Frame->hFrameType & (uint8_t) 0xf0;
    pThisBucket->mFragmentCounter++;

    //set the content type
    pThisBucket->mStream = lType2Frame->hStreamID;
    Stream *thisStream = &mStreams[lType2Frame->hStreamID];
    thisStream->mDataContent = lType2Frame->hDataContent;
    thisStream->mCode = lType2Frame->hCode;
    pThisBucket->mDataContent = thisStream->mDataContent;
    pThisBucket->mCode = thisStream->mCode;

    // When the type2 frames are received only then is the actual size to be delivered known... Now set the real size for the bucketData
    if (lType2Frame->hSizeOfData) {
        pThisBucket->mBucketData->mFrameSize =
                (pThisBucket->mFragmentSize * lType2Frame->hOfFragmentNo) + lType2Frame->hSizeOfData;
        // Type 2 is always at the end and is always the highest number fragment
        size_t lInsertDataPointer = (size_t) lType2Frame->hType1PacketSize * (size_t) lType2Frame->hOfFragmentNo;
        std::copy_n(pSubPacket + sizeof(ElasticFrameType2), lType2Frame->hSizeOfData,
                    pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    }
    return ElasticFrameMessages::noError;
}

// Unpack method for type3 packets. Type3 packets are the parts of frames where the reminder data does not fit a type2 packet. Then a type 3 is added
// in front of a type2 packet to catch the data overshoot.
// Type 3 frames MUST be the same header size as type1 headers (FIXME part of the opportunistic data discussion)
ElasticFrameMessages
ElasticFrameProtocolReceiver::unpackType3(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    auto *lType3Frame = (ElasticFrameType3 *) pSubPacket;
    Bucket *pThisBucket = &mBucketList[lType3Frame->hSuperFrameNo & (uint16_t) CIRCULAR_BUFFER_SIZE];

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
        pThisBucket->mSource = lFromSource;
        pThisBucket->mFlags = lType3Frame->hFrameType & (uint8_t) 0xf0;
        pThisBucket->mStream = lType3Frame->hStreamID;
        Stream *thisStream = &mStreams[lType3Frame->hStreamID];
        pThisBucket->mDataContent = thisStream->mDataContent;
        pThisBucket->mCode = thisStream->mCode;
        pThisBucket->mSavedSuperFrameNo = lType3Frame->hSuperFrameNo;
        pThisBucket->mHaveReceivedFragment.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mDts = UINT64_MAX;
        pThisBucket->mHaveReceivedFragment[lThisFragmentNo] = true;
        pThisBucket->mTimeout = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() + (mBucketTimeoutms * 1000);
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType3Frame->hOfFragmentNo;
        pThisBucket->mFragmentSize = lType3Frame->hType1PacketSize;
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
        size_t lReserveThis = ((pThisBucket->mFragmentSize * (lType3Frame->hOfFragmentNo - 1)) +
                               (lPacketSize - sizeof(ElasticFrameType3)));
        pThisBucket->mBucketData = std::make_unique<SuperFrame>(lReserveThis);

        if (pThisBucket->mBucketData->pFrameData == nullptr) {
            mBucketMap.erase(pThisBucket->mDeliveryOrder);
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        std::copy_n(pSubPacket + sizeof(ElasticFrameType3), lPacketSize - sizeof(ElasticFrameType3),
                    pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
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
    if (pThisBucket->mHaveReceivedFragment[lThisFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedFragment[lThisFragmentNo] = true;
    }

    // Increment the fragment counter
    pThisBucket->mFragmentCounter++;

    pThisBucket->mBucketData->mFrameSize =
            (pThisBucket->mFragmentSize * (lType3Frame->hOfFragmentNo - 1)) +
            (lPacketSize - sizeof(ElasticFrameType3));

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // lInsertDataPointer will point to the fragment start above and fill with the incoming data

    size_t lInsertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
    std::copy_n(pSubPacket + sizeof(ElasticFrameType3), lPacketSize - sizeof(ElasticFrameType3),
                pThisBucket->mBucketData->pFrameData + lInsertDataPointer);
    return ElasticFrameMessages::noError;
}

//mNetMtx is already taken no need to lock anything
void ElasticFrameProtocolReceiver::runToCompletionMethod(
        const std::function<void(pFramePtr &rPacket, ElasticFrameProtocolContext *pCTX)> &rReceiveFunction) {
    int64_t lTimeNow = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    std::vector<uint64_t> lDeleteList;

    for (const auto &rBucket: mBucketMap) {
        if (mHeadOfLineBlockingTimeoutms) {
            //HOL mode
            if (mDeliveryHOLFirstRun) {
                //It's the first run. We are in HOL mode (Run to completion)
                //We can't wait for two frames since we don't know when
                //we will be here again and we can't time out single frames since
                //we are event driven externally. Set the HEAD speculatively and go with that.

                mDeliveryHOLFirstRun = false;
                mNextExpectedFrameNumber = mBucketMap.begin()->second->mDeliveryOrder;
            }
            if (rBucket.second->mDeliveryOrder == mNextExpectedFrameNumber &&
                rBucket.second->mFragmentCounter == rBucket.second->mOfFragmentNo) {
                //We got what we expected. Now deliver.
                //Assemble all data for delivery
                rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                rBucket.second->mBucketData->mBroken =
                        rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                if (rReceiveFunction) {
                    rReceiveFunction(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                } else {
                    receiveCallback(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                }
                lDeleteList.emplace_back(rBucket.first);
                mNextExpectedFrameNumber++; //The next expected frame is this frame number + 1
            } else if (rBucket.second->mTimeout + (mHeadOfLineBlockingTimeoutms * 1000) <= lTimeNow) {
                //We got HOL but the next frame has timed out meaning the time out of the bucket + the HOL timeout
                //We need now need to jump ahead and reset the mNextExpectedFrameNumber
                //Assemble all data for delivery and reset the HOL pointer.

                rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                rBucket.second->mBucketData->mBroken =
                        rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                if (rReceiveFunction) {
                    rReceiveFunction(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                } else {
                    receiveCallback(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                }
                lDeleteList.emplace_back(rBucket.first);
                mNextExpectedFrameNumber = rBucket.second->mDeliveryOrder + 1;
            } else {
                //We're blocked (HOL). No frames are complete or timed out
                //Look again at the delivery of the next fragment to see the status then.
                break;
            }
        } else {
            // We are not in HOL mode..
            // This means just deliver as the frames arrive or times out
            if (rBucket.second->mTimeout <= lTimeNow ||
                rBucket.second->mFragmentCounter == rBucket.second->mOfFragmentNo) {

                //Assemble all data for delivery
                rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                rBucket.second->mBucketData->mBroken =
                        rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                if (rReceiveFunction) {
                    rReceiveFunction(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                } else {
                    receiveCallback(rBucket.second->mBucketData, mCTX ? mCTX.get() : nullptr);
                }
                lDeleteList.emplace_back(rBucket.first);
            }
        }
    }

    for (const auto bucketID: lDeleteList) {
        auto lDeleteMe = mBucketMap[bucketID];
        mBucketMap.erase(bucketID);
        lDeleteMe->mActive = false;
        lDeleteMe->mBucketData = nullptr;
    }
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

            // pop until queue is empty
            if (!mSuperFrameQueue.empty()) {
                lSuperframe = std::move(mSuperFrameQueue.front());
                mSuperFrameQueue.pop_front();
            }
            // If there is more to pop don't close the semaphore else do.
            if (mSuperFrameQueue.empty()) {
                mSuperFrameReady = false;
            }
        }
        //I want to be outside the scope of the lock when calling the callback. Else the
        //callback may lock the internal workers.
        if (lSuperframe) {
            receiveCallback(lSuperframe, mCTX ? mCTX.get() : nullptr);
            lSuperframe = nullptr; //Drop the ownership.
        }
    }
    mIsDeliveryThreadActive = false;
}

// This is the thread going through the buckets to see if they should be delivered to
// the 'user'
void ElasticFrameProtocolReceiver::receiverWorker() {
    int64_t lTimeReference = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

//    uint32_t lTimedebuggerPointer = 0;
//    int64_t lTimeDebugger[100];

    while (mThreadActive) {
        lTimeReference += WORKER_THREAD_SLEEP_US;
        int64_t lTimeSample = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        int64_t lTimeCompensation = lTimeReference - lTimeSample;

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
            lTimeReference = lTimeSample;
            lTimeCompensation = 0;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(lTimeCompensation));
        }

        mNetMtx.lock();
        auto lActiveCount = (uint32_t) mBucketMap.size();
        if (!lActiveCount) {
            mNetMtx.unlock();
            continue; //Nothing to process
        }

        int64_t lTimeNow = lTimeSample + lTimeCompensation;

        std::vector<uint64_t> lDeleteList;

        for (const auto &rBucket: mBucketMap) {
            if (mHeadOfLineBlockingTimeoutms) {
                //HOL mode

                if (mDeliveryHOLFirstRun) {
                    //It's the first run. We are in HOL mode
                    //We need at least two super frames to set the HEAD correct.
                    //However if a fragment has timed out we need to act on this and start the delivery

                    bool lHasTimedOut = false;
                    for (const auto &rBucketInner: mBucketMap) {
                        //Has the bucket Timed out or are all fragments collected?
                        if (rBucketInner.second->mTimeout <= lTimeNow) {
                            //Set the flag signaling at least one frame has timed out
                            //std::cout << "Time out " << std::endl;
                            lHasTimedOut = true;
                        }
                    }

                    if (mBucketMap.size() > 1 || lHasTimedOut) {
                        mDeliveryHOLFirstRun = false;
                        mNextExpectedFrameNumber = mBucketMap.begin()->second->mDeliveryOrder;
                    } else {
                        break; //Nothing to process
                    }
                }

                if (rBucket.second->mDeliveryOrder == mNextExpectedFrameNumber &&
                    rBucket.second->mFragmentCounter == rBucket.second->mOfFragmentNo) {
                    //We got what we expected. Now deliver.
                    //Assemble all data for delivery
                    {
                        std::lock_guard<std::mutex> lk(mSuperFrameMtx);
                        rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                        rBucket.second->mBucketData->mBroken =
                                rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                        rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                        rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                        rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                        rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                        rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                        rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                        rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                        mSuperFrameQueue.push_back(std::move(rBucket.second->mBucketData));
                        mSuperFrameReady = true;
                    }
                    mSuperFrameDeliveryConditionVariable.notify_one();
                    lDeleteList.emplace_back(rBucket.first);
                    mNextExpectedFrameNumber++; //The next expected frame is this frame number + 1
                } else if (rBucket.second->mTimeout + (mHeadOfLineBlockingTimeoutms * 1000) <= lTimeNow) {
                    //We got HOL but the next frame has timed out meaning the time out of the bucket + the HOL timeout
                    //We need now need to jump ahead and reset the mNextExpectedFrameNumber
                    //Assemble all data for delivery and reset the HOL pointer.
                    {
                        std::lock_guard<std::mutex> lk(mSuperFrameMtx);
                        rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                        rBucket.second->mBucketData->mBroken =
                                rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                        rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                        rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                        rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                        rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                        rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                        rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                        rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                        mSuperFrameQueue.push_back(std::move(rBucket.second->mBucketData));
                        mSuperFrameReady = true;
                    }
                    mSuperFrameDeliveryConditionVariable.notify_one();
                    lDeleteList.emplace_back(rBucket.first);
                    mNextExpectedFrameNumber = rBucket.second->mDeliveryOrder + 1;
                } else {
                    //We're blocked (HOL). No frames are complete or timed out
                    //Look again at the delivery of the next fragment to see the status then.
                    break;
                }
            } else {
                // We are not in HOL mode..
                // This means just deliver as the frames arrive or times out
                if (rBucket.second->mTimeout <= lTimeNow ||
                    rBucket.second->mFragmentCounter == rBucket.second->mOfFragmentNo) {
                    //Assemble all data for delivery
                    {
                        std::lock_guard<std::mutex> lk(mSuperFrameMtx);
                        rBucket.second->mBucketData->mDataContent = rBucket.second->mDataContent;
                        rBucket.second->mBucketData->mBroken =
                                rBucket.second->mFragmentCounter != rBucket.second->mOfFragmentNo;
                        rBucket.second->mBucketData->mPts = rBucket.second->mPts;
                        rBucket.second->mBucketData->mDts = rBucket.second->mDts;
                        rBucket.second->mBucketData->mCode = rBucket.second->mCode;
                        rBucket.second->mBucketData->mStreamID = rBucket.second->mStream;
                        rBucket.second->mBucketData->mSource = rBucket.second->mSource;
                        rBucket.second->mBucketData->mFlags = rBucket.second->mFlags;
                        rBucket.second->mBucketData->mSuperFrameNo = rBucket.second->mSavedSuperFrameNo;
                        mSuperFrameQueue.push_back(std::move(rBucket.second->mBucketData));
                        mSuperFrameReady = true;
                    }
                    mSuperFrameDeliveryConditionVariable.notify_one();
                    lDeleteList.emplace_back(rBucket.first);
                }
            }
        }

        for (const auto bucketID: lDeleteList) {
            auto lDeleteMe = mBucketMap[bucketID];
            mBucketMap.erase(bucketID);
            lDeleteMe->mActive = false;
            lDeleteMe->mBucketData = nullptr;
        }

        // ------------------------------------------

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
ElasticFrameProtocolReceiver::receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t lFromSource,
                                              const std::function<void(pFramePtr &rPacket,
                                                                       ElasticFrameProtocolContext *pCTX)> &rReceiveFunction) {
    return receiveFragmentFromPtr(rSubPacket.data(), rSubPacket.size(), lFromSource, rReceiveFunction);
}

// Unpack method. We received a fragment of data or a full frame. Lets unpack it
ElasticFrameMessages
ElasticFrameProtocolReceiver::receiveFragmentFromPtr(const uint8_t *pSubPacket, size_t lPacketSize, uint8_t lFromSource,
                                                     const std::function<void(pFramePtr &rPacket,
                                                                              ElasticFrameProtocolContext *pCTX)> &rReceiveFunction) {
    // Type 0 packet. Discard and continue
    // Type 0 packets can be used to fill with user data outside efp protocol packets just put a uint8_t = Frametype::type0 at position 0 and then any data.
    // Type 1 are frames larger than MTU
    // Type 2 are frames smaller than MTU
    // Type 2 packets are also used at the end of Type 1 packet superFrames
    // Type 3 frames carry the reminder of data when it's too large for type2 to carry.

    ElasticFrameMessages lMessage;

    std::lock_guard<std::mutex> lock(mReceiveMtx);

    if (!(mIsWorkerThreadActive & mIsDeliveryThreadActive) && mCurrentMode == EFPReceiverMode::THREADED) {
        EFP_LOGGER(true, LOGG_ERROR, "Receiver not running")
        return ElasticFrameMessages::receiverNotRunning;
    }

    if ((pSubPacket[0] & (uint8_t) 0x0f) == Frametype::type0) {
        return ElasticFrameMessages::type0Frame;
    } else if ((pSubPacket[0] & (uint8_t) 0x0f) == Frametype::type1) {
        if (lPacketSize < sizeof(ElasticFrameType1)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        lMessage = unpackType1(pSubPacket, lPacketSize, lFromSource);
        if (mCurrentMode == EFPReceiverMode::RUN_TO_COMPLETION) {
            runToCompletionMethod(rReceiveFunction);
        }
        return lMessage;
    } else if ((pSubPacket[0] & (uint8_t) 0x0f) == Frametype::type2) {
        if (lPacketSize < sizeof(ElasticFrameType2)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        lMessage = unpackType2(pSubPacket, lPacketSize, lFromSource);
        if (mCurrentMode == EFPReceiverMode::RUN_TO_COMPLETION) {
            runToCompletionMethod(rReceiveFunction);
        }
        return lMessage;
    } else if ((pSubPacket[0] & (uint8_t) 0x0f) == Frametype::type3) {
        if (lPacketSize < sizeof(ElasticFrameType3)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        lMessage = unpackType3(pSubPacket, lPacketSize, lFromSource);
        if (mCurrentMode == EFPReceiverMode::RUN_TO_COMPLETION) {
            runToCompletionMethod(rReceiveFunction);
        }
        return lMessage;
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
        if (lEmbeddedHeader.mEmbeddedFrameType == ElasticEmbeddedFrameContent::illegal) {
            return ElasticFrameMessages::illegalEmbeddedData;
        }
        pDataContent->emplace_back((lEmbeddedHeader.mEmbeddedFrameType & (uint8_t) 0x7f));
        std::vector<uint8_t> lEmbeddedData(lEmbeddedHeader.mSize);
        std::copy_n(rPacket->pFrameData + lHeaderSize + *pPayloadDataPosition, lEmbeddedHeader.mSize,
                    lEmbeddedData.data());
        pEmbeddedDataList->emplace_back(lEmbeddedData);
        lMoreData = lEmbeddedHeader.mEmbeddedFrameType & (uint8_t) 0x80;
        *pPayloadDataPosition += (lEmbeddedHeader.mSize + lHeaderSize);
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
ElasticFrameProtocolSender::ElasticFrameProtocolSender(uint16_t lSetMTU,
                                                       std::shared_ptr<ElasticFrameProtocolContext> pCTX) {
    mCTX = std::move(pCTX);
    c_sendCallback = nullptr;
    mSendBufferEnd.reserve(lSetMTU);
    mSendBufferFixed.resize(lSetMTU);

    if (lSetMTU < UINT8_MAX) {
        EFP_LOGGER(true, LOGG_ERROR, "MTU lower than " << unsigned(UINT8_MAX) << " is not accepted.")
        mCurrentMTU = UINT8_MAX;
    } else {
        mCurrentMTU = lSetMTU;
    }

    sendCallback = std::bind(&ElasticFrameProtocolSender::sendData, this, std::placeholders::_1, std::placeholders::_2,
                             std::placeholders::_3);
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocolSender constructed")
}

ElasticFrameProtocolSender::~ElasticFrameProtocolSender() {
    EFP_LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocolSender destruct")
}

// Dummy callback for transmitter
void ElasticFrameProtocolSender::sendData(const std::vector<uint8_t> &rSubPacket, uint8_t lStreamID,
                                          ElasticFrameProtocolContext *pCTX) {
    if (c_sendCallback) {
        c_sendCallback(rSubPacket.data(), rSubPacket.size(), lStreamID, mCTX->mUnsafePointer);
    } else {
        EFP_LOGGER(true, LOGG_ERROR, "Implement the sendCallback method for the protocol to work.")
    }
}

// Pack data method. Fragments the data and calls the sendCallback method at the host level.
ElasticFrameMessages
ElasticFrameProtocolSender::packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent lDataContent,
                                        uint64_t lPts,
                                        uint64_t lDts,
                                        uint32_t lCode, uint8_t lStreamID, uint8_t lFlags,
                                        const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                                                 uint8_t streamID)> &rSendFunction) {
    return packAndSendFromPtr(rPacket.data(), rPacket.size(), lDataContent, lPts, lDts, lCode, lStreamID, lFlags,
                              rSendFunction);
}

// Pack data method. Fragments the data and calls the sendCallback method at the host level.
ElasticFrameMessages
ElasticFrameProtocolSender::packAndSendFromPtr(const uint8_t *pPacket, size_t lPacketSize,
                                               ElasticFrameContent lDataContent,
                                               uint64_t lPts, uint64_t lDts,
                                               uint32_t lCode, uint8_t lStreamID, uint8_t lFlags,
                                               const std::function<void(const std::vector<uint8_t> &rSubPacket,
                                                                        uint8_t streamID)> &rSendFunction) {
    std::lock_guard<std::mutex> lock(mSendMtx);

    if (sizeof(ElasticFrameType1) != sizeof(ElasticFrameType3)) {
        return ElasticFrameMessages::type1And3SizeError;
    }

    if (lPts == UINT64_MAX) {
        return ElasticFrameMessages::reservedPTSValue;
    }

    if (lDts == UINT64_MAX) {
        return ElasticFrameMessages::reservedDTSValue;
    }

    if (lCode == UINT32_MAX) {
        return ElasticFrameMessages::reservedCodeValue;
    }

    if (lStreamID == 0 && lDataContent != ElasticFrameContent::efpsig) {
        return ElasticFrameMessages::reservedStreamValue;
    }

    uint64_t lPtsDtsDiff = lPts - lDts;
    if (lPtsDtsDiff >= UINT32_MAX) {
        return ElasticFrameMessages::dtsptsDiffToLarge;
    }

    lFlags &= (uint8_t) 0xf0;

    // Will the data fit?
    // We know that we can send USHRT_MAX (65535) packets
    // The last packet will be a type2 packet.. so check against current MTU multiplied with USHRT_MAX subtracting the space the protocol needs for the headers
    if (lPacketSize
        > (((mCurrentMTU - sizeof(ElasticFrameType1)) * (USHRT_MAX - 1)) + (mCurrentMTU - sizeof(ElasticFrameType2)))) {
        return ElasticFrameMessages::tooLargeFrame;
    }

    if ((lPacketSize + sizeof(ElasticFrameType2)) <= mCurrentMTU) {
        mSendBufferEnd.resize(sizeof(ElasticFrameType2) + lPacketSize);
        auto *pType2Frame = (ElasticFrameType2 *) mSendBufferEnd.data();
        pType2Frame->hFrameType = Frametype::type2 | lFlags;
        pType2Frame->hStreamID = lStreamID;
        pType2Frame->hDataContent = lDataContent;
        pType2Frame->hSizeOfData = (uint16_t) lPacketSize;
        pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType2Frame->hOfFragmentNo = 0;
        pType2Frame->hType1PacketSize = (uint16_t) lPacketSize;
        pType2Frame->hPts = lPts;
        pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
        pType2Frame->hCode = lCode;
        std::copy_n(pPacket, lPacketSize, mSendBufferEnd.data() + sizeof(ElasticFrameType2));
        if (rSendFunction) {
            rSendFunction(mSendBufferEnd, lStreamID);
        } else {
            sendCallback(mSendBufferEnd, lStreamID, mCTX ? mCTX.get() : nullptr);
        }
        mSuperFrameNoGenerator++;
        return ElasticFrameMessages::noError;
    }

    uint16_t lFragmentNo = 0;

    // The size is known for type1 packets no need to write it in any header.
    size_t lDataPayloadType1 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    size_t lDataPayloadType2 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType2));

    uint64_t lDataPointer = 0;
    auto lOfFragmentNo = (uint16_t) floor(
            (double) (lPacketSize) / (double) (mCurrentMTU - sizeof(ElasticFrameType1)));
    uint16_t lOfFragmentNoType1 = lOfFragmentNo;
    bool lType3needed = false;
    size_t lReminderData = lPacketSize - (lOfFragmentNo * lDataPayloadType1);
    if (lReminderData > lDataPayloadType2) {
        // We need a type3 frame. The reminder is too large for a type2 frame
        lType3needed = true;
        lOfFragmentNo++;
    }

    auto *pType1Frame = (ElasticFrameType1 *) mSendBufferFixed.data();
    pType1Frame->hFrameType = Frametype::type1 | lFlags;
    pType1Frame->hStream = lStreamID;
    pType1Frame->hSuperFrameNo = mSuperFrameNoGenerator;
    pType1Frame->hOfFragmentNo = lOfFragmentNo;

    while (lFragmentNo < lOfFragmentNoType1) {
        pType1Frame->hFragmentNo = lFragmentNo++;
        std::copy_n(pPacket + lDataPointer, lDataPayloadType1, mSendBufferFixed.data() + sizeof(ElasticFrameType1));
        lDataPointer += lDataPayloadType1;
        if (rSendFunction) {
            rSendFunction(mSendBufferFixed, lStreamID);
        } else {
            sendCallback(mSendBufferFixed, lStreamID, mCTX ? mCTX.get() : nullptr);
        }
    }

    if (lType3needed) {
        lFragmentNo++;
        mSendBufferEnd.resize(sizeof(ElasticFrameType3) + lReminderData);
        auto *pType3Frame = (ElasticFrameType3 *) mSendBufferEnd.data();
        pType3Frame->hFrameType = Frametype::type3 | lFlags;
        pType3Frame->hStreamID = lStreamID;
        pType3Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType3Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
        pType3Frame->hOfFragmentNo = lOfFragmentNo;
        std::copy_n(pPacket + lDataPointer, lReminderData, mSendBufferEnd.data() + sizeof(ElasticFrameType3));
        lDataPointer += lReminderData;
        if (lDataPointer != lPacketSize) {
            return ElasticFrameMessages::internalCalculationError;
        }

        if (rSendFunction) {
            rSendFunction(mSendBufferEnd, lStreamID);
        } else {
            sendCallback(mSendBufferEnd, lStreamID, mCTX ? mCTX.get() : nullptr);
        }
    }

    // Create the last type2 packet
    size_t lDataLeftToSend = lPacketSize - lDataPointer;

    //Debug me for calculation errors
    if (lType3needed && lDataLeftToSend != 0) {
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lDataLeftToSend + sizeof(ElasticFrameType2) > mCurrentMTU) {
        EFP_LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << unsigned(lPacketSize))
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lOfFragmentNo != lFragmentNo) {
        return ElasticFrameMessages::internalCalculationError;
    }

    mSendBufferEnd.resize(sizeof(ElasticFrameType2) + lDataLeftToSend);
    auto *pType2Frame = (ElasticFrameType2 *) mSendBufferEnd.data();
    pType2Frame->hFrameType = Frametype::type2 | lFlags;
    pType2Frame->hStreamID = lStreamID;
    pType2Frame->hDataContent = lDataContent;
    pType2Frame->hSizeOfData = (uint16_t) lDataLeftToSend;
    pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
    pType2Frame->hOfFragmentNo = lOfFragmentNo;
    pType2Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    pType2Frame->hPts = lPts;
    pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
    pType2Frame->hCode = lCode;
    std::copy_n(pPacket + lDataPointer, lDataLeftToSend, mSendBufferEnd.data() + sizeof(ElasticFrameType2));
    if (rSendFunction) {
        rSendFunction(mSendBufferEnd, lStreamID);
    } else {
        sendCallback(mSendBufferEnd, lStreamID, mCTX ? mCTX.get() : nullptr);
    }
    mSuperFrameNoGenerator++;
    return ElasticFrameMessages::noError;
}

ElasticFrameMessages
ElasticFrameProtocolSender::destructivePackAndSendFromPtr(uint8_t *pPacket, size_t lPacketSize,
                                                          ElasticFrameContent lDataContent, uint64_t lPts,
                                                          uint64_t lDts, uint32_t lCode, uint8_t lStreamID,
                                                          uint8_t lFlags, const std::function<void(const uint8_t *,
                                                                                                   size_t)> &rSendFunction) {

    std::lock_guard<std::mutex> lock(mSendMtx);

    static_assert(sizeof(ElasticFrameType1) == sizeof(ElasticFrameType3));

    if (lPts == UINT64_MAX) {
        return ElasticFrameMessages::reservedPTSValue;
    }

    if (lDts == UINT64_MAX) {
        return ElasticFrameMessages::reservedDTSValue;
    }

    if (lCode == UINT32_MAX) {
        return ElasticFrameMessages::reservedCodeValue;
    }

    if (lStreamID == 0 && lDataContent != ElasticFrameContent::efpsig) {
        return ElasticFrameMessages::reservedStreamValue;
    }

    uint64_t lPtsDtsDiff = lPts - lDts;
    if (lPtsDtsDiff >= UINT32_MAX) {
        return ElasticFrameMessages::dtsptsDiffToLarge;
    }

    lFlags &= (uint8_t) 0xf0;

    // Will the data fit?
    // We know that we can send USHRT_MAX (65535) packets
    // The last packet will be a type2 packet.. so check against current MTU multiplied with USHRT_MAX subtracting the space the protocol needs for the headers
    if (lPacketSize
        > (((mCurrentMTU - sizeof(ElasticFrameType1)) * (USHRT_MAX - 1)) + (mCurrentMTU - sizeof(ElasticFrameType2)))) {
        return ElasticFrameMessages::tooLargeFrame;
    }

    if ((lPacketSize + sizeof(ElasticFrameType2)) <= mCurrentMTU) {
        auto pType2Frame = reinterpret_cast<ElasticFrameType2 *>(pPacket - sizeof(ElasticFrameType2));
        pType2Frame->hFrameType = Frametype::type2 | lFlags;
        pType2Frame->hStreamID = lStreamID;
        pType2Frame->hDataContent = lDataContent;
        pType2Frame->hSizeOfData = (uint16_t) lPacketSize;
        pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType2Frame->hOfFragmentNo = 0;
        pType2Frame->hType1PacketSize = (uint16_t) lPacketSize;
        pType2Frame->hPts = lPts;
        pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
        pType2Frame->hCode = lCode;
        rSendFunction((const uint8_t *) pType2Frame, (size_t) (lPacketSize + sizeof(ElasticFrameType2)));
        mSuperFrameNoGenerator++;
        return ElasticFrameMessages::noError;
    }

    uint16_t lFragmentNo = 0;

    // The size is known for type1 packets no need to write it in any header.
    size_t lDataPayloadType1 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    size_t lDataPayloadType2 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType2));

    uint64_t lDataPointer = 0;
    auto lOfFragmentNo = (uint16_t) floor(
            (double) (lPacketSize) / (double) (mCurrentMTU - sizeof(ElasticFrameType1)));
    uint16_t lOfFragmentNoType1 = lOfFragmentNo;
    bool lType3needed = false;
    size_t lReminderData = lPacketSize - (lOfFragmentNo * lDataPayloadType1);
    if (lReminderData > lDataPayloadType2) {
        // We need a type3 frame. The reminder is too large for a type2 frame
        lType3needed = true;
        lOfFragmentNo++;
    }

    while (lFragmentNo < lOfFragmentNoType1) {
        auto pType1Frame = reinterpret_cast<ElasticFrameType1 *>(pPacket - sizeof(ElasticFrameType1) + lDataPointer);
        pType1Frame->hFrameType = Frametype::type1 | lFlags;
        pType1Frame->hStream = lStreamID;
        pType1Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType1Frame->hFragmentNo = lFragmentNo++;
        pType1Frame->hOfFragmentNo = lOfFragmentNo;
        lDataPointer += lDataPayloadType1;
        rSendFunction((const uint8_t *) pType1Frame, (size_t) mCurrentMTU);
    }

    if (lType3needed) {
        lFragmentNo++;
        auto pType3Frame = reinterpret_cast<ElasticFrameType3 *>(pPacket - sizeof(ElasticFrameType3) + lDataPointer);
        pType3Frame->hFrameType = Frametype::type3 | lFlags;
        pType3Frame->hStreamID = lStreamID;
        pType3Frame->hSuperFrameNo = mSuperFrameNoGenerator;
        pType3Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
        pType3Frame->hOfFragmentNo = lOfFragmentNo;
        lDataPointer += lReminderData;
        if (lDataPointer != lPacketSize) {
            return ElasticFrameMessages::internalCalculationError;
        }
        rSendFunction((const uint8_t *) pType3Frame, (size_t) (lReminderData + sizeof(ElasticFrameType3)));
    }

    // Create the last type2 packet
    size_t lDataLeftToSend = lPacketSize - lDataPointer;

    //Debug me for calculation errors
    if (lType3needed && lDataLeftToSend != 0) {
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lDataLeftToSend + sizeof(ElasticFrameType2) > mCurrentMTU) {
        EFP_LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << unsigned(lPacketSize))
        return ElasticFrameMessages::internalCalculationError;
    }
    //Debug me for calculation errors
    if (lOfFragmentNo != lFragmentNo) {
        return ElasticFrameMessages::internalCalculationError;
    }

    auto pType2Frame = reinterpret_cast<ElasticFrameType2 *>(pPacket - sizeof(ElasticFrameType2) + lDataPointer);
    pType2Frame->hFrameType = Frametype::type2 | lFlags;
    pType2Frame->hStreamID = lStreamID;
    pType2Frame->hDataContent = lDataContent;
    pType2Frame->hSizeOfData = (uint16_t) lDataLeftToSend;
    pType2Frame->hSuperFrameNo = mSuperFrameNoGenerator;
    pType2Frame->hOfFragmentNo = lOfFragmentNo;
    pType2Frame->hType1PacketSize = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    pType2Frame->hPts = lPts;
    pType2Frame->hDtsPtsDiff = (uint32_t) lPtsDtsDiff;
    pType2Frame->hCode = lCode;
    rSendFunction((const uint8_t *) pType2Frame, (size_t) (lDataLeftToSend + sizeof(ElasticFrameType2)));
    mSuperFrameNoGenerator++;
    return ElasticFrameMessages::noError;
}

// Helper methods for embedding/extracting data in the payload part. It's not recommended to use these methods in production code as it's better to build the
// frames externally to avoid insert and copy of data.
ElasticFrameMessages ElasticFrameProtocolSender::addEmbeddedData(std::vector<uint8_t> *pPacket,
                                                                 void *pPrivateData,
                                                                 size_t lPrivateDataSize,
                                                                 ElasticEmbeddedFrameContent lContent,
                                                                 bool lIsLast) {
    if (lPrivateDataSize > UINT16_MAX) {
        return ElasticFrameMessages::tooLargeEmbeddedData;
    }
    ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader;
    lEmbeddedHeader.mSize = (uint16_t) lPrivateDataSize;
    lEmbeddedHeader.mEmbeddedFrameType = lContent;
    if (lIsLast)
        lEmbeddedHeader.mEmbeddedFrameType |= ElasticEmbeddedFrameContent::lastembeddedcontent;
    pPacket->insert(pPacket->begin(), (uint8_t *) pPrivateData, (uint8_t *) pPrivateData + lPrivateDataSize);
    pPacket->insert(pPacket->begin(), (uint8_t *) &lEmbeddedHeader,
                    (uint8_t *) &lEmbeddedHeader + sizeof(lEmbeddedHeader));
    return ElasticFrameMessages::noError;
}

// Used by the unit tests
size_t ElasticFrameProtocolSender::getType1Size() {
    return sizeof(ElasticFrameType1);
}

size_t ElasticFrameProtocolSender::getType2Size() {
    return sizeof(ElasticFrameType2);
}

void ElasticFrameProtocolSender::setSuperFrameNo(uint16_t lSuperFrameNo) {
    mSuperFrameNoGenerator = lSuperFrameNo;
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

uint64_t efp_init_send(uint64_t mtu, void (*f)(const uint8_t *, size_t, uint8_t, void *), void *ctx) {
    std::lock_guard<std::mutex> lock(efp_send_mutex);
    auto sender_ctx = std::make_shared<ElasticFrameProtocolContext>();
    sender_ctx->mUnsafePointer = ctx;
    uint64_t local_c_object_handle = c_object_handle;
    auto result = efp_send_base_map.insert(std::make_pair(local_c_object_handle,
                                                          std::make_shared<ElasticFrameProtocolSender>(
                                                                  static_cast<uint16_t>(mtu),
                                                                  sender_ctx)));
    if (!result.first->second) {
        return 0;
    }
    result.first->second->c_sendCallback = f;
    c_object_handle++;
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
                                    uint8_t,
                                    void *),
                          void (*g)(uint8_t *,
                                    size_t,
                                    uint8_t,
                                    uint64_t,
                                    void *),
                          void *ctx,
                          uint32_t mode
) {
    std::lock_guard<std::mutex> lock(efp_receive_mutex);
    uint64_t local_c_object_handle = c_object_handle;

    ElasticFrameProtocolReceiver::EFPReceiverMode receive_mode;

    auto receiver_ctx = std::make_shared<ElasticFrameProtocolContext>();
    receiver_ctx->mUnsafePointer = ctx;

    if (mode == EFP_MODE_RUN_TO_COMPLETE) {
        receive_mode = ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION;
    } else {
        receive_mode = ElasticFrameProtocolReceiver::EFPReceiverMode::THREADED;
    }

    auto result = efp_receive_base_map.insert(
            std::make_pair(local_c_object_handle,
                           std::make_shared<ElasticFrameProtocolReceiver>(bucketTimeout, holTimeout,
                                                                          receiver_ctx, receive_mode)));
    if (!result.first->second) {
        return 0;
    }
    result.first->second->c_receiveCallback = f;
    result.first->second->c_receiveEmbeddedDataCallback = g;
    c_object_handle++;
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
    std::lock_guard<std::mutex> lock(efp_send_mutex);
    auto efp_base = efp_send_base_map.find(efp_object)->second;
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
size_t efp_add_embedded_data(uint8_t *pDst, uint8_t *pESrc, uint8_t *pDSrc, size_t embeddedDatasize, size_t dataSize,
                             uint8_t type, uint8_t isLast) {
    if (pDst == nullptr) {
        return (sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader) + embeddedDatasize + dataSize);
    }

    ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader;
    lEmbeddedHeader.mSize = (uint16_t) embeddedDatasize;
    if (isLast) {
        type |= ElasticEmbeddedFrameContent::lastembeddedcontent;
    }
    lEmbeddedHeader.mEmbeddedFrameType = type;

    //Copy the header
    std::copy_n((uint8_t *) &lEmbeddedHeader, sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader), pDst);
    //Copy the embedded data
    std::copy_n(pESrc, embeddedDatasize, pDst + sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader));
    //Copy the data payload
    std::copy_n(pDSrc, dataSize, pDst + sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader) + embeddedDatasize);
    return 0;
}

int16_t efp_receive_fragment(uint64_t efp_object,
                             const uint8_t *pSubPacket,
                             size_t packetSize,
                             uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(efp_receive_mutex);
    auto efp_base = efp_receive_base_map.find(efp_object)->second;
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    return (int16_t) efp_base->receiveFragmentFromPtr(pSubPacket, packetSize, fromSource);
}

int16_t efp_end_send(uint64_t efp_object) {
    std::lock_guard<std::mutex> lock(efp_send_mutex);
    auto efp_base = efp_send_base_map.find(efp_object)->second;
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    auto num_deleted = efp_send_base_map.erase(efp_object);
    if (num_deleted) {
        return (int16_t) ElasticFrameMessages::noError;
    }
    return (int16_t) ElasticFrameMessages::efpCAPIfailure;
}

int16_t efp_end_receive(uint64_t efp_object) {
    std::lock_guard<std::mutex> lock(efp_receive_mutex);
    auto efp_base = efp_receive_base_map.find(efp_object)->second;
    if (efp_base == nullptr) {
        return (int16_t) ElasticFrameMessages::efpCAPIfailure;
    }
    auto num_deleted = efp_receive_base_map.erase(efp_object);
    if (num_deleted) {
        return (int16_t) ElasticFrameMessages::noError;
    }
    return (int16_t) ElasticFrameMessages::efpCAPIfailure;
}

uint16_t efp_get_version() {
    return (uint16_t) ((uint16_t) EFP_MAJOR_VERSION << (uint16_t) 8) | (uint16_t) EFP_MINOR_VERSION;
}




