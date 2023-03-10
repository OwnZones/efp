#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest7
//Test sending packets, 5 type 1 + 1 type 2.. Reorder type1 packet 3 and 2 so the delivery order is 1 3 2 4 5 6
//then check for correct length and correct vector in the payload
TEST(UnitTest7, SendLinearVectorAndSwapFragmentOrder) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    size_t packetNumber = 0;
    std::vector<uint8_t> savedSubPacketNumber2;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        EXPECT_EQ(lStreamID, 8);
        packetNumber++;
        if (packetNumber == 2) {
            // Hold packet number 2
            savedSubPacketNumber2 = subPacket;
            return;
        } else if (packetNumber == 3) {
            //First send packet number 3, then packet number 2
            ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);

            info = myEFPReceiver->receiveFragment(savedSubPacketNumber2, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);
            return;
        }

        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 8);
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

    uint8_t streamID = 8;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load();
    }, std::chrono::milliseconds(500)));
}
