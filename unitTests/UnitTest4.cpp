//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest4
//Test sending a packet of MTU-headertyp1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the reciever

#include "UnitTest4.h"

void UnitTest4::sendData(const std::vector<uint8_t> &subPacket) {
    EdgewareFrameMessages info;
    if (subPacket[0] != 1 && unitTestPacketNumberSender == 0) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (subPacket[0] != 2 && unitTestPacketNumberSender == 1) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberSender > 1) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (subPacket[0] == 1 && unitTestPacketNumberSender == 0) {
        //expected size of first packet
        if (subPacket.size() != MTU) {
            unitTestFailed = true;
            unitTestActive = false;
        }
    }
    if (subPacket[0] == 2 && unitTestPacketNumberSender == 1) {
        //expected size of first packet
        if (subPacket.size() != (myEFPPacker->geType2Size() + 1)) {
            unitTestFailed = true;
            unitTestActive = false;
        }
    }
    unitTestPacketNumberSender++;
    info=myEFPReciever->unpack(subPacket,0);
    if (info != EdgewareFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest4::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    if (pts != 1 || code != 2) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (broken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (packet->frameSize != (MTU - myEFPPacker->geType1Size()) + 1) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest4::waitForCompletion() {
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

bool UnitTest4::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest4::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest4::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);
    unitTestPacketNumberSender = 0;
    mydata.resize((MTU - myEFPPacker->geType1Size()) + 1);
    unitTestActive = true;
    result = myEFPPacker->packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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