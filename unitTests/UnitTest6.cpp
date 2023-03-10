#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest6
//Test sending a packet of MTU*2 + some extra bytes containing a linear vector drop the first packet -> the result should be a packet with a hole of MTU-HeaderType1
//then a linear vector of data starting with the number (MTU-HeaderType1) % 256. also check for broken flag is set.
TEST(UnitTest6, SendLinearVectorAndDropFirstPacket) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 2) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    size_t packetNumber = 0;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        packetNumber++;
        if (packetNumber == 1) {
            return; // Drop the first packet
        }
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mPts, 1001);
        EXPECT_EQ(packet->mCode, 2);
        EXPECT_TRUE(packet->mBroken);

        //One block of MTU should is gone, but the size should still be correct, it's just the data that is gone
        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);

        uint8_t vectorChecker = (MTU - myEFPPacker->getType1Size()) % 256;
        for (size_t x = (MTU - myEFPPacker->getType1Size()); x < packet->mFrameSize; x++) {
            EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
        }
        dataReceived = true;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

    uint8_t streamID = 1;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load();
    }, std::chrono::milliseconds(500)));
}
