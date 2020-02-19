//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_UNITTEST7_H
#define EFP_UNITTEST7_H

#include "../ElasticFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTest7 {
public:
    bool startUnitTest();
private:
    void sendData(const std::vector<uint8_t> &subPacket, uint8_t streamID);
    void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet);
    bool waitForCompletion();
    ElasticFrameProtocolReceiver *myEFPReciever = nullptr;
    ElasticFrameProtocolSender *myEFPPacker = nullptr;
    std::atomic_bool unitTestActive;
    std::atomic_bool unitTestFailed;
    int activeUnitTest = 7;
    std::atomic_int unitTestPacketNumberSender;
    std::vector<uint8_t> unitTestsSavedData;
};

#endif //EFP_UNITTEST6_H
