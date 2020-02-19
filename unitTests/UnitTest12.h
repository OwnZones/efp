//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_UNITTEST12_H
#define EFP_UNITTEST12_H

#include "../ElasticFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTest12 {
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
    int activeUnitTest = 12;
    std::atomic_int unitTestPacketNumberSender;
    std::atomic_int unitTestPacketNumberReciever;
    std::vector<uint8_t> unitTestsSavedData;
    std::vector<std::vector<uint8_t>> unitTestsSavedData2D;
    std::vector<std::vector<std::vector<uint8_t>>> unitTestsSavedData3D;
    uint64_t expectedPTS;
};

#endif //EFP_UNITTEST12_H
