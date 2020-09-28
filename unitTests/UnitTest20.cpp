//
// Created by Anders Cedronius on 2020-09-26.
//

//UnitTest20
//Test basic run to completion
//Test sending a packet of MTU-headertyp1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the receiver

#include "UnitTest20.h"

void UnitTest20::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
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
    info=myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest20::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {

    if (packet -> mStreamID != 4) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (packet->mPts != 1001 || packet->mCode != 2) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (packet->mFrameSize != (MTU - myEFPPacker->geType1Size()) + 1) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest20::waitForCompletion() {
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

bool UnitTest20::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=4;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocolReceiver(50, 20, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION);
    myEFPPacker = new (std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest20::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest20::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    mydata.resize((MTU - myEFPPacker->geType1Size()) + 1);
    unitTestActive = true;
    result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts,1001,1,2,streamID,NO_FLAGS);
    if (result != ElasticFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method. Error-> " << signed(result)
                  << std::endl;
        delete myEFPPacker;
        delete myEFPReciever;
        return false;
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