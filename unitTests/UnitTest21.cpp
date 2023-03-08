//
// Created by Anders Cedronius on 2020-04-03.
//

//UnitTest21

#include "UnitTest21.h"

//UnitTest21
//Zero copy test


void UnitTest21::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
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


bool UnitTest21::waitForCompletion() {
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

bool UnitTest21::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(100, 40);
    myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }

    myEFPReciever->receiveCallback = std::bind(&UnitTest21::gotData, this, std::placeholders::_1);

    mydata.clear();
    expectedPTS = 1000;
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12 + 100);
    unitTestActive = true;

    for (int packetNumber = 0; packetNumber < 5; packetNumber++) {

        //destructivePackAndSendFromPtr(const uint8_t *pPacket, size_t lPacketSize, ElasticFrameContent lDataContent, uint64_t lPts,
        //        uint64_t lDts, uint32_t lCode, uint8_t lStreamID, uint8_t lFlags, lambda);

        result = myEFPPacker->destructivePackAndSendFromPtr(mydata.data()+100, mydata.size()-100, ElasticFrameContent::h264, packetNumber + 1001, packetNumber + 1, 0,
                                                            streamID, NO_FLAGS, [&](const uint8_t* lData, size_t lSize) {
                    ElasticFrameMessages info = myEFPReciever->receiveFragmentFromPtr(lData, lSize, 0);
                    if (info != ElasticFrameMessages::noError) {
                        unitTestFailed = true;
                        unitTestActive = false;
                    }
                    //std::cout << "Fragments. " << lSize << std::endl;
        });


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