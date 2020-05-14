//
// Created by Anders Cedronius on 2019-12-05.
//

//PerformanceLab just send packets / recieve and profile the code

#include "PerformanceLab.h"

void PerformanceLab::sendData(const std::vector<uint8_t> &subPacket) {
    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket, 0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
    }
}

void
PerformanceLab::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (!(++unitTestPacketNumberReciever % 100)) {
        std::cout << "Got packet number " << unsigned(unitTestPacketNumberReciever) << std::endl;
    }
}

bool PerformanceLab::startUnitTest() {
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(5, 2);
    myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&PerformanceLab::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&PerformanceLab::gotData, this, std::placeholders::_1);

    uint64_t packetNumber = 0;
    while (true) {
        mydata.clear();
        size_t randSize = rand() % 1000000 + 1;
        mydata.resize(randSize);
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber, packetNumber, EFP_CODE('A', 'N', 'X', 'B'), streamID, NO_FLAGS);
        packetNumber++;
        if (result != ElasticFrameMessages::noError) {
            std::cout << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
    }
}