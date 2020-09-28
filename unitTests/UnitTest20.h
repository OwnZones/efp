//
// Created by Anders Cedronius on 2020-09-26.
//

#ifndef EFP_UNITTEST20_H
#define EFP_UNITTEST20_H

#include "../ElasticFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTest20 {
public:
    bool startUnitTest();
private:
    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet);
    bool waitForCompletion();
    ElasticFrameProtocolReceiver *myEFPReciever = nullptr;
    ElasticFrameProtocolSender *myEFPPacker = nullptr;
    std::atomic_bool unitTestActive;
    std::atomic_bool unitTestFailed;
    int activeUnitTest = 20;
    std::atomic_int unitTestPacketNumberSender;
};

#endif //EFP_UNITTEST20_H
