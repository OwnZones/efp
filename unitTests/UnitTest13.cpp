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
    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket,0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest13::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (packet->mBroken) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    unitTestPacketNumberReciever++;

    if (unitTestPacketNumberReciever+1000 != packet->mPts) {
        std::cout << "Got PTS -> " << unsigned(packet->mPts) << " Expected -> " << unsigned(unitTestPacketNumberReciever) << std::endl;
        unitTestPacketNumberReciever = packet->mPts; //if you want to continue remove the lines under to the return;
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if ((unitTestPacketNumberReciever) != packet->mDts) {
        std::cout << "Got DTS -> " << unsigned(packet->mPts) << " Expected -> " << unsigned(unitTestPacketNumberReciever) << std::endl;
        unitTestPacketNumberReciever = packet->mPts; //if you want to continue remove the lines under to the return;
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    if (packet->mFrameSize != (((MTU - myEFPPacker->geType1Size()) * 5) + 12)) {
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

bool UnitTest13::startUnitTest() {
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
    myEFPPacker->sendCallback = std::bind(&UnitTest13::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest13::gotData, this, std::placeholders::_1);
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 1000;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;

    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 100000; packetNumber++) {

        mydata.clear();
        mydata.resize(((MTU - myEFPPacker->geType1Size()) * 5) + 12);
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber+1001, packetNumber+1, 0, streamID, INLINE_PAYLOAD);
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
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