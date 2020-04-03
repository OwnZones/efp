//
// Created by Anders Cedronius on 2020-04-03.
//

//UnitTest18

#include "UnitTest18.h"

//UnitTest18
//Using the optional lambda in the pack and send method

void UnitTest18::sendData(const std::vector<uint8_t> &subPacket) {
        unitTestFailed = true;
        unitTestActive = false;
}

void UnitTest18::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    unitTestPacketNumberReciever++;

    if (unitTestPacketNumberReciever == 5) {
        unitTestActive = false;
        std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        return;
    }
}


bool UnitTest18::waitForCompletion() {
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

bool UnitTest18::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(10, 4);
    myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest18::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest18::gotData, this, std::placeholders::_1);

    mydata.clear();
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 1000;
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
    unitTestActive = true;

    for (int packetNumber = 0; packetNumber < 5; packetNumber++) {
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001, packetNumber + 1, 0,
                                          streamID, NO_FLAGS, [&](const std::vector<uint8_t> &subPacket, uint8_t streamID) {

                    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket, 0);
                    if (info != ElasticFrameMessages::noError) {
                        unitTestFailed = true;
                        unitTestActive = false;
                    }


        }
                                            );
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
    }

    if (waitForCompletion()) {
        delete myEFPPacker;
        delete myEFPReciever;
        return false;
    } else {
        delete myEFPPacker;
        delete myEFPReciever;
        return true;
    }
}