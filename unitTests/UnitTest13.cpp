//
// Created by Anders Cedronius on 2019-12-05.
//

//FIXME-- Test is currently only sending the packets.
//UnitTest13
//Test sending 100 000 superframes of size from 500 to 10.000 bytes
//Reverse the packets to the unpacker and drop the middle packet (packet 3) also deliver the fragments reversed meaning packet 5 last fragment first..
//This is testing the out of order head of line blocking mechanism
//The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.

#include "UnitTest13.h"

void UnitTest13::sendData(const std::vector<uint8_t> &subPacket) {
    EdgewareFrameMessages info = myEFPReciever->unpack(subPacket,0);
    if (info != EdgewareFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest13::gotData(EdgewareFrameProtocol::pFramePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
    if (broken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    unitTestPacketNumberReciever++;

    if (unitTestPacketNumberReciever != pts) {
        std::cout << "Got PTS -> " << unsigned(pts) << " Expected -> " << unsigned(unitTestPacketNumberReciever) << std::endl;
        unitTestPacketNumberReciever = pts; //if you want to continue remove the lines under to the break
        unitTestFailed = true;
        unitTestActive = false;
        return;

    }

    if (packet->frameSize != (((MTU - myEFPPacker->geType1Size()) * 5) + 12)) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (unitTestPacketNumberReciever < 100000) {
        if (!(unitTestPacketNumberReciever % 1000)) {
            std::cout << "Got packet number " << unsigned(unitTestPacketNumberReciever) << std::endl;
        }
        return;
    }

    if (unitTestPacketNumberReciever == 100000) {
        unitTestActive = false;
        std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        return;
    }
    unitTestFailed = true;
    unitTestActive = false;
}

bool UnitTest13::waitForCompletion() {
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

bool UnitTest13::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest13::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest13::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 0;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;

    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 100000; packetNumber++) {

        mydata.clear();
        mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
        result = myEFPPacker->packAndSend(mydata, EdgewareFrameContent::h264, packetNumber+1, 0, streamID, INLINE_PAYLOAD);
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