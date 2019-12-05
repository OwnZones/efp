//
// Created by Anders Cedronius on 2019-12-05.
//

#ifndef EFP_UNITTESTTEMPLATE_H
#define EFP_UNITTESTTEMPLATE_H

#include "../EdgewareFrameProtocol.h"

#define MTU 1456 //SRT-max

class UnitTestTemplate {
public:
    bool startUnitTest();
private:
    void sendData(const std::vector<uint8_t> &subPacket);
    void gotData(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags);
    bool waitForCompletion();
    EdgewareFrameProtocol *myEFPReciever = nullptr;
    EdgewareFrameProtocol *myEFPPacker = nullptr;
    std::atomic_bool unitTestActive;
    std::atomic_bool unitTestFailed;
    int activeUnitTest = 1;
};


#endif //EFP_UNITTESTTEMPLATE_H
