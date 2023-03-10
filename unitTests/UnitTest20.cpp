#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest20
//Test basic run to completion
//Test sending a packet of MTU-headertype1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the receiver
TEST(UnitTest20, RunToCompletion) {
    const size_t FRAME_SIZE = (MTU - ElasticFrameProtocolSender::getType1Size()) + 1;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50, 20,
                                                                                                                 nullptr,
                                                                                                                 ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    size_t sentPacketNumber = 0;
    std::vector<std::vector<uint8_t>> keptBackFragments;
    std::vector<std::vector<std::vector<uint8_t>>> keptBackSuperFrames;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        if (sentPacketNumber == 0) {
            EXPECT_EQ(subPacket[0], 1);
            EXPECT_EQ(subPacket.size(), MTU);
        }
        if (sentPacketNumber == 1) {
            EXPECT_EQ(subPacket[0], 2);
            EXPECT_EQ(subPacket.size(), (myEFPPacker->getType2Size() + 1));
        } else if (sentPacketNumber > 1) {
            FAIL();
        }

        sentPacketNumber++;
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
        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 4;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);
    EXPECT_EQ(dataReceived.load(), 1);
}
