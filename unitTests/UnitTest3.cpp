#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest3
//Test sending 1 byte packet of value 0xaa and receiving it at the EFP receiver.
TEST(UnitTest3, SendOneBytePacket) {
    const size_t FRAME_SIZE = 1;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        ElasticFrameMessages status = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(status, ElasticFrameMessages::noError);
    };
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mPts, 1001);
        EXPECT_EQ(packet->mCode, 2);
        EXPECT_FALSE(packet->mBroken);
        EXPECT_EQ(packet->mDataContent, ElasticFrameContentNamespace::adts);
        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);
        EXPECT_EQ(packet->pFrameData[0], 0xaa);
        dataReceived = true;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);
    mydata[0] = 0xaa;

    uint8_t streamID = 1;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load();
    }, std::chrono::milliseconds(500)));
}
