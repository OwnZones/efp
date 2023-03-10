#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest10
//send two type 2 packets out of order and receive them in order.
TEST(UnitTest10, SendType2FragmentsAndSwapOrder) {
    const size_t FRAME_SIZE = (MTU - ElasticFrameProtocolSender::getType2Size());
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    size_t packetNumber = 0;
    std::vector<uint8_t> fragmentKeptBack;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        packetNumber++;
        if (packetNumber == 1) {
            fragmentKeptBack = subPacket;
        } else if (packetNumber == 2) {
            ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);

            info = myEFPReceiver->receiveFragment(fragmentKeptBack, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);
        }

        EXPECT_LE(packetNumber, 2);
    };

    size_t receivedFrameNumber = 0;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        receivedFrameNumber++;
        if (receivedFrameNumber == 1) {
            EXPECT_EQ(packet->mPts, 1001);
            dataReceived++;
        }
        if (receivedFrameNumber == 2) {
            EXPECT_EQ(packet->mPts, 1002);
            dataReceived++;
        }

        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 1;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, 1001, 1, 0, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, 1002, 2, 0, streamID, NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 2;
    }, std::chrono::milliseconds(500)));
}
