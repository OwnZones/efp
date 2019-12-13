//
// Created by Anders Cedronius on 2019-11-11.
//

#include "EdgewareFrameProtocol.h"
#include "EdgewareInternal.h"

//Constructor setting the MTU (Only needed if sending, mode == packer)
//Limit the MTU to uint16_t MAX and 255 min //The upper limit is hard
// the lower limit is actually type2frameSize+1, keep it at 255 for now
EdgewareFrameProtocol::EdgewareFrameProtocol(uint16_t setMTU, EdgewareFrameMode mode) {
    if ((setMTU < UINT8_MAX) && mode != EdgewareFrameMode::unpacker) {
        LOGGER(true, LOGG_ERROR, "MTU lower than " << unsigned(UINT8_MAX) << " is not accepted.");
        mCurrentMTU = UINT8_MAX;
    } else {
        mCurrentMTU = setMTU;
    }
    mCurrentMode = mode;
    mThreadActive = false;
    mIsThreadActive = false;
    sendCallback = std::bind(&EdgewareFrameProtocol::sendData, this, std::placeholders::_1);
    recieveCallback = std::bind(&EdgewareFrameProtocol::gotData,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3,
                                std::placeholders::_4,
                                std::placeholders::_5,
                                std::placeholders::_6,
                                std::placeholders::_7);
    LOGGER(true, LOGG_NOTIFY, "EdgewareFrameProtocol constructed");
}

EdgewareFrameProtocol::~EdgewareFrameProtocol() {
    //If my worker is active we need to stop it.
    if (mThreadActive) {
        stopUnpacker();
    }
    LOGGER(true, LOGG_NOTIFY, "EdgewareFrameProtocol destruct");
}

//Dummy callback for transmitter
void EdgewareFrameProtocol::sendData(const std::vector<uint8_t> &subPacket) {
    LOGGER(true, LOGG_ERROR, "Implement the sendCallback method for the protocol to work.");
}

//Dummy callback for reciever
void EdgewareFrameProtocol::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken,
                                    uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    LOGGER(true, LOGG_ERROR, "Implement the recieveCallback method for the protocol to work.");
}

//This method is generating a linear uint64_t counter from the nonlinear uint16_t
//counter. The maximum loss / hole this calculator can handle is (INT16_MAX)
uint64_t EdgewareFrameProtocol::superFrameRecalculator(uint16_t superFrame) {
    if (mSuperFrameFirstTime) {
        mOldSuperframeNumber = superFrame;
        mSuperFrameRecalc = superFrame;
        mSuperFrameFirstTime = false;
        return mSuperFrameRecalc;
    }

    int16_t changeValue = (int16_t) superFrame - (int16_t) mOldSuperframeNumber;
    int64_t cval = (int64_t) changeValue;
    mOldSuperframeNumber = superFrame;

    if (cval > INT16_MAX) {
        cval -= (UINT16_MAX - 1);
        mSuperFrameRecalc = mSuperFrameRecalc - cval;
    } else {
        mSuperFrameRecalc = mSuperFrameRecalc + cval;
    }
    return mSuperFrameRecalc;
}

//Unpack method for type1 packets. Type1 packets are the parts of frames larger than the MTU
EdgewareFrameMessages EdgewareFrameProtocol::unpackType1(const std::vector<uint8_t> &subPacket, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    EdgewareFrameType1 type1Frame = *(EdgewareFrameType1 *) subPacket.data();
    Bucket *thisBucket = &mBucketList[type1Frame.superFrameNo & CIRCULAR_BUFFER_SIZE];
    //LOGGER(false, LOGG_NOTIFY, "superFrameNo1-> " << unsigned(type1Frame.superFrameNo))

    //is this entry in the buffer active? If no, create a new else continue filling the bucket with data.
    if (!thisBucket->mActive) {
        //LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t deliveryOrderCandidate = superFrameRecalculator(type1Frame.superFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (deliveryOrderCandidate == thisBucket->mDeliveryOrder) {
            return EdgewareFrameMessages::tooOldFragment;
        }
        thisBucket->mDeliveryOrder = deliveryOrderCandidate;
        thisBucket->mActive = true;

        thisBucket->mFlags = type1Frame.frameType & 0xf0;

        thisBucket->mStream = type1Frame.stream;
        Stream *thisStream = &streams[fromSource][type1Frame.stream];
        thisBucket->mDataContent = thisStream->dataContent;
        thisBucket->mCode = thisStream->code;

        thisBucket->mSavedSuperFrameNo = type1Frame.superFrameNo;
        thisBucket->mHaveRecievedPacket.reset();
        thisBucket->mPts = UINT64_MAX;
        thisBucket->mHaveRecievedPacket[type1Frame.fragmentNo] = 1;
        thisBucket->mTimeout = mBucketTimeout;
        thisBucket->mFragmentCounter = 0;
        thisBucket->mOfFragmentNo = type1Frame.ofFragmentNo;
        thisBucket->mFragmentSize = (subPacket.size() - sizeof(EdgewareFrameType1));
        size_t insertDataPointer = thisBucket->mFragmentSize * type1Frame.fragmentNo;
        thisBucket->mBucketData = std::make_shared<AllignedFrameData>(
                thisBucket->mFragmentSize * (type1Frame.ofFragmentNo + 1));
        thisBucket->mBucketData->frameSize = thisBucket->mFragmentSize * type1Frame.ofFragmentNo;

        if (thisBucket->mBucketData->framedata == nullptr) {
            thisBucket->mActive = false;
            return EdgewareFrameMessages::memoryAllocationError;
        }

        std::copy(subPacket.begin() + sizeof(EdgewareFrameType1),
                  subPacket.end(),
                  thisBucket->mBucketData->framedata + insertDataPointer);

        //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer,
        //            subPacket.data() + sizeof(EdgewareFrameType1), subPacket.size() - sizeof(EdgewareFrameType1));
        return EdgewareFrameMessages::noError;
    }

    //there is a gap in recieving the packets. Increase the bucket size list.. if the
    //bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (type1Frame.superFrameNo != thisBucket->mSavedSuperFrameNo) {
        return EdgewareFrameMessages::bufferOutOfResources;
    }

    //I'm getting a packet with data larger than the expected size
    //this can be generated by wraparound in the bucket bucketList
    //The notification about more than 50% buffer full level should already
    //be triggered by now.
    //I invalidate this bucket to save me but the user should be notified somehow about this state. FIXME

    if (thisBucket->mOfFragmentNo < type1Frame.fragmentNo || type1Frame.ofFragmentNo != thisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        thisBucket->mActive = false;
        return EdgewareFrameMessages::bufferOutOfBounds;
    }

    //Have I already recieved this packet before? (duplicate?)
    if (thisBucket->mHaveRecievedPacket[type1Frame.fragmentNo] == 1) {
        return EdgewareFrameMessages::duplicatePacketRecieved;
    } else {
        thisBucket->mHaveRecievedPacket[type1Frame.fragmentNo] = 1;
    }

    //Let's re-set the timout and let also add +1 to the fragment counter
    thisBucket->mTimeout = mBucketTimeout;
    thisBucket->mFragmentCounter++;

    //move the data to the correct fragment position in the frame.
    //A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // insertDataPointer will point to the fragment start above and fill with the incomming data

    size_t insertDataPointer = thisBucket->mFragmentSize * type1Frame.fragmentNo;

    std::copy(subPacket.begin() + sizeof(EdgewareFrameType1),
              subPacket.end(),
              thisBucket->mBucketData->framedata + insertDataPointer);

    //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer, subPacket.data() + sizeof(EdgewareFrameType1),
    //            subPacket.size() - sizeof(EdgewareFrameType1));
    return EdgewareFrameMessages::noError;
}

// Unpack method for type2 packets. Where we know there is also type 1 packets involved and possibly type3.
// Type2 packets are also parts of frames smaller than the MTU
// The data IS the last data of a sequence
// See the comments from above.

EdgewareFrameMessages EdgewareFrameProtocol::unpackType2LastFrame(const std::vector<uint8_t> &subPacket,
                                                                  uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);
    EdgewareFrameType2 type2Frame = *(EdgewareFrameType2 *) subPacket.data();
    Bucket *thisBucket = &mBucketList[type2Frame.superFrameNo & CIRCULAR_BUFFER_SIZE];

    if (!thisBucket->mActive) {
        uint64_t deliveryOrderCandidate = superFrameRecalculator(type2Frame.superFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (deliveryOrderCandidate == thisBucket->mDeliveryOrder) {
            return EdgewareFrameMessages::tooOldFragment;
        }
        thisBucket->mDeliveryOrder = deliveryOrderCandidate;
        thisBucket->mActive = true;

        thisBucket->mFlags = type2Frame.frameType & 0xf0;

        thisBucket->mStream = type2Frame.stream;
        Stream *thisStream = &streams[fromSource][type2Frame.stream];
        thisStream->dataContent = type2Frame.dataContent;
        thisStream->code = type2Frame.code;
        thisBucket->mDataContent = thisStream->dataContent;
        thisBucket->mCode = thisStream->code;

        thisBucket->mSavedSuperFrameNo = type2Frame.superFrameNo;
        thisBucket->mHaveRecievedPacket.reset();
        thisBucket->mPts = type2Frame.pts;
        thisBucket->mHaveRecievedPacket[type2Frame.fragmentNo] = 1;
        thisBucket->mTimeout = mBucketTimeout;
        thisBucket->mOfFragmentNo = type2Frame.ofFragmentNo;
        thisBucket->mFragmentCounter = 0;
        thisBucket->mFragmentSize = type2Frame.type1PacketSize;
        size_t reserveThis = ((thisBucket->mFragmentSize * type2Frame.ofFragmentNo) +
                              (type2Frame.sizeOfData));
        thisBucket->mBucketData = std::make_shared<AllignedFrameData>(reserveThis);
        if (thisBucket->mBucketData->framedata == nullptr) {
            thisBucket->mActive = false;
            return EdgewareFrameMessages::memoryAllocationError;
        }
        size_t insertDataPointer = type2Frame.type1PacketSize * type2Frame.fragmentNo;

        std::copy(subPacket.begin() + sizeof(EdgewareFrameType2),
                  subPacket.end(),
                  thisBucket->mBucketData->framedata + insertDataPointer);

        //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer,
        //            subPacket.data() + sizeof(EdgewareFrameType2), subPacket.size() - sizeof(EdgewareFrameType2));
        return EdgewareFrameMessages::noError;
    }

    if (type2Frame.superFrameNo != thisBucket->mSavedSuperFrameNo) {
        return EdgewareFrameMessages::bufferOutOfResources;
    }

    if (thisBucket->mOfFragmentNo < type2Frame.fragmentNo || type2Frame.ofFragmentNo != thisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        thisBucket->mActive = false;
        return EdgewareFrameMessages::bufferOutOfBounds;
    }

    if (thisBucket->mHaveRecievedPacket[type2Frame.fragmentNo] == 1) {
        return EdgewareFrameMessages::duplicatePacketRecieved;
    } else {
        thisBucket->mHaveRecievedPacket[type2Frame.fragmentNo] = 1;
    }

    //Type 2 frames contains the pts and code. If for some reason the type2 packet is missing or the frame is delivered
    //Before the type2 frame arrives PTS and CODE are set to it's respective 'illegal' value. meaning you cant't use them.
    thisBucket->mTimeout = mBucketTimeout;
    thisBucket->mPts = type2Frame.pts;
    thisBucket->mCode = type2Frame.code;
    thisBucket->mFlags = type2Frame.frameType & 0xf0;
    thisBucket->mFragmentCounter++;

    //set the content type
    thisBucket->mStream = type2Frame.stream;
    Stream *thisStream = &streams[fromSource][type2Frame.stream];
    thisStream->dataContent = type2Frame.dataContent;
    thisStream->code = type2Frame.code;
    thisBucket->mDataContent = thisStream->dataContent;
    thisBucket->mCode = thisStream->code;


    //when the type2 frames are recieved only then is the actual size to be delivered known... Now set the real size for the bucketData
    if (type2Frame.sizeOfData) {
        thisBucket->mBucketData->frameSize =
                (thisBucket->mFragmentSize * type2Frame.ofFragmentNo) + (subPacket.size() - sizeof(EdgewareFrameType2));
        //Type 2 is always at the end and is always the highest number fragment
        size_t insertDataPointer = type2Frame.type1PacketSize * type2Frame.fragmentNo;

        std::copy(subPacket.begin() + sizeof(EdgewareFrameType2),
                  subPacket.end(),
                  thisBucket->mBucketData->framedata + insertDataPointer);
        //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer, subPacket.data() + sizeof(EdgewareFrameType2),
        //            subPacket.size() - sizeof(EdgewareFrameType2));
    }

    return EdgewareFrameMessages::noError;
}

//Unpack method for type3 packets. Type3 packets are the parts of frames where the reminder data does not fit a type2 packet. Then a type 3 is added
//in front of a type2 packet to catch the data overshoot.
//Type 3 frames MUST be the same header size as type1 headers
EdgewareFrameMessages EdgewareFrameProtocol::unpackType3(const std::vector<uint8_t> &subPacket, uint8_t fromSource) {
    std::lock_guard<std::mutex> lock(mNetMtx);

    EdgewareFrameType3 type3Frame = *(EdgewareFrameType3 *) subPacket.data();
    Bucket *thisBucket = &mBucketList[type3Frame.superFrameNo & CIRCULAR_BUFFER_SIZE];

    //If there is a type3 frame it's the second last frame
    uint16_t thisFragmentNo = type3Frame.ofFragmentNo - 1;

    //is this entry in the buffer active? If no, create a new else continue filling the bucket with data.
    if (!thisBucket->mActive) {
        //LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        uint64_t deliveryOrderCandidate = superFrameRecalculator(type3Frame.superFrameNo);
        //Is this a old fragment where we already delivered the superframe?
        if (deliveryOrderCandidate == thisBucket->mDeliveryOrder) {
            return EdgewareFrameMessages::tooOldFragment;
        }
        thisBucket->mDeliveryOrder = deliveryOrderCandidate;
        thisBucket->mActive = true;

        thisBucket->mFlags = type3Frame.frameType & 0xf0;

        thisBucket->mStream = type3Frame.stream;
        Stream *thisStream = &streams[fromSource][type3Frame.stream];
        thisBucket->mDataContent = thisStream->dataContent;
        thisBucket->mCode = thisStream->code;

        thisBucket->mSavedSuperFrameNo = type3Frame.superFrameNo;
        thisBucket->mHaveRecievedPacket.reset();
        thisBucket->mPts = UINT64_MAX;
        thisBucket->mHaveRecievedPacket[thisFragmentNo] = 1;
        thisBucket->mTimeout = mBucketTimeout;
        thisBucket->mFragmentCounter = 0;
        thisBucket->mOfFragmentNo = type3Frame.ofFragmentNo;
        thisBucket->mFragmentSize = type3Frame.type1PacketSize;
        size_t insertDataPointer = thisBucket->mFragmentSize * thisFragmentNo;
        size_t reserveThis = ((thisBucket->mFragmentSize * (type3Frame.ofFragmentNo - 1)) +
                              (subPacket.size() - sizeof(EdgewareFrameType3)));
        thisBucket->mBucketData = std::make_shared<AllignedFrameData>(reserveThis);

        if (thisBucket->mBucketData->framedata == nullptr) {
            thisBucket->mActive = false;
            return EdgewareFrameMessages::memoryAllocationError;
        }

        std::copy(subPacket.begin() + sizeof(EdgewareFrameType3),
                  subPacket.end(),
                  thisBucket->mBucketData->framedata + insertDataPointer);
        //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer,
        //            subPacket.data() + sizeof(EdgewareFrameType3), subPacket.size() - sizeof(EdgewareFrameType3));
        return EdgewareFrameMessages::noError;
    }

    //there is a gap in recieving the packets. Increase the bucket size list.. if the
    //bucket size list is == X*UINT16_MAX you will no longer detect any buffer errors
    if (type3Frame.superFrameNo != thisBucket->mSavedSuperFrameNo) {
        return EdgewareFrameMessages::bufferOutOfResources;
    }

    //I'm getting a packet with data larger than the expected size
    //this can be generated by wraparound in the bucket bucketList
    //The notification about more than 50% buffer full level should already
    //be triggered by now.
    //I invalidate this bucket to save me but the user should be notified somehow about this state. FIXME

    if (thisBucket->mOfFragmentNo < thisFragmentNo || type3Frame.ofFragmentNo != thisBucket->mOfFragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        thisBucket->mActive = false;
        return EdgewareFrameMessages::bufferOutOfBounds;
    }

    //Have I already recieved this packet before? (duplicate?)
    if (thisBucket->mHaveRecievedPacket[thisFragmentNo] == 1) {
        return EdgewareFrameMessages::duplicatePacketRecieved;
    } else {
        thisBucket->mHaveRecievedPacket[thisFragmentNo] = 1;
    }

    //Let's re-set the timout and let also add +1 to the fragment counter
    thisBucket->mTimeout = mBucketTimeout;
    thisBucket->mFragmentCounter++;

    thisBucket->mBucketData->frameSize =
            (thisBucket->mFragmentSize * (type3Frame.ofFragmentNo - 1)) +
            (subPacket.size() - sizeof(EdgewareFrameType3));

    //move the data to the correct fragment position in the frame.
    //A bucket contains the frame data -> This is the internal data format
    // |bucket start|information about the frame|bucket end| in the bucket there is a pointer to the actual data named framePtr this is the structure there ->
    // linear array of -> |fragment start|fragment data|fragment end|
    // insertDataPointer will point to the fragment start above and fill with the incomming data

    size_t insertDataPointer = thisBucket->mFragmentSize * thisFragmentNo;
    std::copy(subPacket.begin() + sizeof(EdgewareFrameType3),
              subPacket.end(),
              thisBucket->mBucketData->framedata + insertDataPointer);
    //std::memcpy(thisBucket->bucketData->framedata + insertDataPointer, subPacket.data() + sizeof(EdgewareFrameType3),
    //            subPacket.size() - sizeof(EdgewareFrameType3));
    return EdgewareFrameMessages::noError;
}

// This is the thread going trough the buckets to see if they should be delivered to
// the 'user'
// 1000 times per second is a bit aggressive, change to 100 times per second? FIXME, talk about what is realistic.. Settable? static? limits? why? for what reason?
void EdgewareFrameProtocol::unpackerWorker(uint32_t timeout) {
    //Set the defaults. meaning the thread is running and there is no head of line blocking action going on.
    bool foundHeadOfLineBlocking = false;
    bool fistDelivery = mHeadOfLineBlockingTimeout ? false
                                                  : true; //if hol is used then we must recieve at least two packets first to know where to start counting.
    uint32_t headOfLineBlockingCounter = 0;
    uint64_t headOfLineBlockingTail = 0;
    uint64_t expectedNextFrameToDeliver = 0;

    uint64_t oldestFrameDelivered = 0;

    uint64_t savedPTS = 0;

    while (mThreadActive) {
        usleep(1000 * 10); //Check all active buckets 100 times a second
        bool timeOutTrigger = false;
        uint32_t activeCount = 0;
        std::vector<CandidateToDeliver> candidates;
        uint64_t deliveryOrderOldest = UINT64_MAX;

        //The default mode is not to clear any buckets
        bool clearHeadOfLineBuckets = false;
        //If I'm in head of blocking garbage collect mode.
        if (foundHeadOfLineBlocking) {
            //If some one instructed me to timeout then let's timeout first
            if (headOfLineBlockingCounter) {
                headOfLineBlockingCounter--;
                //LOGGER(true, LOGG_NOTIFY, "Flush head countdown " << unsigned(headOfLineBlockingCounter))
            } else {
                //LOGGER(true, LOGG_NOTIFY, "Flush trigger " << unsigned(headOfLineBlockingCounter))
                //Timeout triggered.. Let's garbage collect the head.
                clearHeadOfLineBuckets = true;
                foundHeadOfLineBlocking = false;
            }
        }
        mNetMtx.lock();

        //Scan trough all buckets

        for (uint64_t i = 0; i < CIRCULAR_BUFFER_SIZE + 1; i++) {

            //Only work with the buckets that are active
            if (mBucketList[i].mActive) {
                //Keep track of number of active buckets
                activeCount++;

                //save the number of the oldest bucket in queue to be delivered
                if (deliveryOrderOldest > mBucketList[i].mDeliveryOrder) {
                    deliveryOrderOldest = mBucketList[i].mDeliveryOrder;
                }
                //Are we cleaning out old buckets and did we found a head to timout?
                if ((mBucketList[i].mDeliveryOrder < headOfLineBlockingTail) && clearHeadOfLineBuckets) {
                    //LOGGER(true, LOGG_NOTIFY, "BOOM clear-> " << unsigned(bucketList[i].deliveryOrder))
                    mBucketList[i].mTimeout = 1;
                }

                mBucketList[i].mTimeout--;

                //If the bucket is ready to be delivered or is the bucket timedout?
                if (!mBucketList[i].mTimeout) {
                    timeOutTrigger = true;
                    candidates.emplace_back(CandidateToDeliver(mBucketList[i].mDeliveryOrder, i));
                    mBucketList[i].mTimeout = 1; //We want to timeout this again if head of line blocking is on
                } else if (mBucketList[i].mFragmentCounter == mBucketList[i].mOfFragmentNo) {
                    candidates.emplace_back(CandidateToDeliver(mBucketList[i].mDeliveryOrder, i));
                }
            }
        }

        size_t numCandidatesToDeliver = candidates.size();

        if ((!fistDelivery && numCandidatesToDeliver >= 2) || timeOutTrigger) {
            fistDelivery = true;
            expectedNextFrameToDeliver = deliveryOrderOldest;
        }

        //Do we got any timedout buckets or finished buckets?
        if (numCandidatesToDeliver && fistDelivery) {
            //Sort them in delivery order
            std::sort(candidates.begin(), candidates.end(), sortDeliveryOrder());

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



            //So ok we have cleared the head send it all out
            if (clearHeadOfLineBuckets) {
                //LOGGER(true, LOGG_NOTIFY, "FLUSH HEAD!")

                uint64_t andTheNextIs = candidates[0].deliveryOrder;

                for (auto &x: candidates) {
                    if (oldestFrameDelivered <= x.deliveryOrder) {

                        //Here we introduce a new concept..
                        //If we are cleaning out the HOL. Only go soo far to either a gap (counter) or packet "non time out".
                        //If you remove the 'if' below HOL will clean out all superframes from the top of the buffer to the bottom of the buffer no matter the
                        //Status of the packets in between. So HOL cleaning just wipes out all waiting. This might be a wanted behaviour to avoid time-stall
                        //However packets in queue are lost since they will 'falsely' be seen as coming late and then discarded.

                        //FIXME
                        //if for example candidates.size() is larger than a certain size then maybe just flush to the end to avoid a long head

                        if (andTheNextIs != x.deliveryOrder) {
                            //we did not expect this. is the bucket timed out .. then continue...
                            if (mBucketList[x.bucket].mTimeout > 1) {
                                break;
                            }
                        }
                        andTheNextIs = x.deliveryOrder + 1;

                        oldestFrameDelivered = mHeadOfLineBlockingTimeout ? x.deliveryOrder : 0;
                        recieveCallback(mBucketList[x.bucket].mBucketData,
                                        mBucketList[x.bucket].mDataContent,
                                        mBucketList[x.bucket].mFragmentCounter != mBucketList[x.bucket].mOfFragmentNo,
                                        mBucketList[x.bucket].mPts,
                                        mBucketList[x.bucket].mCode,
                                        mBucketList[x.bucket].mStream,
                                        mBucketList[x.bucket].mFlags);

                    }
                    expectedNextFrameToDeliver = x.deliveryOrder + 1;
                    //std::cout << " (y) " << unsigned(expectedNextFrameToDeliver) << std::endl;
                    savedPTS = mBucketList[x.bucket].mPts;
                    mBucketList[x.bucket].mActive = false;
                    mBucketList[x.bucket].mBucketData = nullptr;
                }
            } else {

                //in this run we have not cleared the head.. is there a head to clear?
                //We can't be in waitning for timout and we can't have a 0 time-out
                //A 0 timout means out of order delivery else we-re here.
                //So in out of order delivery we time out the buckets instead of flushing the head.

                //Check for head of line blocking only if HOL-timoeut is set
                if (expectedNextFrameToDeliver < mBucketList[candidates[0].bucket].mDeliveryOrder &&
                    mHeadOfLineBlockingTimeout &&
                    !foundHeadOfLineBlocking) {

                    //for (auto &x: candidates) { //DEBUG-Keep for now
                    //    std::cout << ">>>" << unsigned(x.deliveryOrder) << " is broken " << x.broken << std::endl;
                    //}

                    foundHeadOfLineBlocking = true; //Found hole
                    headOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                    headOfLineBlockingTail = mBucketList[candidates[0].bucket].mDeliveryOrder; //This is the tail
                    //LOGGER(true, LOGG_NOTIFY, "HOL " << unsigned(expectedNextFrameToDeliver) << " "
                    //<< unsigned(bucketList[candidates[0].bucket].deliveryOrder)
                    //<< " tail " << unsigned(headOfLineBlockingTail)
                    //<< " savedPTS " << unsigned(savedPTS))
                }

                //Deliver only when head of line blocking is cleared and we're back to normal
                if (!foundHeadOfLineBlocking) {
                    for (auto &x: candidates) {

                        if (expectedNextFrameToDeliver != x.deliveryOrder && mHeadOfLineBlockingTimeout) {
                            foundHeadOfLineBlocking = true; //Found hole
                            headOfLineBlockingCounter = mHeadOfLineBlockingTimeout; //Number of times to spin this loop
                            headOfLineBlockingTail =
                                    x.deliveryOrder; //So we basically give the non existing data a chance to arrive..
                            //LOGGER(true, LOGG_NOTIFY, "HOL2 " << unsigned(expectedNextFrameToDeliver) << " " << unsigned(x.deliveryOrder) << " tail " << unsigned(headOfLineBlockingTail))
                            break;
                        }
                        expectedNextFrameToDeliver = x.deliveryOrder + 1;

                        //std::cout << unsigned(oldestFrameDelivered) << " " << unsigned(x.deliveryOrder) << std::endl;
                        if (oldestFrameDelivered <= x.deliveryOrder) {
                            oldestFrameDelivered = mHeadOfLineBlockingTimeout ? x.deliveryOrder : 0;
                            recieveCallback(mBucketList[x.bucket].mBucketData,
                                            mBucketList[x.bucket].mDataContent,
                                            mBucketList[x.bucket].mFragmentCounter != mBucketList[x.bucket].mOfFragmentNo,
                                            mBucketList[x.bucket].mPts,
                                            mBucketList[x.bucket].mCode,
                                            mBucketList[x.bucket].mStream,
                                            mBucketList[x.bucket].mFlags);
                        }
                        savedPTS = mBucketList[x.bucket].mPts;
                        mBucketList[x.bucket].mActive = false;
                        mBucketList[x.bucket].mBucketData = nullptr;
                    }
                }
            }
        }
        mNetMtx.unlock();

        //Is more than 75% of the buffer used. //FIXME notify the user in some way
        if (activeCount > (CIRCULAR_BUFFER_SIZE / 4) * 3) {
            LOGGER(true, LOGG_WARN, "Current active buckets are more than half the circular buffer.");
        }

    }
    mIsThreadActive = false;
}

//Start reciever worker thread
EdgewareFrameMessages EdgewareFrameProtocol::startUnpacker(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster) {
    if (mCurrentMode != EdgewareFrameMode::unpacker) {
        return EdgewareFrameMessages::wrongMode;
    }

    if (mIsThreadActive) {
        LOGGER(true, LOGG_WARN, "Unpacker already working");
        return EdgewareFrameMessages::unpackerAlreadyStarted;
    }
    if (bucketTimeoutMaster == 0) {
        LOGGER(true, LOGG_WARN, "bucketTimeoutMaster can't be 0");
        return EdgewareFrameMessages::parameterError;
    }
    if (holTimeoutMaster >= bucketTimeoutMaster) {
        LOGGER(true, LOGG_WARN, "holTimeoutMaster can't be less or equal to bucketTimeoutMaster");
        return EdgewareFrameMessages::parameterError;
    }

    mBucketTimeout = bucketTimeoutMaster;
    mHeadOfLineBlockingTimeout = holTimeoutMaster;
    mThreadActive =
            true; //you must set these parameters here to avoid races. For example calling start then stop before the thread actually starts.
    mIsThreadActive = true;
    std::thread(std::bind(&EdgewareFrameProtocol::unpackerWorker, this, bucketTimeoutMaster)).detach();
    return EdgewareFrameMessages::noError;
}

//Stop reciever worker thread
EdgewareFrameMessages EdgewareFrameProtocol::stopUnpacker() {
    std::lock_guard<std::mutex> lock(mUnpackMtx);

    if (mCurrentMode != EdgewareFrameMode::unpacker) {
        return EdgewareFrameMessages::wrongMode;
    }

    //Set the semaphore to stop thread
    mThreadActive = false;
    uint32_t lockProtect = 1000;
    //check for it to actually stop
    while (mIsThreadActive) {
        usleep(1000);
        if (!--lockProtect) {
            //we gave it a second now exit anyway
            LOGGER(true, LOGG_FATAL, "unpackerWorker thread not stopping. Quitting anyway");
            return EdgewareFrameMessages::failedStoppingUnpacker;
        }
    }
    return EdgewareFrameMessages::noError;
}

//Unpack method. We recieved a fragment of data or a full frame. Lets unpack it
EdgewareFrameMessages EdgewareFrameProtocol::unpack(const std::vector<uint8_t> &subPacket, uint8_t fromSource) {
    //Type 0 packet. Discard and continue
    //Type 0 packets can be used to fill with user data outside efp protocol packets just put a uint8_t = Frametype::type0 at position 0 and then any data.
    //Type 1 are frames larger than MTU
    //Type 2 are frames smaller than MTU
    //Type 2 packets are also used at the end of Type 1 packet superFrames
    //Type 3 frames carry the reminder of data when it's too large for type2 to carry.

    std::lock_guard<std::mutex> lock(mUnpackMtx);

    if (mCurrentMode != EdgewareFrameMode::unpacker) {
        return EdgewareFrameMessages::wrongMode;
    }

    if (!mIsThreadActive) {
        LOGGER(true, LOGG_ERROR, "Unpacker not started");
        return EdgewareFrameMessages::unpackerNotStarted;
    }

    if ((subPacket[0] & 0x0f) == Frametype::type0) {
        return EdgewareFrameMessages::type0Frame;
    } else if ((subPacket[0] & 0x0f) == Frametype::type1) {
        if (subPacket.size() < sizeof(EdgewareFrameType1)) {
            return EdgewareFrameMessages::framesizeMismatch;
        }
        return unpackType1(subPacket, fromSource);
    } else if ((subPacket[0] & 0x0f) == Frametype::type2) {
        if (subPacket.size() < sizeof(EdgewareFrameType2)) {
            return EdgewareFrameMessages::framesizeMismatch;
        }
        EdgewareFrameType2 type2Frame = *(EdgewareFrameType2 *) subPacket.data();
        if (type2Frame.ofFragmentNo == type2Frame.fragmentNo) {
            return unpackType2LastFrame(subPacket, fromSource);
        } else {
            return EdgewareFrameMessages::endOfPacketError;
        }
    } else if ((subPacket[0] & 0x0f) == Frametype::type3) {
        if (subPacket.size() < sizeof(EdgewareFrameType3)) {
            return EdgewareFrameMessages::framesizeMismatch;
        }
        return unpackType3(subPacket, fromSource);
    }

    //did not catch anything I understand
    return EdgewareFrameMessages::unknownFrametype;
}

//Pack data method. Fragments the data and calls the sendCallback method at the host level.
EdgewareFrameMessages
EdgewareFrameProtocol::packAndSend(const std::vector<uint8_t> &packet, EdgewareFrameContent dataContent, uint64_t pts,
                                   uint32_t code, uint8_t stream, uint8_t flags) {

    std::lock_guard<std::mutex> lock(mPackkMtx);

    if (sizeof(EdgewareFrameType1) != sizeof(EdgewareFrameType3)) {
        return EdgewareFrameMessages::type1And3SizeError;
    }

    if (mCurrentMode != EdgewareFrameMode::packer) {
        return EdgewareFrameMessages::wrongMode;
    }

    if (pts == UINT64_MAX) {
        return EdgewareFrameMessages::reservedPTSValue;
    }

    if (code == UINT32_MAX) {
        return EdgewareFrameMessages::reservedCodeValue;
    }

    if (stream == 0) {
        return EdgewareFrameMessages::reservedStreamValue;
    }

    flags &= 0xf0;

    //Will the data fit?
    //we know that we can send USHRT_MAX (65535) packets
    //the last packet will be a type2 packet.. so the current MTU muliplied with USHRT_MAX subtracting the space the protocol needs for the headers
    if (packet.size()
        > (((mCurrentMTU - sizeof(EdgewareFrameType1)) * (USHRT_MAX - 1)) + (mCurrentMTU - sizeof(EdgewareFrameType2)))) {
        return EdgewareFrameMessages::tooLargeFrame;
    }

    if ((packet.size() + sizeof(EdgewareFrameType2)) <= mCurrentMTU) {
        EdgewareFrameType2 type2Frame;
        type2Frame.superFrameNo = mSuperFrameNoGenerator;
        type2Frame.frameType |= flags;
        type2Frame.dataContent = dataContent;
        type2Frame.sizeOfData = (uint16_t) packet.size(); //The total size fits uint16_t since we cap the MTU to uint16_t
        type2Frame.pts = pts;
        type2Frame.code = code;
        type2Frame.stream = stream;
        try {
            std::vector<uint8_t> finalPacket(sizeof(EdgewareFrameType2) + packet.size());
            std::copy((uint8_t *) &type2Frame, ((uint8_t *) &type2Frame) + sizeof(EdgewareFrameType2),
                      finalPacket.begin());
            std::copy(packet.begin(), packet.end(), finalPacket.begin() + sizeof(EdgewareFrameType2));
            sendCallback(finalPacket);
        }
        catch (std::bad_alloc const &) {
            return EdgewareFrameMessages::memoryAllocationError;
        }
        mSuperFrameNoGenerator++;
        return EdgewareFrameMessages::noError;
    }

    uint16_t fragmentNo = 0;
    EdgewareFrameType1 type1Frame;
    type1Frame.frameType |= flags;
    type1Frame.stream = stream;
    type1Frame.superFrameNo = mSuperFrameNoGenerator;
    //The size is known for type1 packets no need to write it in any header.
    size_t dataPayloadType1 = (uint16_t) (mCurrentMTU - sizeof(EdgewareFrameType1));
    size_t dataPayloadType2 = (uint16_t) (mCurrentMTU - sizeof(EdgewareFrameType2));

    uint64_t dataPointer = 0;
    uint16_t ofFragmentNo = floor((double) (packet.size()) / (double) (mCurrentMTU - sizeof(EdgewareFrameType1)));
    uint16_t ofFragmentNoType1 = ofFragmentNo;
    bool type3needed = false;
    size_t reminderData = packet.size() - (ofFragmentNo * dataPayloadType1);
    if (reminderData > dataPayloadType2) {
        //We need a type3 frame. The reminder is too large for a type2 frame
        type3needed = true;
        ofFragmentNo++;
    }

    type1Frame.ofFragmentNo = ofFragmentNo;
    std::vector<uint8_t> finalPacketLoop(sizeof(EdgewareFrameType1) + dataPayloadType1);
    while (fragmentNo < ofFragmentNoType1) {
        type1Frame.fragmentNo = fragmentNo++;
        std::copy((uint8_t *) &type1Frame, ((uint8_t *) &type1Frame) + sizeof(EdgewareFrameType1),
                  finalPacketLoop.begin());
        std::copy(packet.begin() + dataPointer,
                  packet.begin() + dataPointer + dataPayloadType1,
                  finalPacketLoop.begin() + sizeof(EdgewareFrameType1));
        dataPointer += dataPayloadType1;
        sendCallback(finalPacketLoop);
    }

    if (type3needed) {
        fragmentNo++;
        std::vector<uint8_t> type3PacketData(sizeof(EdgewareFrameType3) + reminderData);
        EdgewareFrameType3 type3Frame;
        type3Frame.frameType |= flags;
        type3Frame.stream = type1Frame.stream;
        type3Frame.ofFragmentNo = type1Frame.ofFragmentNo;
        type3Frame.type1PacketSize = mCurrentMTU - sizeof(EdgewareFrameType1);
        type3Frame.superFrameNo = type1Frame.superFrameNo;
        std::copy((uint8_t *) &type3Frame, ((uint8_t *) &type3Frame) + sizeof(EdgewareFrameType3),
                  type3PacketData.begin());
        std::copy(packet.begin() + dataPointer,
                  packet.begin() + dataPointer + reminderData,
                  type3PacketData.begin() + sizeof(EdgewareFrameType3));
        dataPointer += reminderData;
        if (dataPointer != packet.size()) {
            return EdgewareFrameMessages::internalCalculationError;
        }
        sendCallback(type3PacketData);
    }

    //Create the last type2 packet
    size_t dataLeftToSend = packet.size() - dataPointer;

    if (type3needed && dataLeftToSend != 0) {
        return EdgewareFrameMessages::internalCalculationError;
    }

    //Debug me for calculation errors
    if (dataLeftToSend + sizeof(EdgewareFrameType2) > mCurrentMTU) {
        LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << packet.size());
        return EdgewareFrameMessages::internalCalculationError;
    }

    if (ofFragmentNo != fragmentNo) {
        return EdgewareFrameMessages::internalCalculationError;
    }

    EdgewareFrameType2 type2Frame;
    type2Frame.frameType |= flags;
    type2Frame.superFrameNo = mSuperFrameNoGenerator;
    type2Frame.fragmentNo = fragmentNo;
    type2Frame.ofFragmentNo = ofFragmentNo;
    type2Frame.dataContent = dataContent;
    type2Frame.sizeOfData = (uint16_t) dataLeftToSend;
    type2Frame.pts = pts;
    type2Frame.code = code;
    type2Frame.type1PacketSize = mCurrentMTU - sizeof(EdgewareFrameType1);
    std::vector<uint8_t> finalPacket(sizeof(EdgewareFrameType2) + dataLeftToSend);
    std::copy((uint8_t *) &type2Frame, ((uint8_t *) &type2Frame) + sizeof(EdgewareFrameType2), finalPacket.begin());
    if (dataLeftToSend) {
        std::copy(packet.begin() + dataPointer,
                  packet.begin() + dataPointer + dataLeftToSend,
                  finalPacket.begin() + sizeof(EdgewareFrameType2));
    }
    sendCallback(finalPacket);
    mSuperFrameNoGenerator++;
    return EdgewareFrameMessages::noError;
}

//Helper methods for embeding/extracting data in the payload part. It's not recommended to use these methods in production code as it's better to build the
//frames externally to avoid insert and copy of data.

EdgewareFrameMessages EdgewareFrameProtocol::addEmbeddedData(std::vector<uint8_t> *packet,
                                                             void *privateData,
                                                             size_t privateDataSize,
                                                             EdgewareEmbeddedFrameContent content,
                                                             bool isLast) {
    if (privateDataSize > UINT16_MAX) {
        return EdgewareFrameMessages::tooLargeEmbeddedData;
    }

    EdgewareFrameContentNamespace::EdgewareEmbeddedHeader embeddedHeader;
    embeddedHeader.size = privateDataSize;
    embeddedHeader.embeddedFrameType = content;
    if (isLast)
        embeddedHeader.embeddedFrameType =
                embeddedHeader.embeddedFrameType | EdgewareEmbeddedFrameContent::lastEmbeddedContent;
    packet->insert(packet->begin(), (uint8_t *) privateData, (uint8_t *) privateData + privateDataSize);
    packet->insert(packet->begin(), (uint8_t *) &embeddedHeader, (uint8_t *) &embeddedHeader + sizeof(embeddedHeader));
    return EdgewareFrameMessages::noError;
}

EdgewareFrameMessages EdgewareFrameProtocol::extractEmbeddedData(EdgewareFrameProtocol::pFramePtr &packet,
                                                                 std::vector<std::vector<uint8_t>> *embeddedDataList,
                                                                 std::vector<uint8_t> *dataContent,
                                                                 size_t *payloadDataPosition) {
    bool moreData = true;
    size_t headerSize = sizeof(EdgewareFrameContentNamespace::EdgewareEmbeddedHeader);
    do {
        EdgewareFrameContentNamespace::EdgewareEmbeddedHeader embeddedHeader =
                *(EdgewareFrameContentNamespace::EdgewareEmbeddedHeader *) (packet->framedata + *payloadDataPosition);
        if (embeddedHeader.embeddedFrameType == EdgewareEmbeddedFrameContent::illegal) {
            return EdgewareFrameMessages::illegalEmbeddedData;
        }
        dataContent->emplace_back((embeddedHeader.embeddedFrameType & 0x7f));
        std::vector<uint8_t> embeddedData(embeddedHeader.size);
        std::copy(packet->framedata + headerSize + *payloadDataPosition,
                  packet->framedata + headerSize + *payloadDataPosition + embeddedHeader.size,
                  embeddedData.begin());
        embeddedDataList->emplace_back(embeddedData);
        moreData = embeddedHeader.embeddedFrameType & 0x80;
        *payloadDataPosition += (embeddedHeader.size + headerSize);
        if (*payloadDataPosition >= packet->frameSize) {
            return EdgewareFrameMessages::bufferOutOfBounds;
        }
    } while (!moreData);
    return EdgewareFrameMessages::noError;
}

//Used by the unit tests
size_t EdgewareFrameProtocol::geType1Size() {
    return sizeof(EdgewareFrameType1);
}

size_t EdgewareFrameProtocol::geType2Size() {
    return sizeof(EdgewareFrameType2);
}



