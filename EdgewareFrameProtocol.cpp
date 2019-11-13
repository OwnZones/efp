//
// Created by Anders Cedronius on 2019-11-11.
//

#include "EdgewareFrameProtocol.h"

EdgewareFrameProtocol::EdgewareFrameProtocol(uint32_t setMTU) {
    if (setMTU > UINT_MAX) {
        LOGGER(true, LOGG_FATAL, "MTU Larger than 65535, that is illegal.");
    } else {
        currentMTU = setMTU;
    }
    threadActive = false;
    isThreadActive = false;
    LOGGER(true, LOGG_NOTIFY, "EdgewareFrameProtocol constructed");
}

EdgewareFrameProtocol::~EdgewareFrameProtocol() {
    LOGGER(true, LOGG_NOTIFY, "EdgewareFrameProtocol destruct");
}

void EdgewareFrameProtocol::sendData(const std::vector<uint8_t> &subPacket) {
    LOGGER(true, LOGG_ERROR, "Implement the sendCallback method for the protocol to work.");
}

void EdgewareFrameProtocol::gotData(const std::vector<uint8_t> &packet, EdgewareFrameContent content, bool broken) {
    LOGGER(true, LOGG_ERROR, "Implement the recieveCallback method for the protocol to work.");
}

uint64_t EdgewareFrameProtocol::superFrameRecalculator(uint16_t superframe) {
    if (superFrameFirstTime) {
        oldSuperframeNumber = (int64_t) superframe;
        superFrameRecalc = oldSuperframeNumber;
        superFrameFirstTime = false;
        return superFrameRecalc;
    }

    int64_t superFrameDiff = (int64_t) superframe - oldSuperframeNumber;
    oldSuperframeNumber = (int64_t) superframe;

    if (superFrameDiff > 0) {
        superFrameRecalc += superFrameDiff;
    } else {
        superFrameRecalc += ((UINT16_MAX + 1) - abs(superFrameDiff));
    }
    return superFrameRecalc;
}

EdgewareFrameMessages EdgewareFrameProtocol::unpackType1(const std::vector<uint8_t> &subPacket) {
    std::lock_guard<std::mutex> lock(netMtx);

    EdgewareFrameType1 type1Frame = *(EdgewareFrameType1 *) subPacket.data();
    Bucket *thisBucket = &bucketList[(uint8_t) type1Frame.superFrameNo];

    if (!thisBucket->active) {
        //LOGGER(false,LOGG_NOTIFY,"Setting: " << unsigned(type1Frame.superFrameNo));
        thisBucket->active = true;
        thisBucket->haveRecievedPacket.reset();
        thisBucket->haveRecievedPacket[type1Frame.fragmentNo] = 1;
        thisBucket->deliveryOrder = superFrameRecalculator(type1Frame.superFrameNo);
        thisBucket->dataContent = type1Frame.dataContent;
        thisBucket->timeout = bucketTimeout;
        thisBucket->fragmentCounter = 0;
        thisBucket->ofFragmentNo = type1Frame.ofFragmentNo;
        thisBucket->fragmentSize = (subPacket.size() - sizeof(EdgewareFrameType1));
        thisBucket->bucketData.clear();
        thisBucket->bucketData.insert(thisBucket->bucketData.end(), subPacket.begin() + sizeof(EdgewareFrameType1),
                                      subPacket.end());
        return EdgewareFrameMessages::noError;
    }

    if (thisBucket->ofFragmentNo < type1Frame.fragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        thisBucket->active = false;
        return EdgewareFrameMessages::bufferOutOfBounds;
    }

    if (thisBucket->haveRecievedPacket[type1Frame.fragmentNo] == 1) {
        return EdgewareFrameMessages::duplicatePacketRecieved;
    } else {
        thisBucket->haveRecievedPacket[type1Frame.fragmentNo] = 1;
    }

    if (!thisBucket->fragmentSize) {
        thisBucket->fragmentSize = (subPacket.size() - sizeof(EdgewareFrameType1));
    }

    thisBucket->timeout = bucketTimeout;
    thisBucket->fragmentCounter++;

    if (thisBucket->fragmentCounter == type1Frame.fragmentNo) {
        thisBucket->bucketData.insert(thisBucket->bucketData.end(), subPacket.begin() + sizeof(EdgewareFrameType1),
                                      subPacket.end());
        return EdgewareFrameMessages::noError;
    }

    size_t insertDataPointer = thisBucket->fragmentSize * type1Frame.fragmentNo;
    if (thisBucket->bucketData.size() < insertDataPointer) {
        thisBucket->bucketData.insert(thisBucket->bucketData.end(), subPacket.begin() + sizeof(EdgewareFrameType1),
                                      subPacket.end());
        return EdgewareFrameMessages::noError;
    }

    thisBucket->bucketData.insert(thisBucket->bucketData.begin() + insertDataPointer,
                                  subPacket.begin() + sizeof(EdgewareFrameType1), subPacket.end());
    return EdgewareFrameMessages::noError;
}

EdgewareFrameMessages EdgewareFrameProtocol::unpackType2LastFrame(const std::vector<uint8_t> &subPacket) {
    std::lock_guard<std::mutex> lock(netMtx);
    EdgewareFrameType2 type2Frame = *(EdgewareFrameType2 *) subPacket.data();
    Bucket *thisBucket = &bucketList[(uint8_t) type2Frame.superFrameNo];

    if (!thisBucket->active) {
        thisBucket->active = true;
        thisBucket->haveRecievedPacket.reset();
        thisBucket->haveRecievedPacket[type2Frame.fragmentNo] = 1;
        thisBucket->deliveryOrder = superFrameRecalculator(type2Frame.superFrameNo);
        thisBucket->dataContent = type2Frame.dataContent;
        thisBucket->timeout = bucketTimeout;
        thisBucket->fragmentCounter = 0;
        thisBucket->ofFragmentNo = type2Frame.ofFragmentNo;
        thisBucket->fragmentSize = 0;
        thisBucket->bucketData.clear();
        thisBucket->bucketData.insert(thisBucket->bucketData.end(), subPacket.begin() + sizeof(EdgewareFrameType2),
                                      subPacket.end());
        return EdgewareFrameMessages::noError;
    }

    if (thisBucket->ofFragmentNo < type2Frame.fragmentNo) {
        LOGGER(true, LOGG_FATAL, "bufferOutOfBounds");
        thisBucket->active = false;
        return EdgewareFrameMessages::bufferOutOfBounds;
    }

    if (thisBucket->haveRecievedPacket[type2Frame.fragmentNo] == 1) {
        return EdgewareFrameMessages::duplicatePacketRecieved;
    } else {
        thisBucket->haveRecievedPacket[type2Frame.fragmentNo] = 1;
    }

    thisBucket->timeout = bucketTimeout;
    thisBucket->fragmentCounter++;
    thisBucket->bucketData.insert(thisBucket->bucketData.end(), subPacket.begin() + sizeof(EdgewareFrameType2),
                                  subPacket.end());
    return EdgewareFrameMessages::noError;
}

void EdgewareFrameProtocol::unpackerWorker(uint32_t timeout) {
    threadActive = true;
    isThreadActive = true;
    bool foundHeadOfLineBlocking = false;
    uint32_t headOfLineBlockingCounter = 0;
    uint64_t headOfLineBlockingTail = 0;

    while (threadActive) {
        usleep(1000); //Check all active buckets each milisecond
        uint32_t activeCount = 0;
        std::vector<CandidateToDeliver> vec;
        uint64_t deliveryOrderOldest = UINT64_MAX;

        bool clearHeadOfLineBuckets=false;
        //If im in head of blocking garbage collect mode.
        if (foundHeadOfLineBlocking) {
            if (headOfLineBlockingCounter) {
                headOfLineBlockingCounter--;
            } else {
                clearHeadOfLineBuckets=true;
                foundHeadOfLineBlocking=false;
            }
        }

        netMtx.lock();
        for (int i = 0; i < UINT8_MAX; i++) {
            if (bucketList[i].active) {

                //save the oldest bucket in queue to be delivered
                if (deliveryOrderOldest > bucketList[i].deliveryOrder) {
                    deliveryOrderOldest = bucketList[i].deliveryOrder;
                }

                if ((bucketList[i].deliveryOrder < headOfLineBlockingTail) && clearHeadOfLineBuckets) {
                    bucketList[i].timeout = 1;
                }

                activeCount++;
                if (bucketList[i].fragmentCounter == bucketList[i].ofFragmentNo) {
                    vec.push_back(CandidateToDeliver(bucketList[i].deliveryOrder, i, false));
                } else {
                    if (!--bucketList[i].timeout) {
                        vec.push_back(CandidateToDeliver(bucketList[i].deliveryOrder, i, true));
                        bucketList[i].timeout = 1; //We want to timeout this again if head of line blocking is on
                    }

                }
            }
        }

        if (vec.size()) {
            std::sort(vec.begin(), vec.end(), sortDeliveryOrder());

            //Check for head of line blocking only if hol-timoeut is set
            if (deliveryOrderOldest < bucketList[vec[0].bucket].deliveryOrder && headOfLineBlockingTimeout && !foundHeadOfLineBlocking) {
                LOGGER(false, LOGG_NOTIFY, "HOL found");
                foundHeadOfLineBlocking = true;
                headOfLineBlockingCounter = headOfLineBlockingTimeout;
                headOfLineBlockingTail = bucketList[vec[0].bucket].deliveryOrder;
            }

            //Deliver only when head of line blocking is cleared
            if (!foundHeadOfLineBlocking) {
                for (auto &x: vec) {
                    recieveCallback(bucketList[x.bucket].bucketData, bucketList[x.bucket].dataContent, x.broken);
                    bucketList[x.bucket].active = false;
                }
            }
        }
        netMtx.unlock();

        if (activeCount == UINT8_MAX / 2) {
            LOGGER(true, LOGG_FATAL, "Current active buckets are more than half the circular buffer.");
        }

    }
    isThreadActive = false;
}

void EdgewareFrameProtocol::startUnpacker(uint32_t bucketTimeoutMaster, uint32_t holTimeoutMaster) {
    bucketTimeout = bucketTimeoutMaster;
    headOfLineBlockingTimeout = holTimeoutMaster;
    std::thread(std::bind(&EdgewareFrameProtocol::unpackerWorker, this, bucketTimeoutMaster)).detach();
}

void EdgewareFrameProtocol::stopUnpacker() {
    threadActive = false;
    uint32_t lockProtect = 1000;
    while (isThreadActive) {
        usleep(1000);
        if (!--lockProtect) {
            LOGGER(true, LOGG_FATAL, "Thread not stopping fatal.");
            break;
        }
    }
}

EdgewareFrameMessages EdgewareFrameProtocol::unpack(const std::vector<uint8_t> &subPacket) {
    if (subPacket[0] == Frametype::type0) {
        return EdgewareFrameMessages::noError;
    } else if (subPacket[0] == Frametype::type1) {
        return unpackType1(subPacket);
    } else if (subPacket[0] == Frametype::type2) {
        if (subPacket.size() < sizeof(EdgewareFrameType2)) {
            return EdgewareFrameMessages::framesizeMismatch;
        }
        EdgewareFrameType2 type2Frame = *(EdgewareFrameType2 *) subPacket.data();
        if (type2Frame.ofFragmentNo > 0) {
            if (type2Frame.ofFragmentNo == type2Frame.fragmentNo) {
                return unpackType2LastFrame(subPacket);
            } else {
                return EdgewareFrameMessages::endOfPacketError;
            }
        }
        recieveCallback(std::vector<uint8_t>(subPacket.begin() + sizeof(EdgewareFrameType2), subPacket.end()),
                        type2Frame.dataContent, false);
    } else {
        return EdgewareFrameMessages::unknownFrametype;
    }

    return EdgewareFrameMessages::noError;
}

EdgewareFrameMessages
EdgewareFrameProtocol::packAndSend(const std::vector<uint8_t> &packet, EdgewareFrameContent dataContent) {
    if (packet.size() > currentMTU * UINT_MAX) {
        return EdgewareFrameMessages::tooLargeFrame;
    }

    if ((packet.size() + sizeof(EdgewareFrameType2)) <= currentMTU) {
        EdgewareFrameType2 type2Frame;
        type2Frame.superFrameNo = superFrameNo;
        type2Frame.dataContent = dataContent;
        type2Frame.sizeOfData = (uint16_t) packet.size();
        std::vector<uint8_t> finalPacket;
        finalPacket.insert(finalPacket.end(), (uint8_t *) &type2Frame, ((uint8_t *) &type2Frame) + sizeof type2Frame);
        finalPacket.insert(finalPacket.end(), packet.begin(), packet.end());
        sendCallback(finalPacket);
        superFrameNo++;
        return EdgewareFrameMessages::noError;
    }

    uint16_t fragmentNo = 0;
    EdgewareFrameType1 type1Frame;
    type1Frame.dataContent = dataContent;
    type1Frame.superFrameNo = superFrameNo;
    size_t dataPayload = (uint16_t) (currentMTU - sizeof(EdgewareFrameType1));

    uint64_t dataPointer = 0;

    size_t diffFrames = sizeof(EdgewareFrameType2) - sizeof(EdgewareFrameType1);
    uint16_t ofFragmentNo =
            ceil((double) (packet.size() + diffFrames) / (double) (currentMTU - sizeof(EdgewareFrameType1))) - 1;
    type1Frame.ofFragmentNo = ofFragmentNo;

    for (; fragmentNo < ofFragmentNo; fragmentNo++) {
        type1Frame.fragmentNo = fragmentNo;
        std::vector<uint8_t> finalPacket;
        finalPacket.insert(finalPacket.end(), (uint8_t *) &type1Frame, ((uint8_t *) &type1Frame) + sizeof type1Frame);
        finalPacket.insert(finalPacket.end(), packet.begin() + dataPointer, packet.begin() + dataPointer + dataPayload);
        dataPointer += dataPayload;
        sendCallback(finalPacket);
    }

    size_t dataLeftToSend = packet.size() - dataPointer;
    //Debug me for calculation errors
    if (dataLeftToSend + sizeof(EdgewareFrameType2) > currentMTU) {
        LOGGER(true, LOGG_FATAL, "Calculation bug.. Value that made me sink -> " << packet.size());
        return EdgewareFrameMessages::internalCalculationError;
    }

    EdgewareFrameType2 type2Frame;
    type2Frame.superFrameNo = superFrameNo;
    type2Frame.fragmentNo = fragmentNo;
    type2Frame.ofFragmentNo = ofFragmentNo;
    type2Frame.dataContent = dataContent;
    type2Frame.sizeOfData = (uint16_t) dataLeftToSend;
    std::vector<uint8_t> finalPacket;
    finalPacket.insert(finalPacket.end(), (uint8_t *) &type2Frame, ((uint8_t *) &type2Frame) + sizeof type2Frame);
    finalPacket.insert(finalPacket.end(), packet.begin() + dataPointer, packet.begin() + dataPointer + dataLeftToSend);
    sendCallback(finalPacket);

    superFrameNo++;
    return EdgewareFrameMessages::noError;
}