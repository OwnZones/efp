//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_UNITTEST17_H
#define EFP_UNITTEST17_H

#include "../ElasticFrameProtocol.h"
#include <random>

#define MTU 1456 //SRT-max

class UnitTest17 {
public:
    bool startUnitTest();
private:

    std::mutex debugPrintMutex;

    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(ElasticFrameProtocolReceiver::pFramePtr &packet);
    bool waitForCompletion();
    ElasticFrameProtocolReceiver *myEFPReciever = nullptr;
    ElasticFrameProtocolSender *myEFPPacker = nullptr;
    std::atomic_bool unitTestActive;
    std::atomic_bool unitTestFailed;
    int activeUnitTest = 17;

    uint64_t unitTestPacketNumberReciever = 0;
    uint64_t expectedPTS;
    uint64_t brokenCounter = 0;
    std::vector<std::vector<uint8_t>> reorderBuffer;
    std::default_random_engine randEng;
    struct TestProps {
        size_t sizeOfData = 0;
        uint64_t pts;
        bool reorder = false;
        bool loss = false;
        bool broken = false;
    };
    uint64_t counter293=0;

    std::mutex testDataMtx;
    std::vector<TestProps>testData;
};

#endif //EFP_UNITTEST17_H
