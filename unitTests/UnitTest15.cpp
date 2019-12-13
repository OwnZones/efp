//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest15
//This is the crazy-monkey test1. We randomize the size for 1000 packets. We store the size in a private struct and embedd it.
//when we receive the packet we check the size saved in the embedded data and also the linear vector in the payload.

#include "UnitTest15.h"

void UnitTest15::sendData(const std::vector<uint8_t> &subPacket) {
    EdgewareFrameMessages info = myEFPReciever->unpack(subPacket,0);
    if (info != EdgewareFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest15::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    EdgewareFrameMessages info;
    std::vector<std::vector<uint8_t>> embeddedData;
    std::vector<uint8_t> embeddedContentFlag;
    size_t payloadDataPosition = 0;

    unitTestPacketNumberReciever++;
    if (broken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (unitTestPacketNumberReciever != pts) {
        std::cout << "Got PTS -> " << unsigned(pts) << " Expected -> " << unsigned(unitTestPacketNumberReciever) << std::endl;
        unitTestPacketNumberReciever = pts; //if you want to continue remove the lines under to the break
        unitTestFailed = true;
        unitTestActive = false;
        return;

    }

    if (flags & INLINE_PAYLOAD) {
        info = myEFPReciever->extractEmbeddedData(packet, &embeddedData, &embeddedContentFlag,
                                                 &payloadDataPosition);

        if (info != EdgewareFrameMessages::noError) {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }

        for (int x = 0; x<embeddedData.size();x++) {
            if(embeddedContentFlag[x] == EdgewareEmbeddedFrameContent::embeddedPrivateData) {
                std::vector<uint8_t> thisVector = embeddedData[x];
                PrivateData myPrivateData = *(PrivateData *) thisVector.data();
                size_t thisStuff=sizeof(PrivateData);

                if (myPrivateData.sizeOfData != packet->frameSize) {
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
                for (int x = payloadDataPosition; x < packet->frameSize; x++) {
                    if (packet->framedata[x] != vectorChecker++) {
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

bool UnitTest15::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    EdgewareFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) EdgewareFrameProtocol();
    myEFPPacker = new (std::nothrow) EdgewareFrameProtocol(MTU, EdgewareFrameProtocolModeNamespace::packer);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest15::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest15::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);

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
        myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData), EdgewareEmbeddedFrameContent::embeddedPrivateData, true);
        if (myPrivateData.sizeOfData != mydata.size()) {
            std::cout << "Packer error"
                      << std::endl;
        }

        result = myEFPPacker->packAndSend(mydata, EdgewareFrameContent::h264, packetNumber+1, 'ANXB', streamID, INLINE_PAYLOAD);
        if (result != EdgewareFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            myEFPReciever->stopUnpacker();
            delete myEFPReciever;
            delete myEFPPacker;
            return false;
        }
    }

    if (waitForCompletion()){
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return false;
    } else {
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return true;
    }
}