//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest8
//Test sending packets, 5 type 1 + 1 type 2.. Send the type2 packet first to the unpacker and send the type1 packets out of order

#include "UnitTest8.h"

void UnitTest8::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info;
    unitTestPacketNumberSender++;
    if (subPacket[0] == 2) {
        unitTestPacketNumberSender = 0;
        info=myEFPReciever->receiveFragment(subPacket,0);
        if (info != ElasticFrameMessages::noError) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
        for (auto &x: unitTestsSavedData2D) {
            if (unitTestPacketNumberSender == 1) {
                unitTestsSavedData = x;
            } else if (unitTestPacketNumberSender == 2) {
                unitTestPacketNumberSender++;
                info=myEFPReciever->receiveFragment(x,0);
                if (info != ElasticFrameMessages::noError) {
                    std::cout << "Error-> " << signed(info) << std::endl;
                    unitTestFailed = true;
                    unitTestActive = false;
                }
                info=myEFPReciever->receiveFragment(unitTestsSavedData,0);
                if (info != ElasticFrameMessages::noError) {
                    std::cout << "Error-> " << signed(info) << std::endl;
                    unitTestFailed = true;
                    unitTestActive = false;
                }
            } else {
                info=myEFPReciever->receiveFragment(x,0);
                if (info != ElasticFrameMessages::noError) {
                    std::cout << "Error-> " << signed(info) << std::endl;
                    unitTestFailed = true;
                    unitTestActive = false;
                }
            }
            unitTestPacketNumberSender++;
        }
    } else {
        unitTestsSavedData2D.push_back(subPacket);
    }

}

void UnitTest8::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (!unitTestActive) return;

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
    if (packet->mFrameSize != (((MTU - myEFPPacker->geType1Size()) * 5) + 12)) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    uint8_t vectorChecker = 0;
    for (int x = 0; x < packet->mFrameSize; x++) {
        if (packet->pFrameData[x] != vectorChecker++) {

            unitTestFailed = true;
            unitTestActive = false;
            break;
        }
    }

    unitTestActive = false;
    std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
}

bool UnitTest8::waitForCompletion() {
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

bool UnitTest8::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest8::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest8::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    unitTestsSavedData.clear();
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