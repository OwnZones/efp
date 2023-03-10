#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest4
//Test sending a packet of MTU-HeaderType1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the receiver
TEST(UnitTest4, SendPacketFrameType1AndFrameType2) {
    const size_t FRAME_SIZE = (MTU - ElasticFrameProtocolSender::getType1Size()) + 1;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    size_t packetNumber = 0;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        if (packetNumber == 0) {
            // Should be of type 1
            EXPECT_EQ(subPacket[0], 1);
            EXPECT_EQ(subPacket.size(), MTU);
        } else if (packetNumber == 1) {
            // Should be of type 2
            EXPECT_EQ(subPacket[0], 2);
            EXPECT_EQ(subPacket.size(), (myEFPPacker->getType2Size() + 1));
        }
        EXPECT_LT(packetNumber, 2);
        packetNumber++;
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 4);
        EXPECT_EQ(packet->mPts, 1001);
        EXPECT_EQ(packet->mCode, 2);
        EXPECT_FALSE(packet->mBroken);
        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);
        dataReceived = true;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 4;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load();
    }, std::chrono::milliseconds(500)));
}
