#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"

#define MTU 1456 //SRT-max

//UnitTest21
//Zero copy test
TEST(UnitTest21, ZeroCopySend) {
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(100,
                                                                                                                 40);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    int64_t nextExpectedPts = 1001;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mPts, nextExpectedPts);
        nextExpectedPts++;
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, 0);
        EXPECT_FALSE(packet->mBroken);

        EXPECT_EQ(packet->mFrameSize, ((MTU - myEFPPacker->getType1Size()) * 5) + 12);

        uint8_t vectorChecker = 0;
        for (size_t x = 0; x < packet->mFrameSize; x++) {
            EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
        }

        dataReceived++;
    };

    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 5; packetNumber++) {
        std::vector<uint8_t> mydata;
        mydata.resize(((MTU - myEFPPacker->getType1Size()) * 5) + 12 + 100);
        std::generate(mydata.begin() + 100, mydata.end(), [n = 0]() mutable { return n++; });

        auto sendCallback = [&](const uint8_t *lData, size_t lSize) {
            ElasticFrameMessages info = myEFPReceiver->receiveFragmentFromPtr(lData, lSize, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);
        };
        ElasticFrameMessages result = myEFPPacker->destructivePackAndSendFromPtr(mydata.data() + 100,
                                                                                 mydata.size() - 100,
                                                                                 ElasticFrameContent::h264,
                                                                                 packetNumber + 1001, packetNumber + 1,
                                                                                 0,
                                                                                 streamID, NO_FLAGS,
                                                                                 sendCallback);


        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 5;
    }, std::chrono::milliseconds(500)));
}
