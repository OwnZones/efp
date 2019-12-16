//
// Created by UnitX Edgeware AB 2019
//

#include "ElasticFrameProtocol.h"
#include "ElasticInternal.h"

// Constructor setting the MTU (Only needed if sending, mode == packer)
// Limit the MTU to uint16_t MAX and 255 min //The upper limit is hard
// The lower limit is actually type2frameSize+1, keep it at 255 for now
ElasticFrameProtocol::ElasticFrameProtocol(uint16_t setMTU, ElasticFrameMode mode) {
    if ((setMTU < UINT8_MAX) && mode != ElasticFrameMode::receiver) {
        LOGGER(true, LOGG_ERROR, "MTU lower than " << unsigned(UINT8_MAX) << " is not accepted.");
        mCurrentMTU = UINT8_MAX;
    } else {
        mCurrentMTU = setMTU;
    }
    mCurrentMode = mode;
    mThreadActive = false;
    mIsThreadActive = false;
    sendCallback = std::bind(&ElasticFrameProtocol::sendData, this, std::placeholders::_1);
    receiveCallback = std::bind(&ElasticFrameProtocol::gotData,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5,
                                std::placeholders::_6,
                                std::placeholders::_7);
    LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol constructed");
}

ElasticFrameProtocol::~ElasticFrameProtocol() {
    // If my worker is active we need to stop it.
    if (mThreadActive) {
        stopReceiver();
    }
    LOGGER(true, LOGG_NOTIFY, "ElasticFrameProtocol destruct");
}

// Dummy callback for transmitter
void ElasticFrameProtocol::sendData(const std::vector<uint8_t> &rSubPacket) {
    LOGGER(true, LOGG_ERROR, "Implement the sendCallback method for the protocol to work.");
}

// Dummy callback for reciever
void ElasticFrameProtocol::gotData(ElasticFrameProtocol::pFramePtr &rPacket, ElasticFrameContent content, bool broken,
                                    uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    LOGGER(true, LOGG_ERROR, "Implement the recieveCallback method for the protocol to work.");
}

// This method is generating a linear uint64_t counter from the nonlinear uint16_t couter
// The maximum loss / hole this calculator can handle is ((about) INT16_MAX / 2)
uint64_t ElasticFrameProtocol::superFrameRecalculator(uint16_t superFrame) {
    if (mSuperFrameFirstTime) {
        mOldSuperFrameNumber = superFrame;
        mSuperFrameRecalc = superFrame;
        mSuperFrameFirstTime = false;
        return mSuperFrameRecalc;
    }

    int16_t lChangeValue = (int16_t) superFrame - (int16_t) mOldSuperFrameNumber;
    int64_t lCval = (int64_t) lChangeValue;
    mOldSuperFrameNumber = superFrame;

    if (lCval > INT16_MAX) {
        lCval -= (UINT16_MAX - 1);
        mSuperFrameRecalc = mSuperFrameRecalc - lCval;
    } else {
        mSuperFrameRecalc = mSuperFrameRecalc + lCval;
    }
    return mSuperFrameRecalc;
}

// Unpack method for type1 packets. Type1 packets are the parts of frames larger than the MTU
ElasticFrameMessages ElasticFrameProtocol::unpackType1(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    ElasticFrameType1 lType1Frame = *(ElasticFrameType1 *) rSubPacket.data();
    Bucket *pThisBucket = &mBucketList[lType1Frame.hSuperFrameNo & CIRCULAR_BUFFER_SIZE];
    //LOGGER(false, LOGG_NOTIFY, "superFrameNo1-> " << unsigned(type1Frame.superFrameNo))

    // Is this entry in the buffer active? If no, create a new else continue filling the bucket with data.
    if (!pThisBucket->mActive) {
        //LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType1Frame.hSuperFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }
        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        pThisBucket->mActive = true;

        pThisBucket->mFlags = lType1Frame.hFrameType & 0xf0;

        pThisBucket->mStream = lType1Frame.hStream;
        Stream *pThisStream = &mStreams[fromSource][lType1Frame.hStream];
        pThisBucket->mDataContent = pThisStream->dataContent;
        pThisBucket->mCode = pThisStream->code;

        pThisBucket->mSavedSuperFrameNo = lType1Frame.hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mHaveReceivedPacket[lType1Frame.hFragmentNo] = 1;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType1Frame.hOfFragmentNo;
        pThisBucket->mFragmentSize = (rSubPacket.size() - sizeof(ElasticFrameType1));
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lType1Frame.hFragmentNo;
        pThisBucket->mBucketData = std::make_shared<AllignedFrameData>(
                pThisBucket->mFragmentSize * (lType1Frame.hOfFragmentNo + 1));
        pThisBucket->mBucketData->frameSize = pThisBucket->mFragmentSize * lType1Frame.hOfFragmentNo;

        if (pThisBucket->mBucketData->frameData == nullptr) {
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }

        std::copy(rSubPacket.begin() + sizeof(ElasticFrameType1),
                  rSubPacket.end(),
                  pThisBucket->mBucketData->frameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    // There is a gap in receiving the packets. Increase the bucket size list.. if the
    // bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (lType1Frame.hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    // I'm getting a packet with data larger than the expected size
    // this can be generated by wraparound in the bucket bucketList
    // The notification about more than 50% buffer full level should already
    // be triggered by now.
    // I invalidate this bucket to save me but the user should be notified somehow about this state. FIXME

    if (pThisBucket->mOfFragmentNo < lType1Frame.hFragmentNo || lType1Frame.hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    // Have I already recieved this packet before? (duplicate?)
    if (pThisBucket->mHaveReceivedPacket[lType1Frame.hFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lType1Frame.hFragmentNo] = 1;
    }

    // Let's re-set the timout and let also add +1 to the fragment counter
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mFragmentCounter++;

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // insertDataPointer will point to the fragment start above and fill with the incomming data

    size_t insertDataPointer = pThisBucket->mFragmentSize * lType1Frame.hFragmentNo;

    std::copy(rSubPacket.begin() + sizeof(ElasticFrameType1),
              rSubPacket.end(),
              pThisBucket->mBucketData->frameData + insertDataPointer);

    return ElasticFrameMessages::noError;
}

// Unpack method for type2 packets. Where we know there is also type 1 packets involved and possibly type3.
// Type2 packets are also parts of frames smaller than the MTU
// The data IS the last data of a sequence
// See the comments from above.

ElasticFrameMessages ElasticFrameProtocol::unpackType2LastFrame(const std::vector<uint8_t> &rSubPacket,
                                                                  uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    ElasticFrameType2 lType2Frame = *(ElasticFrameType2 *) rSubPacket.data();
    Bucket *pThisBucket = &mBucketList[lType2Frame.hSuperFrameNo & CIRCULAR_BUFFER_SIZE];

    if (!pThisBucket->mActive) {
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType2Frame.hSuperFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }
        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        pThisBucket->mActive = true;

        pThisBucket->mFlags = lType2Frame.hFrameType & 0xf0;

        pThisBucket->mStream = lType2Frame.hStream;
        Stream *pThisStream = &mStreams[fromSource][lType2Frame.hStream];
        pThisStream->dataContent = lType2Frame.hDataContent;
        pThisStream->code = lType2Frame.hCode;
        pThisBucket->mDataContent = pThisStream->dataContent;
        pThisBucket->mCode = pThisStream->code;

        pThisBucket->mSavedSuperFrameNo = lType2Frame.hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = lType2Frame.hPts;
        pThisBucket->mHaveReceivedPacket[lType2Frame.hFragmentNo] = 1;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mOfFragmentNo = lType2Frame.hOfFragmentNo;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mFragmentSize = lType2Frame.hType1PacketSize;
        size_t lReserveThis = ((pThisBucket->mFragmentSize * lType2Frame.hOfFragmentNo) +
                              (lType2Frame.hSizeOfData));
        pThisBucket->mBucketData = std::make_shared<AllignedFrameData>(lReserveThis);
        if (pThisBucket->mBucketData->frameData == nullptr) {
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }
        size_t lInsertDataPointer = lType2Frame.hType1PacketSize * lType2Frame.hFragmentNo;

        std::copy(rSubPacket.begin() + sizeof(ElasticFrameType2),
                  rSubPacket.end(),
                  pThisBucket->mBucketData->frameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    if (lType2Frame.hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    if (pThisBucket->mOfFragmentNo < lType2Frame.hFragmentNo || lType2Frame.hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    if (pThisBucket->mHaveReceivedPacket[lType2Frame.hFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lType2Frame.hFragmentNo] = 1;
    }

    // Type 2 frames contains the pts and code. If for some reason the type2 packet is missing or the frame is delivered
    // Before the type2 frame arrives PTS and CODE are set to it's respective 'illegal' value. meaning you cant't use them.
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mPts = lType2Frame.hPts;
    pThisBucket->mCode = lType2Frame.hCode;
    pThisBucket->mFlags = lType2Frame.hFrameType & 0xf0;
    pThisBucket->mFragmentCounter++;

    //set the content type
    pThisBucket->mStream = lType2Frame.hStream;
    Stream *thisStream = &mStreams[fromSource][lType2Frame.hStream];
    thisStream->dataContent = lType2Frame.hDataContent;
    thisStream->code = lType2Frame.hCode;
    pThisBucket->mDataContent = thisStream->dataContent;
    pThisBucket->mCode = thisStream->code;

    // When the type2 frames are recieved only then is the actual size to be delivered known... Now set the real size for the bucketData
    if (lType2Frame.hSizeOfData) {
        pThisBucket->mBucketData->frameSize =
                (pThisBucket->mFragmentSize * lType2Frame.hOfFragmentNo) + (rSubPacket.size() - sizeof(ElasticFrameType2));
        // Type 2 is always at the end and is always the highest number fragment
        size_t lInsertDataPointer = lType2Frame.hType1PacketSize * lType2Frame.hFragmentNo;

        std::copy(rSubPacket.begin() + sizeof(ElasticFrameType2),
                  rSubPacket.end(),
                  pThisBucket->mBucketData->frameData + lInsertDataPointer);
    }

    return ElasticFrameMessages::noError;
}

// Unpack method for type3 packets. Type3 packets are the parts of frames where the reminder data does not fit a type2 packet. Then a type 3 is added
// in front of a type2 packet to catch the data overshoot.
// Type 3 frames MUST be the same header size as type1 headers
ElasticFrameMessages ElasticFrameProtocol::unpackType3(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    ElasticFrameType3 lType3Frame = *(ElasticFrameType3 *) rSubPacket.data();
    Bucket *pThisBucket = &mBucketList[lType3Frame.hSuperFrameNo & CIRCULAR_BUFFER_SIZE];

    // If there is a type3 frame it's the second last frame
    uint16_t lThisFragmentNo = lType3Frame.hOfFragmentNo - 1;

    // Is this entry in the buffer active? If no, create a new else continue filling the bucket with data.
    if (!pThisBucket->mActive) {
        //LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t lDeliveryOrderCandidate = superFrameRecalculator(lType3Frame.hSuperFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (lDeliveryOrderCandidate == pThisBucket->mDeliveryOrder) {
            return ElasticFrameMessages::tooOldFragment;
        }
        pThisBucket->mDeliveryOrder = lDeliveryOrderCandidate;
        pThisBucket->mActive = true;

        pThisBucket->mFlags = lType3Frame.hFrameType & 0xf0;

        pThisBucket->mStream = lType3Frame.hStream;
        Stream *thisStream = &mStreams[fromSource][lType3Frame.hStream];
        pThisBucket->mDataContent = thisStream->dataContent;
        pThisBucket->mCode = thisStream->code;

        pThisBucket->mSavedSuperFrameNo = lType3Frame.hSuperFrameNo;
        pThisBucket->mHaveReceivedPacket.reset();
        pThisBucket->mPts = UINT64_MAX;
        pThisBucket->mHaveReceivedPacket[lThisFragmentNo] = 1;
        pThisBucket->mTimeout = mBucketTimeout;
        pThisBucket->mFragmentCounter = 0;
        pThisBucket->mOfFragmentNo = lType3Frame.hOfFragmentNo;
        pThisBucket->mFragmentSize = lType3Frame.hType1PacketSize;
        size_t lInsertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
        size_t lReserveThis = ((pThisBucket->mFragmentSize * (lType3Frame.hOfFragmentNo - 1)) +
                              (rSubPacket.size() - sizeof(ElasticFrameType3)));
        pThisBucket->mBucketData = std::make_shared<AllignedFrameData>(lReserveThis);

        if (pThisBucket->mBucketData->frameData == nullptr) {
            pThisBucket->mActive = false;
            return ElasticFrameMessages::memoryAllocationError;
        }

        std::copy(rSubPacket.begin() + sizeof(ElasticFrameType3),
                  rSubPacket.end(),
                  pThisBucket->mBucketData->frameData + lInsertDataPointer);
        return ElasticFrameMessages::noError;
    }

    // There is a gap in recieving the packets. Increase the bucket size list.. if the
    // bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (lType3Frame.hSuperFrameNo != pThisBucket->mSavedSuperFrameNo) {
        return ElasticFrameMessages::bufferOutOfResources;
    }

    // I'm getting a packet with data larger than the expected size
    // this can be generated by wraparound in the bucket bucketList
    // The notification about more than 50% buffer full level should already
    // be triggered by now.
    // I invalidate this bucket to save me but the user should be notified somehow about this state. FIXME

    if (pThisBucket->mOfFragmentNo < lThisFragmentNo || lType3Frame.hOfFragmentNo != pThisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        pThisBucket->mActive = false;
        return ElasticFrameMessages::bufferOutOfBounds;
    }

    // Have I already recieved this packet before? (duplicate?)
    if (pThisBucket->mHaveReceivedPacket[lThisFragmentNo] == 1) {
        return ElasticFrameMessages::duplicatePacketReceived;
    } else {
        pThisBucket->mHaveReceivedPacket[lThisFragmentNo] = 1;
    }

    // Let's re-set the timout and let also add +1 to the fragment counter
    pThisBucket->mTimeout = mBucketTimeout;
    pThisBucket->mFragmentCounter++;

    pThisBucket->mBucketData->frameSize =
            (pThisBucket->mFragmentSize * (lType3Frame.hOfFragmentNo - 1)) +
            (rSubPacket.size() - sizeof(ElasticFrameType3));

    // Move the data to the correct fragment position in the frame.
    // A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // insertDataPointer will point to the fragment start above and fill with the incomming data

    size_t insertDataPointer = pThisBucket->mFragmentSize * lThisFragmentNo;
    std::copy(rSubPacket.begin() + sizeof(ElasticFrameType3),
              rSubPacket.end(),
              pThisBucket->mBucketData->frameData + insertDataPointer);
    return ElasticFrameMessages::noError;
}

// This is the thread going trough the buckets to see if they should be delivered to
// the 'user'
void ElasticFrameProtocol::receiverWorker(uint32_t timeout){
    //Set the defaults. meaning the thread is running and there is no head of line blocking action going on.
    bool lFoundHeadOfLineBlocking = false;
    bool lFistDelivery = mHeadOfLineBlockingTimeout ? false
                                                  : true; //if hol is used then we must recieve at least two packets first to know where to start counting.
    uint32_t lHeadOfLineBlockingCounter = 0;
    uint64_t lHeadOfLineBlockingTail = 0;
    uint64_t lExpectedNextFrameToDeliver = 0;
    uint64_t lOldestFrameDelivered = 0;
    uint64_t lSavedPTS = 0;

    while (mThreadActive) {
        usleep(1000 * 10); // Check all active buckets 100 times a second
        bool lTimeOutTrigger = false;
        uint32_t lActiveCount = 0;
        std::vector<CandidateToDeliver> lCandidates;
        uint64_t lDeliveryOrderOldest = UINT64_MAX;

        // The default mode is not to clear any buckets
        bool lClearHeadOfLineBuckets = false;
        // If I'm in head of blocking garbage collect mode.
        if (lFoundHeadOfLineBlocking) {
            // If some one instructed me to timeout then let's timeout first
            if (lHeadOfLineBlockingCounter) {
                lHeadOfLineBlockingCounter--;
                // LOGGER(true, LOGG_NOTIFY, "Flush head countdown " << unsigned(headOfLineBlockingCounter))
            } else {
                // LOGGER(true, LOGG_NOTIFY, "Flush trigger " << unsigned(headOfLineBlockingCounter))
                // Timeout triggered.. Let's garbage collect the head.
                lClearHeadOfLineBuckets = true;
                lFoundHeadOfLineBlocking = false;
            }
        }
        mNetMtx.lock();

        // Scan trough all buckets

        for (uint64_t i = 0; i < CIRCULAR_BUFFER_SIZE + 1; i++) {

            // Only work with the buckets that are active
            if (mBucketList[i].mActive) {
                // Keep track of number of active buckets
                lActiveCount++;

                // Save the number of the oldest bucket in queue to be delivered
                if (lDeliveryOrderOldest > mBucketList[i].mDeliveryOrder) {
                    lDeliveryOrderOldest = mBucketList[i].mDeliveryOrder;
                }
                // Are we cleaning out old buckets and did we found a head to timout?
                if ((mBucketList[i].mDeliveryOrder < lHeadOfLineBlockingTail) && lClearHeadOfLineBuckets) {
                    //LOGGER(true, LOGG_NOTIFY, "BOOM clear-> " << unsigned(bucketList[i].deliveryOrder))
                    mBucketList[i].mTimeout = 1;
                }

                mBucketList[i].mTimeout--;

                // If the bucket is ready to be delivered or is the bucket timedout?
                if (!mBucketList[i].mTimeout) {
                    lTimeOutTrigger = true;
                    lCandidates.emplace_back(CandidateToDeliver(mBucketList[i].mDeliveryOrder, i));
                    mBucketList[i].mTimeout = 1; //We want to timeout this again if head of line blocking is on
                } else if (mBucketList[i].mFragmentCounter == mBucketList[i].mOfFragmentNo) {
                    lCandidates.emplace_back(CandidateToDeliver(mBucketList[i].mDeliveryOrder, i));
                }
            }
        }

        size_t lNumCandidatesToDeliver = lCandidates.size();
        if ((!lFistDelivery && lNumCandidatesToDeliver >= 2) || lTimeOutTrigger) {
            lFistDelivery = true;
            lExpectedNextFrameToDeliver = lDeliveryOrderOldest;
        }

        // Do we got any timedout buckets or finished buckets?
        if (lNumCandidatesToDeliver && lFistDelivery) {
            //Sort them in delivery order
            std::sort(lCandidates.begin(), lCandidates.end(), sortDeliveryOrder());

            //FIXME - we could implement fast HOL clearing here

            //Fast HOL candidate
            //We're not clearing cuckets and we have found HOL
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
                //LOGGER(true, LOGG_NOTIFY, "FLUSH HEAD!")

                uint64_t lAndTheNextIs = lCandidates[0].deliveryOrder;

                for (auto &x: lCandidates) {
                    if (lOldestFrameDelivered <= x.deliveryOrder) {

                        // Here we introduce a new concept..
                        // If we are cleaning out the HOL. Only go soo far to either a gap (counter) or packet "non time out".
                        // If you remove the 'if' below HOL will clean out all superframes from the top of the buffer to the bottom of the buffer no matter the
                        // Status of the packets in between. So HOL cleaning just wipes out all waiting. This might be a wanted behaviour to avoid time-stall
                        // However packets in queue are lost since they will 'falsely' be seen as coming late and then discarded.

                        // FIXME
                        // If for example candidates.size() is larger than a certain size then maybe just flush to the end to avoid a blocking HOL situation
                        // If for example every second packet is lost then we will build a large queue

                        if (lAndTheNextIs != x.deliveryOrder) {
                            // We did not expect this. is the bucket timed out .. then continue...
                            if (mBucketList[x.bucket].mTimeout > 1) {
                                break;
                            }
                        }
                        lAndTheNextIs = x.deliveryOrder + 1;

                        lOldestFrameDelivered = mHeadOfLineBlockingTimeout ? x.deliveryOrder : 0;
                        receiveCallback(mBucketList[x.bucket].mBucketData,
                                        mBucketList[x.bucket].mDataContent,
                                        mBucketList[x.bucket].mFragmentCounter != mBucketList[x.bucket].mOfFragmentNo,
                                        mBucketList[x.bucket].mPts,
                                        mBucketList[x.bucket].mCode,
                                        mBucketList[x.bucket].mStream,
                                        mBucketList[x.bucket].mFlags);

                    }
                    lExpectedNextFrameToDeliver = x.deliveryOrder + 1;
                    // std::cout << " (y) " << unsigned(expectedNextFrameToDeliver) << std::endl;
                    lSavedPTS = mBucketList[x.bucket].mPts;
                    mBucketList[x.bucket].mActive = false;
                    mBucketList[x.bucket].mBucketData = nullptr;
                }
            } else {

                // In this run we have not cleared the head.. is there a head to clear?
                // We can't be in waitning for timout and we can't have a 0 time-out
                // A 0 timout means out of order delivery else we-re here.
                // So in out of order delivery we time out the buckets instead of flushing the head.

                // Check for head of line blocking only if HOL-timoeut is set
                if (lExpectedNextFrameToDeliver < mBucketList[lCandidates[0].bucket].mDeliveryOrder &&
                    mHeadOfLineBlockingTimeout &&
                    !lFoundHeadOfLineBlocking) {

                    //for (auto &x: candidates) { //DEBUG-Keep for now
                    //    std::cout << ">>>" << unsigned(x.deliveryOrder) << " is broken " << x.broken << std::endl;
                    //}

                    lFoundHeadOfLineBlocking = true; //Found hole
                    lHeadOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                    lHeadOfLineBlockingTail = mBucketList[lCandidates[0].bucket].mDeliveryOrder; //This is the tail
                    //LOGGER(true, LOGG_NOTIFY, "HOL " << unsigned(expectedNextFrameToDeliver) << " "
                    //<< unsigned(bucketList[candidates[0].bucket].deliveryOrder)
                    //<< " tail " << unsigned(headOfLineBlockingTail)
                    //<< " savedPTS " << unsigned(savedPTS))
                }

                //Deliver only when head of line blocking is cleared and we're back to normal
                if (!lFoundHeadOfLineBlocking) {
                    for (auto &x: lCandidates) {

                        if (lExpectedNextFrameToDeliver != x.deliveryOrder && mHeadOfLineBlockingTimeout) {
                            lFoundHeadOfLineBlocking = true; //Found hole
                            lHeadOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                            lHeadOfLineBlockingTail =
                                    x.deliveryOrder; //So we basically give the non existing data a chance to arrive..
                            //LOGGER(true, LOGG_NOTIFY, "HOL2 " << unsigned(expectedNextFrameToDeliver) << " " << unsigned(x.deliveryOrder) << " tail " << unsigned(headOfLineBlockingTail))
                            break;
                        }
                        lExpectedNextFrameToDeliver = x.deliveryOrder + 1;

                        //std::cout << unsigned(oldestFrameDelivered) << " " << unsigned(x.deliveryOrder) << std::endl;
                        if (lOldestFrameDelivered <= x.deliveryOrder) {
                            lOldestFrameDelivered = mHeadOfLineBlockingTimeout ? x.deliveryOrder : 0;
                            receiveCallback(mBucketList[x.bucket].mBucketData,
                                            mBucketList[x.bucket].mDataContent,
                                            mBucketList[x.bucket].mFragmentCounter != mBucketList[x.bucket].mOfFragmentNo,
                                            mBucketList[x.bucket].mPts,
                                            mBucketList[x.bucket].mCode,
                                            mBucketList[x.bucket].mStream,
                                            mBucketList[x.bucket].mFlags);
                        }
                        lSavedPTS = mBucketList[x.bucket].mPts;
                        mBucketList[x.bucket].mActive = false;
                        mBucketList[x.bucket].mBucketData = nullptr;
                    }
                }
            }
        }
        mNetMtx.unlock();

        // Is more than 75% of the buffer used. //FIXME notify the user in some way
        if (lActiveCount > (CIRCULAR_BUFFER_SIZE / 4) * 3) {
            LOGGER(true, LOGG_WARN, "Current active buckets are more than half the circular buffer.");
        }

    }
    mIsThreadActive = false;
}

// Start reciever worker thread
ElasticFrameMessages ElasticFrameProtocol::startReceiver(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster){
    if (mCurrentMode != ElasticFrameMode::receiver) {
        return ElasticFrameMessages::wrongMode;
    }

    if (mIsThreadActive) {
        LOGGER(true, LOGG_WARN, "Unpacker already working");
        return ElasticFrameMessages::receiverAlreadyStarted;
    }
    if (bucketTimeoutMaster == 0) {
        LOGGER(true, LOGG_WARN, "bucketTimeoutMaster can't be 0");
        return ElasticFrameMessages::parameterError;
    }
    if (holTimeoutMaster >= bucketTimeoutMaster) {
        LOGGER(true, LOGG_WARN, "holTimeoutMaster can't be less or equal to bucketTimeoutMaster");
        return ElasticFrameMessages::parameterError;
    }

    mBucketTimeout = bucketTimeoutMaster;
    mHeadOfLineBlockingTimeout = holTimeoutMaster;
    mThreadActive =
            true; //you must set these parameters here to avoid races. For example calling start then stop before the thread actually starts.
    mIsThreadActive = true;
    std::thread(std::bind(&ElasticFrameProtocol::receiverWorker, this, bucketTimeoutMaster)).detach();
    return ElasticFrameMessages::noError;
}

// Stop reciever worker thread
ElasticFrameMessages ElasticFrameProtocol::stopReceiver(){
    std::lock_guard<std::mutex> lock(mReceiveMtx);

    if (mCurrentMode != ElasticFrameMode::receiver) {
        return ElasticFrameMessages::wrongMode;
    }

    //Set the semaphore to stop thread
    mThreadActive = false;
    uint32_t lLockProtect = 1000;
    //check for it to actually stop
    while (mIsThreadActive) {
        usleep(1000);
        if (!--lLockProtect) {
            //we gave it a second now exit anyway
            LOGGER(true, LOGG_FATAL, "unpackerWorker thread not stopping. Quitting anyway");
            return ElasticFrameMessages::failedStoppingReceiver;
        }
    }
    return ElasticFrameMessages::noError;
}

// Unpack method. We recieved a fragment of data or a full frame. Lets unpack it
ElasticFrameMessages ElasticFrameProtocol::receiveFragment(const std::vector<uint8_t> &rSubPacket, uint8_t fromSource) {
    // Type 0 packet. Discard and continue
    // Type 0 packets can be used to fill with user data outside efp protocol packets just put a uint8_t = Frametype::type0 at position 0 and then any data.
    // Type 1 are frames larger than MTU
    // Type 2 are frames smaller than MTU
    // Type 2 packets are also used at the end of Type 1 packet superFrames
    // Type 3 frames carry the reminder of data when it's too large for type2 to carry.

    std::lock_guard<std::mutex> lock(mReceiveMtx);

    if (mCurrentMode != ElasticFrameMode::receiver) {
        return ElasticFrameMessages::wrongMode;
    }

    if (!mIsThreadActive) {
        LOGGER(true, LOGG_ERROR, "Receiver not running");
        return ElasticFrameMessages::receiverNotRunning;
    }

    if ((rSubPacket[0] & 0x0f) == Frametype::type0) {
        return ElasticFrameMessages::type0Frame;
    } else if ((rSubPacket[0] & 0x0f) == Frametype::type1) {
        if (rSubPacket.size() < sizeof(ElasticFrameType1)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        return unpackType1(rSubPacket, fromSource);
    } else if ((rSubPacket[0] & 0x0f) == Frametype::type2) {
        if (rSubPacket.size() < sizeof(ElasticFrameType2)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        ElasticFrameType2 lType2Frame = *(ElasticFrameType2 *) rSubPacket.data();
        if (lType2Frame.hOfFragmentNo == lType2Frame.hFragmentNo) {
            return unpackType2LastFrame(rSubPacket, fromSource);
        } else {
            return ElasticFrameMessages::endOfPacketError;
        }
    } else if ((rSubPacket[0] & 0x0f) == Frametype::type3) {
        if (rSubPacket.size() < sizeof(ElasticFrameType3)) {
            return ElasticFrameMessages::frameSizeMismatch;
        }
        return unpackType3(rSubPacket, fromSource);
    }

    // Did not catch anything I understand
    return ElasticFrameMessages::unknownFrameType;
}

// Pack data method. Fragments the data and calls the sendCallback method at the host level.
ElasticFrameMessages
ElasticFrameProtocol::packAndSend(const std::vector<uint8_t> &rPacket, ElasticFrameContent dataContent, uint64_t pts,
                                   uint32_t code, uint8_t stream, uint8_t flags) {

    std::lock_guard<std::mutex> lock(mSendMtx);

    if (sizeof(ElasticFrameType1) != sizeof(ElasticFrameType3)) {
        return ElasticFrameMessages::type1And3SizeError;
    }

    if (mCurrentMode != ElasticFrameMode::sender) {
        return ElasticFrameMessages::wrongMode;
    }

    if (pts == UINT64_MAX) {
        return ElasticFrameMessages::reservedPTSValue;
    }

    if (code == UINT32_MAX) {
        return ElasticFrameMessages::reservedCodeValue;
    }

    if (stream == 0) {
        return ElasticFrameMessages::reservedStreamValue;
    }

    flags &= 0xf0;

    // Will the data fit?
    // we know that we can send USHRT_MAX (65535) packets
    // the last packet will be a type2 packet.. so the current MTU muliplied with USHRT_MAX subtracting the space the protocol needs for the headers
    if (rPacket.size()
        > (((mCurrentMTU - sizeof(ElasticFrameType1)) * (USHRT_MAX - 1)) + (mCurrentMTU - sizeof(ElasticFrameType2)))) {
        return ElasticFrameMessages::tooLargeFrame;
    }

    if ((rPacket.size() + sizeof(ElasticFrameType2)) <= mCurrentMTU) {
        ElasticFrameType2 lType2Frame;
        lType2Frame.hSuperFrameNo = mSuperFrameNoGenerator;
        lType2Frame.hFrameType |= flags;
        lType2Frame.hDataContent = dataContent;
        lType2Frame.hSizeOfData = (uint16_t) rPacket.size(); //The total size fits uint16_t since we cap the MTU to uint16_t
        lType2Frame.hPts = pts;
        lType2Frame.hCode = code;
        lType2Frame.hStream = stream;
        try {
            std::vector<uint8_t> lFinalPacket(sizeof(ElasticFrameType2) + rPacket.size());
            std::copy((uint8_t *) &lType2Frame, ((uint8_t *) &lType2Frame) + sizeof(ElasticFrameType2),
                      lFinalPacket.begin());
            std::copy(rPacket.begin(), rPacket.end(), lFinalPacket.begin() + sizeof(ElasticFrameType2));
            sendCallback(lFinalPacket);
        }
        catch (std::bad_alloc const &) {
            return ElasticFrameMessages::memoryAllocationError;
        }
        mSuperFrameNoGenerator++;
        return ElasticFrameMessages::noError;
    }

    uint16_t lFragmentNo = 0;
    ElasticFrameType1 lType1Frame;
    lType1Frame.hFrameType |= flags;
    lType1Frame.hStream = stream;
    lType1Frame.hSuperFrameNo = mSuperFrameNoGenerator;
    // The size is known for type1 packets no need to write it in any header.
    size_t lDataPayloadType1 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType1));
    size_t lDataPayloadType2 = (uint16_t) (mCurrentMTU - sizeof(ElasticFrameType2));

    uint64_t lDataPointer = 0;
    uint16_t lOfFragmentNo = floor((double) (rPacket.size()) / (double) (mCurrentMTU - sizeof(ElasticFrameType1)));
    uint16_t lOfFragmentNoType1 = lOfFragmentNo;
    bool lType3needed = false;
    size_t lReminderData = rPacket.size() - (lOfFragmentNo * lDataPayloadType1);
    if (lReminderData > lDataPayloadType2) {
        // We need a type3 frame. The reminder is too large for a type2 frame
        lType3needed = true;
        lOfFragmentNo++;
    }

    lType1Frame.hOfFragmentNo = lOfFragmentNo;
    std::vector<uint8_t> lFinalPacketLoop(sizeof(ElasticFrameType1) + lDataPayloadType1);
    while (lFragmentNo < lOfFragmentNoType1) {
        lType1Frame.hFragmentNo = lFragmentNo++;
        std::copy((uint8_t *) &lType1Frame, ((uint8_t *) &lType1Frame) + sizeof(ElasticFrameType1),
                  lFinalPacketLoop.begin());
        std::copy(rPacket.begin() + lDataPointer,
                  rPacket.begin() + lDataPointer + lDataPayloadType1,
                  lFinalPacketLoop.begin() + sizeof(ElasticFrameType1));
        lDataPointer += lDataPayloadType1;
        sendCallback(lFinalPacketLoop);
    }

    if (lType3needed) {
        lFragmentNo++;
        std::vector<uint8_t> lType3PacketData(sizeof(ElasticFrameType3) + lReminderData);
        ElasticFrameType3 lType3Frame;
        lType3Frame.hFrameType |= flags;
        lType3Frame.hStream = lType1Frame.hStream;
        lType3Frame.hOfFragmentNo = lType1Frame.hOfFragmentNo;
        lType3Frame.hType1PacketSize = mCurrentMTU - sizeof(ElasticFrameType1);
        lType3Frame.hSuperFrameNo = lType1Frame.hSuperFrameNo;
        std::copy((uint8_t *) &lType3Frame, ((uint8_t *) &lType3Frame) + sizeof(ElasticFrameType3),
                  lType3PacketData.begin());
        std::copy(rPacket.begin() + lDataPointer,
                  rPacket.begin() + lDataPointer + lReminderData,
                  lType3PacketData.begin() + sizeof(ElasticFrameType3));
        lDataPointer += lReminderData;
        if (lDataPointer != rPacket.size()) {
            return ElasticFrameMessages::internalCalculationError;
        }
        sendCallback(lType3PacketData);
    }

    // Create the last type2 packet
    size_t lDataLeftToSend = rPacket.size() - lDataPointer;

    if (lType3needed && lDataLeftToSend != 0) {
        return ElasticFrameMessages::internalCalculationError;
    }

    //Debug me for calculation errors
    if (lDataLeftToSend + sizeof(ElasticFrameType2) > mCurrentMTU) {
        LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << rPacket.size());
        return ElasticFrameMessages::internalCalculationError;
    }

    if (lOfFragmentNo != lFragmentNo) {
        return ElasticFrameMessages::internalCalculationError;
    }

    ElasticFrameType2 lType2Frame;
    lType2Frame.hFrameType |= flags;
    lType2Frame.hSuperFrameNo = mSuperFrameNoGenerator;
    lType2Frame.hFragmentNo = lFragmentNo;
    lType2Frame.hOfFragmentNo = lOfFragmentNo;
    lType2Frame.hDataContent = dataContent;
    lType2Frame.hSizeOfData = (uint16_t) lDataLeftToSend;
    lType2Frame.hPts = pts;
    lType2Frame.hCode = code;
    lType2Frame.hType1PacketSize = mCurrentMTU - sizeof(ElasticFrameType1);
    std::vector<uint8_t> lFinalPacket(sizeof(ElasticFrameType2) + lDataLeftToSend);
    std::copy((uint8_t *) &lType2Frame, ((uint8_t *) &lType2Frame) + sizeof(ElasticFrameType2), lFinalPacket.begin());
    if (lDataLeftToSend) {
        std::copy(rPacket.begin() + lDataPointer,
                  rPacket.begin() + lDataPointer + lDataLeftToSend,
                  lFinalPacket.begin() + sizeof(ElasticFrameType2));
    }
    sendCallback(lFinalPacket);
    mSuperFrameNoGenerator++;
    return ElasticFrameMessages::noError;
}

// Helper methods for embeding/extracting data in the payload part. It's not recommended to use these methods in production code as it's better to build the
// frames externally to avoid insert and copy of data.

ElasticFrameMessages ElasticFrameProtocol::addEmbeddedData(std::vector<uint8_t> *pPacket,
                                                             void *pPrivateData,
                                                             size_t privateDataSize,
                                                             ElasticEmbeddedFrameContent content,
                                                             bool isLast) {
    if (privateDataSize > UINT16_MAX) {
        return ElasticFrameMessages::tooLargeEmbeddedData;
    }

    ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader;
    lEmbeddedHeader.size = privateDataSize;
    lEmbeddedHeader.embeddedFrameType = content;
    if (isLast)
        lEmbeddedHeader.embeddedFrameType =
                lEmbeddedHeader.embeddedFrameType | ElasticEmbeddedFrameContent::lastEmbeddedContent;
    pPacket->insert(pPacket->begin(), (uint8_t *) pPrivateData, (uint8_t *) pPrivateData + privateDataSize);
    pPacket->insert(pPacket->begin(), (uint8_t *) &lEmbeddedHeader, (uint8_t *) &lEmbeddedHeader + sizeof(lEmbeddedHeader));
    return ElasticFrameMessages::noError;
}

ElasticFrameMessages ElasticFrameProtocol::extractEmbeddedData(ElasticFrameProtocol::pFramePtr &rPacket,
                                                                 std::vector<std::vector<uint8_t>> *pEmbeddedDataList,
                                                                 std::vector<uint8_t> *pDataContent,
                                                                 size_t *pPayloadDataPosition) {
    bool lMoreData = true;
    size_t headerSize = sizeof(ElasticFrameContentNamespace::ElasticEmbeddedHeader);
    do {
        ElasticFrameContentNamespace::ElasticEmbeddedHeader lEmbeddedHeader =
                *(ElasticFrameContentNamespace::ElasticEmbeddedHeader *) (rPacket->frameData + *pPayloadDataPosition);
        if (lEmbeddedHeader.embeddedFrameType == ElasticEmbeddedFrameContent::illegal) {
            return ElasticFrameMessages::illegalEmbeddedData;
        }
        pDataContent->emplace_back((lEmbeddedHeader.embeddedFrameType & 0x7f));
        std::vector<uint8_t> embeddedData(lEmbeddedHeader.size);
        std::copy(rPacket->frameData + headerSize + *pPayloadDataPosition,
                  rPacket->frameData + headerSize + *pPayloadDataPosition + lEmbeddedHeader.size,
                  embeddedData.begin());
        pEmbeddedDataList->emplace_back(embeddedData);
        lMoreData = lEmbeddedHeader.embeddedFrameType & 0x80;
        *pPayloadDataPosition += (lEmbeddedHeader.size + headerSize);
        if (*pPayloadDataPosition >= rPacket->frameSize) {
            return ElasticFrameMessages::bufferOutOfBounds;
        }
    } while (!lMoreData);
    return ElasticFrameMessages::noError;
}

// Used by the unit tests
size_t ElasticFrameProtocol::geType1Size() {
    return sizeof(ElasticFrameType1);
}

size_t ElasticFrameProtocol::geType2Size() {
    return sizeof(ElasticFrameType2);
}




