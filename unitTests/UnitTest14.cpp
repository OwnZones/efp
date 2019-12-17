//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest14
//Send 15 packets with embeddedPrivateData. odd packet numbers will have two embedded private data fields. Also check for not broken and correct fourcc code.
//the reminder of the packet is a vector. Check it's integrity

#include "UnitTest14.h"

void UnitTest14::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest14::gotData(ElasticFrameProtocol::pFramePtr &packet) {
    ElasticFrameMessages info;
    std::vector<std::vector<uint8_t>> embeddedData;
    std::vector<uint8_t> embeddedContentFlag;
    size_t payloadDataPosition = 0;

    unitTestPacketNumberReciever++;
    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (packet->mCode != 'ANXB') {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (packet->mFlags & INLINE_PAYLOAD) {
        info=myEFPReciever->extractEmbeddedData(packet,&embeddedData,&embeddedContentFlag,&payloadDataPosition);

        if (info != ElasticFrameMessages::noError) {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }

        if (embeddedData.size() != embeddedContentFlag.size()) {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }

        if (unitTestPacketNumberReciever & 1) {
            if (embeddedData.size() != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                return;
            }
        }

        for (int x = 0; x<embeddedData.size();x++) {
            if(embeddedContentFlag[x] == ElasticEmbeddedFrameContent::embeddedprivatedata) {
                std::vector<uint8_t> thisVector = embeddedData[x];
                PrivateData myPrivateData = *(PrivateData *) thisVector.data();
                if (myPrivateData.myPrivateInteger != 10 || myPrivateData.myPrivateUint8_t != 44) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    return;
                }
            } else {
                unitTestFailed = true;
                unitTestActive = false;
                return;
            }

        }

        uint8_t vectorChecker = 0;
        for (int x = payloadDataPosition; x < packet->mFrameSize; x++) {
            if (packet->pFrameData[x] != vectorChecker++) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
        }
        if (unitTestPacketNumberReciever==15) {
            unitTestActive = false;
            std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        }
    } else {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
}

bool UnitTest14::waitForCompletion() {
    int breakOut = 0;
    while (unitTestActive) {
        usleep(1000 * 250); //quarter of a second
        if (breakOut++ == 10) {
            std::cout << "waitForCompletion did wait for 5 seconds. fail the test." << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
    }
    if (unitTestFailed) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed." << std::endl;
        return true;
    }
    return false;
}

bool UnitTest14::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocol();
    myEFPPacker = new (std::nothrow) ElasticFrameProtocol(MTU, ElasticFrameProtocolModeNamespace::sender);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest14::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest14::gotData, this, std::placeholders::_1);
    myEFPReciever->startReceiver(5, 2);

    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 0;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;

    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 15; packetNumber++) {
        mydata.clear();
        mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        PrivateData myPrivateData;
        PrivateData myPrivateDataExtra;
        if(!(packetNumber & 1)) {
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateDataExtra, sizeof(myPrivateDataExtra),ElasticEmbeddedFrameContent::embeddedprivatedata,true);
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData),ElasticEmbeddedFrameContent::embeddedprivatedata,false);
        } else {
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData),ElasticEmbeddedFrameContent::embeddedprivatedata,true);
        }


        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber+1, 'ANXB', streamID, INLINE_PAYLOAD);
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            myEFPReciever->stopReceiver();
            delete myEFPReciever;
            delete myEFPPacker;
            return false;
        }
    }

    if (waitForCompletion()){
        myEFPReciever->stopReceiver();
        delete myEFPReciever;
        delete myEFPPacker;
        return false;
    } else {
        myEFPReciever->stopReceiver();
        delete myEFPReciever;
        delete myEFPPacker;
        return true;
    }
}