//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest11
//Test sending 5 packets, 5 type 1 + 1 type 2..
//Reverse the packets to the unpacker and drop the middle packet (packet 3)
//This is testing the out of order head of line blocking mechanism
//The result should be deliver packet 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.

#include "UnitTest11.h"

void UnitTest11::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
    if (subPacket[0] == 2) {
        unitTestPacketNumberSender++;
        unitTestsSavedData2D.push_back(subPacket);
        unitTestsSavedData3D.push_back(unitTestsSavedData2D);
        if (unitTestPacketNumberSender == 5) {
            for (size_t item=unitTestsSavedData3D.size();item > 0;item--) {
                for (auto &x: unitTestsSavedData3D[item-1]) {
                    if (item != 3) {
                        info = myEFPReciever->receiveFragment(x,0);
                        if (info != ElasticFrameMessages::noError) {
                            std::cout << "Error-> " << signed(info) << std::endl;
                            unitTestFailed = true;
                            unitTestActive = false;
                        }
                    }
                }
            }
        }
        unitTestsSavedData2D.clear();
        return;
    }
    unitTestsSavedData2D.push_back(subPacket);
}

void UnitTest11::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (!unitTestActive) return;

    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    unitTestPacketNumberReciever++;
    if (unitTestPacketNumberReciever == 1) {
        if (packet->mPts == 1001) {
            expectedPTS=1002;
            return;
        }
        if (packet->mPts == 1002) {
            expectedPTS=1004;
            return;
        }
        if (packet->mPts == 1004) {
            expectedPTS=1005;
            return;
        }
        if (packet->mPts == 1005) {
            unitTestActive = false;
            std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberReciever == 2) {
        if (expectedPTS == packet->mPts) {
            if (packet->mPts == 1002) {
                expectedPTS=1004;
            }
            if (packet->mPts == 1004) {
                expectedPTS=1005;
            }
            if (packet->mPts == 1005) {
                unitTestActive = false;
                std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
            }
            return;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberReciever == 3) {
        if (expectedPTS == packet->mPts) {
            if (packet->mPts == 1004) {
                expectedPTS=1005;
            }
            if (packet->mPts == 1005) {
                unitTestActive = false;
                std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
            }
            return;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberReciever == 4) {
        if (expectedPTS == packet->mPts) {
            unitTestActive = false;
            std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
            return;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    unitTestFailed = true;
    unitTestActive = false;
}

bool UnitTest11::waitForCompletion() {
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

bool UnitTest11::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocolReceiver(5,2);
    myEFPPacker = new (std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest11::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest11::gotData, this, std::placeholders::_1);
    mydata.clear();
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 1000;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 5; packetNumber++) {
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,packetNumber + 1, 0, streamID, NO_FLAGS);
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