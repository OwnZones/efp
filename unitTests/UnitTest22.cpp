//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest22

//Send 4 frames
//Each frame is split into 6 fragments
//Deliver frame 1 as is
//Deliver frame 2 with two missing fragments (fragment 2 and fragment 5) drop fragment 2 and save fragment 5 for later
//Deliver frame 3 as is
//wait for 60ms (passing the time out) and then send the saved fragment 5 from frame 2
//Deliver frame 4 as is
//Send fragment 5 from frame 2 again

//Expected result is that all frames should be delivered in order (EFP is set to HOL-mode) but mark frame 2 as broken
//When passing fragment 5 again (meaning sending late fragments) EFP should respond with too old.

#include "UnitTest22.h"

void UnitTest22::sendData(const std::vector<uint8_t> &subPacket) {
    unitTestFragmentnumber++;

    //std::cout << "Fragment " << unitTestFragmentnumber << std::endl;
    if (unitTestFragmentnumber == 8) {
        //std::cout << "Skip fragment 8 (second fragment in second super frame)" << std::endl;
        return;
    }

    if (unitTestFragmentnumber == 11) {
        //std::cout << "Save fragment 11 for later" << std::endl;
        savedFragment = subPacket;
        return;
    }

    ElasticFrameMessages info;

    info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }

    if (unitTestFragmentnumber == 18) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        //std::cout << "Send saved fragment" << std::endl;
        info = myEFPReciever->receiveFragment(savedFragment,0);
        if (info != ElasticFrameMessages::noError) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
        return;
    }

    if (unitTestFragmentnumber == 24) {
        //std::cout << "Send saved old fragment" << std::endl;
        info = myEFPReciever->receiveFragment(savedFragment,0);
        //The fragment is old so it should be signaled as such
        if (info != ElasticFrameMessages::tooOldFragment) {
            std::cout << "Error-> " << signed(info) << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        } else {
            unitTestActive = false;
            std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        }
        return;
    }
}

void UnitTest22::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    //std::cout << "Got data " << packet->mPts << " is broken " << packet->mBroken << std::endl;
    if (packet->mPts == 1002) {
        if (!packet->mBroken) {
            unitTestFailed = true;
            unitTestActive = false;
        }
        return;
    }

    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
    }
}

bool UnitTest22::waitForCompletion() {
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

bool UnitTest22::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    unitTestFragmentnumber = 0;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID=1;
    myEFPReciever = new (std::nothrow) ElasticFrameProtocolReceiver(20, 20, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION);
    myEFPPacker = new (std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest22::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest22::gotData, this, std::placeholders::_1);
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    uint64_t ptsValue = 1001;
    for (int x = 0; x < 4; x++) {
        //std::cout << "send " << ptsValue << std::endl;
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, ptsValue++, 1, 2, streamID, NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(20));
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