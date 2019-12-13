//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest10
//send two type 2 packets out of order and recieve them in order.

#include "UnitTest10.h"

void UnitTest10::sendData(const std::vector<uint8_t> &subPacket) {
    EdgewareFrameMessages info;
    if (unitTestPacketNumberSender == 0) {
        unitTestsSavedData = subPacket;
    }

    if (unitTestPacketNumberSender == 1) {
        info = myEFPReciever->unpack(subPacket,0);
        if (info != EdgewareFrameMessages::noError) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
        info = myEFPReciever->unpack(unitTestsSavedData,0);
        if (info != EdgewareFrameMessages::noError) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
    }

    if (unitTestPacketNumberSender > 1) {
        unitTestFailed = true;
        unitTestActive = false;
    }
    unitTestPacketNumberSender++;
}

void UnitTest10::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    if (!unitTestActive) return;

    unitTestPacketNumberReciever++;
    if (unitTestPacketNumberReciever == 1) {
        if (pts == 1) {
            return;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberReciever == 2) {
        if (pts == 2) {
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

bool UnitTest10::waitForCompletion() {
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

bool UnitTest10::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest10::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest10::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(MTU-myEFPPacker->geType2Size());
    unitTestActive = true;
    result = myEFPPacker->packAndSend(mydata, EdgewareFrameContent::h264,1,0,streamID,NO_FLAGS);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method. Error-> " << signed(result)
                  << std::endl;
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return false;
    }

    result = myEFPPacker->packAndSend(mydata, EdgewareFrameContent::h264,2,0,streamID,NO_FLAGS);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method. Error-> " << signed(result)
                  << std::endl;
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return false;
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