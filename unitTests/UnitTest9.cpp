//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest9
//Test sending packets, 5 type 1 + 1 type 2.. Drop the type 2 packet.
//broken should be set, the PTS and code should be set to the illegal value and the vector should be linear for 5 times MTU - myEFPPacker.geType1Size()

#include "UnitTest9.h"

void UnitTest9::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
    if (subPacket[0] == 2) {
        return;
    }
    info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest9::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (!unitTestActive) return;

    if (!packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }
    if (packet->mPts != UINT64_MAX) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    uint8_t vectorChecker = 0;
    for (int x = 0; x < ((MTU - myEFPPacker->geType1Size()) * 5); x++) {
        if (packet->pFrameData[x] != vectorChecker++) {

            unitTestFailed = true;
            unitTestActive = false;
            break;
        }
    }
    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest9::waitForCompletion() {
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

bool UnitTest9::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest9::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest9::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
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