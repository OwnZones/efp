#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest5
//Test sending a packet of MTU*5+MTU/2 containing a linear vector -> the result should be a packet with that size containing a linear vector.
TEST(UnitTest5, SendLinearVectorOverMultipleFragments) {
    const size_t FRAME_SIZE = (MTU * 5) + (MTU / 2);
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mPts, 1001);
        EXPECT_EQ(packet->mCode, 2);
        EXPECT_FALSE(packet->mBroken);
        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);

        uint8_t vectorChecker = 0;
        for (size_t x = 0; x < packet->mFrameSize; x++) {
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
