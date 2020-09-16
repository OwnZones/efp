//
// Created by Anders Cedronius on 2020-09-15.
//

#ifndef EFP_UNITTEST19_H
#define EFP_UNITTEST19_H

#include "../ElasticFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTest19 {
public:
    bool startUnitTest();
private:
    void sendData(const std::vector<uint8_t> &subPacket, uint8_t streamID, ElasticFrameProtocolContext* pCTX);
    void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet, ElasticFrameProtocolContext* pCTX);
    bool waitForCompletion();
    ElasticFrameProtocolReceiver *myEFPReciever = nullptr;
    ElasticFrameProtocolSender *myEFPPacker = nullptr;
    std::atomic_bool unitTestActive;
    std::atomic_bool unitTestFailed;
    int activeUnitTest = 19;
    std::atomic_int unitTestPacketNumberSender;
    std::atomic_int unitTestPacketNumberReciever;
    std::vector<uint8_t> unitTestsSavedData;
    uint64_t expectedPTS;
};

#endif //EFP_UNITTEST19_H
