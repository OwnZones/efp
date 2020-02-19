//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_PERFORMANCELAB_H
#define EFP_PERFORMANCELAB_H

#include "../ElasticFrameProtocol.h"
#include <random>

#define MTU 1456 //SRT-max

class PerformanceLab {
public:
    bool startUnitTest();
private:
    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet);
    ElasticFrameProtocolReceiver *myEFPReciever = nullptr;
    ElasticFrameProtocolSender *myEFPPacker = nullptr;
    int unitTestPacketNumberReciever = 0;
};

#endif //EFP_PERFORMANCELAB_H
