//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest10
//send two type 2 packets out of order and recieve them in order.

#include "UnitTest10.h"

void UnitTest10::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
    if (unitTestPacketNumberSender == 0) {
        unitTestsSavedData = subPacket;
    }

    if (unitTestPacketNumberSender == 1) {
        info = myEFPReciever->receiveFragment(subPacket,0);
        if (info != ElasticFrameMessages::noError) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
        info = myEFPReciever->receiveFragment(unitTestsSavedData,0);
        if (info != ElasticFrameMessages::noError) {
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

void UnitTest10::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (!unitTestActive) return;

    unitTestPacketNumberReciever++;
    if (unitTestPacketNumberReciever == 1) {
        if (packet->mPts == 1001) {
            return;
        }
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (unitTestPacketNumberReciever == 2) {
        if (packet->mPts == 1002) {
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

bool UnitTest10::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocolReceiver(5, 2);
    myEFPPacker = new (std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest10::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest10::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(MTU-myEFPPacker->geType2Size());
    unitTestActive = true;
    result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264,1001,1,0,streamID,NO_FLAGS);
    if (result != ElasticFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method. Error-> " << signed(result)
                  << std::endl;

        delete myEFPPacker;
        delete myEFPReciever;
        return false;
    }

    result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264,1002,2,0,streamID,NO_FLAGS);
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