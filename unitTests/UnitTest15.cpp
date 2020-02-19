//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest15
//This is the crazy-monkey test1. We randomize the size for 1000 packets. We store the size in a private struct and embedd it.
//when we receive the packet we check the size saved in the embedded data and also the linear vector in the payload.

#include "UnitTest15.h"

void UnitTest15::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest15::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
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

    if (unitTestPacketNumberReciever+1000 != packet->mPts) {
        std::cout << "Got PTS -> " << unsigned(packet->mPts) << " Expected -> " << unsigned(unitTestPacketNumberReciever) << std::endl;
        unitTestPacketNumberReciever = packet->mPts; //if you want to continue remove the lines under to the break
        unitTestFailed = true;
        unitTestActive = false;
        return;

    }

    if (packet->mFlags & INLINE_PAYLOAD) {
        info = myEFPReciever->extractEmbeddedData(packet, &embeddedData, &embeddedContentFlag,
                                                 &payloadDataPosition);

        if (info != ElasticFrameMessages::noError) {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }

        for (int x = 0; x<embeddedData.size();x++) {
            if(embeddedContentFlag[x] == ElasticEmbeddedFrameContent::embeddedprivatedata) {
                std::vector<uint8_t> thisVector = embeddedData[x];
                PrivateData myPrivateData = *(PrivateData *) thisVector.data();
                size_t thisStuff=sizeof(PrivateData);

                if (myPrivateData.sizeOfData != packet->mFrameSize) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    return;
                }

                if (myPrivateData.myPrivateInteger != 10 || myPrivateData.myPrivateUint8_t != 44) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    return;
                }

                uint8_t vectorChecker = 0;
                for (size_t x = payloadDataPosition; x < packet->mFrameSize; x++) {
                    if (packet->pFrameData[x] != vectorChecker++) {
                        unitTestFailed = true;
                        unitTestActive = false;
                        return;
                    }
                }
            } else {
                unitTestFailed = true;
                unitTestActive = false;
                return;
            }

        }
        if (unitTestPacketNumberReciever < 1000) {
            if (!(unitTestPacketNumberReciever % 100)) {
                std::cout << "Got packet number " << unsigned(unitTestPacketNumberReciever) << std::endl;
            }
            return;
        }

        if (unitTestPacketNumberReciever == 1000) {
            unitTestActive = false;
            std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
            return;
        }

    }
}

bool UnitTest15::waitForCompletion() {
    int breakOut = 0;
    while (unitTestActive) {
        //quarter of a second
        std::this_thread::sleep_for(std::chrono::microseconds(1000 * 250));
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

bool UnitTest15::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocolReceiver(10, 4);
    myEFPPacker = new (std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest15::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest15::gotData, this, std::placeholders::_1);

    unitTestPacketNumberReciever = 0;

    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 1000; packetNumber++) {
        mydata.clear();

        // std::cout << "bip " << unsigned(packetNumber) << std::endl;

        size_t randSize = rand() % 1000000 + 1;
        //size_t randSize = (MTU*2-(myEFPPacker.geType1Size()*2)-(1+sizeof(PrivateData) + 4));
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        PrivateData myPrivateData;
        myPrivateData.sizeOfData = mydata.size() + sizeof(PrivateData) + 4; //4 is the embedded frame header size
        myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData), ElasticEmbeddedFrameContent::embeddedprivatedata, true);
        if (myPrivateData.sizeOfData != mydata.size()) {
            std::cout << "Packer error"
                      << std::endl;
        }

        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber+1001, packetNumber+1, EFP_CODE('A', 'N', 'X', 'B'), streamID, INLINE_PAYLOAD);
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
    }

    if (waitForCompletion()){
        delete myEFPPacker;
        delete myEFPReciever;
        return false;
    } else {
        delete myEFPPacker;
        delete myEFPReciever;
        return true;
    }
}