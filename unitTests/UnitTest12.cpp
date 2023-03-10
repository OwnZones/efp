#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest12
//Test sending 5 packets, with 5 frame type 1 + 1 frame type 2
//Reverse both the packets to the unpacker and the order of the fragments within each packet 5. Also drop the middle packet (packet 3)
//This is testing the out of order head of line blocking mechanism
//The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
TEST(UnitTest12, SendFivePacketsDeliverInReversedOrderWithReversedFragmentOrder) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(100,
                                                                                                                 40);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    size_t sentSuperFrameNumber = 0;
    std::vector<std::vector<uint8_t>> keptBackFragments;
    std::vector<std::vector<std::vector<uint8_t>>> keptBackSuperFrames;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        if (subPacket[0] == 2) {
            sentSuperFrameNumber++;
            keptBackFragments.push_back(subPacket);
            keptBackSuperFrames.push_back(keptBackFragments);
            if (sentSuperFrameNumber == 5) {
                for (size_t item = keptBackSuperFrames.size(); item > 0; item--) {
                    if (item == 3) {
                        continue; // Drop packet 3
                    }
                    std::vector<std::vector<uint8_t>> superFrame = keptBackSuperFrames[item - 1];
                    for (size_t fragment = superFrame.size(); fragment > 0; fragment--) {
                        ElasticFrameMessages info = myEFPReceiver->receiveFragment(superFrame[fragment - 1], 0);
                        EXPECT_EQ(info, ElasticFrameMessages::noError);
                    }
                }
            }
            keptBackFragments.clear();
            return;
        }
        keptBackFragments.push_back(subPacket);
    };

    size_t receivedFrameNumber = 0;
    int64_t nextExpectedPts = 1001;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        receivedFrameNumber++;
        EXPECT_EQ(packet->mPts, nextExpectedPts);
        // Expect pts 1003 to be missing
        nextExpectedPts += (nextExpectedPts == 1002 ? 2 : 1);

        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);
        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 5; packetNumber++) {
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, 0,
                                                               streamID, NO_FLAGS);
        ASSERT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 4;
    }, std::chrono::milliseconds(500)));
}
