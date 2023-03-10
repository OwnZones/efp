#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest8
//Test sending packets, 5 type 1 + 1 type 2.. Send the type2 packet first to the unpacker and send the type1 packets out of order
TEST(UnitTest8, SendLinearVectorReceiverType2FragmentFirst) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<bool> dataReceived = false;

    std::vector<std::vector<uint8_t>> dataKeptBack;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        if (subPacket[0] == 2) {
            // Type 2 fragment => last fragment. Send it to the receiver first and then send the rest of the fragments
            ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);

            // Send the rest of the fragments we have been keeping back, but swap the order of the first two
            std::swap(dataKeptBack[0], dataKeptBack[1]);
            for (auto &x: dataKeptBack) {
                ElasticFrameMessages info = myEFPReceiver->receiveFragment(x, 0);
                EXPECT_EQ(info, ElasticFrameMessages::noError);
            }
        } else {
            // Not the last fragment, keep it back until we send the last fragment
            dataKeptBack.push_back(subPacket);
        }

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
