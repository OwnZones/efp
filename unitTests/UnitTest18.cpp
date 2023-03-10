#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest18
//Using the optional lambda in the pack and send method
TEST(UnitTest18, OptionalLambdaInPackAndSend) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(100,
                                                                                                                 40);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        FAIL() << "Did not expect this callback to be called";
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 1;
    for (int packetNumber = 0; packetNumber < 5; packetNumber++) {
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, 0,
                                                               streamID, NO_FLAGS,
                                                               [&](const std::vector<uint8_t> &subPacket,
                                                                   uint8_t streamID) {
                                                                   ElasticFrameMessages info = myEFPReceiver->receiveFragment(
                                                                           subPacket, 0);
                                                                   EXPECT_EQ(info, ElasticFrameMessages::noError);
                                                               }
        );
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 5;
    }, std::chrono::milliseconds(500)));
}
