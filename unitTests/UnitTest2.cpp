//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest2
//Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent and only one recieved

#include "UnitTest2.h"

void UnitTest2::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
    info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
}

void UnitTest2::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {

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
    if (packet->mDataContent != ElasticFrameContentNamespace::adts) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest2::waitForCompletion() {
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

bool UnitTest2::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest2::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest2::gotData, this, std::placeholders::_1);
    mydata.resize(MTU - myEFPPacker->geType2Size());
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