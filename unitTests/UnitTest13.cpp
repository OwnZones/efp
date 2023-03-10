#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//FIXME-- Test is currently only sending the packets. lgtm [cpp/fixme-comment]
//UnitTest13
//Test sending 100 000 superframes of size from 500 to 10.000 bytes
//Reverse the packets to the unpacker and drop the middle packet (packet 3) also deliver the fragments reversed meaning packet 5 last fragment first..
//This is testing the out of order head of line blocking mechanism
//The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
TEST(UnitTest13, SendSuperFramesOfRandomSize) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    size_t receivedFrameNumber = 0;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        receivedFrameNumber++;
        EXPECT_EQ(packet->mPts, receivedFrameNumber + 1000);
        EXPECT_EQ(packet->mDts, receivedFrameNumber);

        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);

        EXPECT_LE(receivedFrameNumber, 100000);

        dataReceived++;
    };

    std::vector<uint8_t> mydata;

    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 100000; packetNumber++) {
        mydata.clear();
        mydata.resize(FRAME_SIZE);
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, 0, streamID, NO_FLAGS);
        ASSERT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 100000;
    }, std::chrono::milliseconds(500)));
}
