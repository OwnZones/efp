//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest6
//Test sending a packet of MTU*5+MTU/2 containing a linear vector drop the first packet -> the result should be a packet with a hole of MTU-headertype1
//then a linear vector of data starting with the number (MTU-headertyp1) % 256. also check for broken flag is set.

#include "UnitTest6.h"

void UnitTest6::sendData(const std::vector<uint8_t> &subPacket) {
    if (!unitTestPacketNumberSender++) {
        return;
    }
    EdgewareFrameMessages info =myEFPReciever->unpack(subPacket,0);
    if (info != EdgewareFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest6::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    if (pts != 1 || code != 2) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    //One block of MTU should be gone
    if (packet->frameSize != (((MTU - myEFPPacker->geType1Size()) * 2) + 12)) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (!broken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    uint8_t vectorChecker = (MTU - myEFPPacker->geType1Size()) % 256;
    for (int x = (MTU - myEFPPacker->geType1Size()); x < packet->frameSize; x++) {
        if (packet->framedata[x] != vectorChecker++) {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }
    }
    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest6::waitForCompletion() {
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

bool UnitTest6::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest6::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest6::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);
    unitTestPacketNumberSender = 0;
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 2) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
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