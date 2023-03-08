#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest11
//Test sending 5 packets, with 5 frame type 1 + 1 frame type 2
//Reverse the packets to the unpacker and drop the middle packet (packet 3)
//This is testing the out of order head of line blocking mechanism
//The result should be: deliver packet 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
TEST(UnitTest11, SendFivePacketsDeliverInReversedOrder) {
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
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
                for (size_t superFrame = keptBackSuperFrames.size(); superFrame > 0; superFrame--) {
                    if (superFrame == 3) {
                        continue; // Drop packet number 3
                    }

                    for (auto &x: keptBackSuperFrames[superFrame - 1]) {
                        ElasticFrameMessages info = myEFPReceiver->receiveFragment(x, 0);
                        EXPECT_EQ(info, ElasticFrameMessages::noError);
                    }
                }
            }
            keptBackFragments.clear();
        } else {
            keptBackFragments.push_back(subPacket);
        }
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

        EXPECT_EQ(packet->mFrameSize, ((MTU - myEFPPacker->getType1Size()) * 5) + 12);
        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(((MTU - myEFPPacker->getType1Size()) * 5) + 12);

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
