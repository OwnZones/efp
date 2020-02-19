//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_UNITTEST10_H
#define EFP_UNITTEST10_H

#include "../ElasticFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTest10 {
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
    int activeUnitTest = 10;
    std::atomic_int unitTestPacketNumberSender;
    std::atomic_int unitTestPacketNumberReciever;
    std::vector<uint8_t> unitTestsSavedData;
    std::vector<std::vector<uint8_t>> unitTestsSavedData2D;
};

#endif //EFP_UNITTEST10_H
