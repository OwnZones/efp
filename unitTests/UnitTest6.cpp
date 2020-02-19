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
    ElasticFrameMessages info =myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest6::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (packet->mPts != 1001 || packet->mCode != 2) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    //One block of MTU should be gone
    if (packet->mFrameSize != (((MTU - myEFPPacker->geType1Size()) * 2) + 12)) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (!packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    uint8_t vectorChecker = (MTU - myEFPPacker->geType1Size()) % 256;
    for (size_t x = (MTU - myEFPPacker->geType1Size()); x < packet->mFrameSize; x++) {
        if (packet->pFrameData[x] != vectorChecker++) {
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

bool UnitTest6::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest6::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest6::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 2) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
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