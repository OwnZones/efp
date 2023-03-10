#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


// UnitTest23
// Test sending 10 packets, each being split into six EFP fragments, five of type 1 and one of type 2
// Drop packet 4 and 5, and deliver the rest in reversed order
// Check that we can see that we lost two packets/superframes in the receiver when we receive the last packet
TEST(UnitTest23, SendFivePacketsDeliverInReversedOrderWithReversedFragmentOrder) {
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
            if (sentSuperFrameNumber == 10) {
                for (size_t item = keptBackSuperFrames.size(); item > 0; item--) {
                    if (item == 4 || item == 5) {
                        continue; // Drop packet 4 and 5
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
    uint16_t lastReceivedSuperFrame = 0;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        receivedFrameNumber++;
        EXPECT_EQ(packet->mPts, nextExpectedPts);
        // Expect pts 1004 and 1005 to be missing
        nextExpectedPts += (nextExpectedPts == 1003 ? 3 : 1);

        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        EXPECT_EQ(packet->mFrameSize, FRAME_SIZE);

        if (packet->mPts == 1006) {
            // Check that we know we lost 2 super frames here
            EXPECT_EQ(lastReceivedSuperFrame + 3, packet->mSuperFrameNo);
        } else if (packet->mPts != 1001) {
            EXPECT_EQ(lastReceivedSuperFrame + 1, packet->mSuperFrameNo);
        }
        lastReceivedSuperFrame = packet->mSuperFrameNo;

        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);

    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 10; packetNumber++) {
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, 0,
                                                               streamID, NO_FLAGS);
        ASSERT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 8;
    }, std::chrono::milliseconds(500)));
}
