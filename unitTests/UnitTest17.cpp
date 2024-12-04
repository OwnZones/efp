#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


namespace {
    struct PrivateData {
        int myPrivateInteger = 10;
        uint8_t myPrivateUint8_t = 44;
        size_t sizeOfData = 0;
    };
} // namespace

//UnitTest17
//Send 100 frames... Stop the sender. Delete the sender. delete the receiver
//start a new sender and force the superframe counter to != 0 start the receiver
TEST(UnitTest17, StopAndRestartSend) {
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
        EXPECT_FALSE(packet->mBroken);
        EXPECT_EQ(packet->mPts, 1000 + receivedFrameNumber);
        EXPECT_EQ(packet->mStreamID, 1);
        dataReceived++;
    };

    uint8_t streamID = 1;
    for (uint64_t packetNumber = 0; packetNumber < 100; packetNumber++) {
        std::vector<uint8_t> mydata;
        size_t randSize = rand() % 10000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber, EFP_CODE('A', 'N', 'X', 'B'), streamID,
                                                               NO_FLAGS);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    myEFPReceiver.reset();
    myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50, 20);
    myEFPReceiver->receiveCallback = myEFPReceiver->receiveCallback = [&](
            ElasticFrameProtocolReceiver::pFramePtr &packet,
            ElasticFrameProtocolContext *) {
        receivedFrameNumber++;
        EXPECT_FALSE(packet->mBroken);
        // We already received 100 frames
        EXPECT_EQ(packet->mPts, 1000 - 100 + receivedFrameNumber);
        EXPECT_EQ(packet->mStreamID, 2);
        dataReceived++;
    };

    myEFPPacker.reset();
    myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    myEFPPacker->sendCallback =
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };
    myEFPPacker->setSuperFrameNo(4567);

    streamID = 2;
    for (uint64_t packetNumber = 0; packetNumber < 100; packetNumber++) {
        streamID = 2;
        std::vector<uint8_t> mydata;
        size_t randSize = rand() % 10000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber,
                                                               EFP_CODE('A', 'N', 'X', 'B'), streamID, NO_FLAGS);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 200;
    }, std::chrono::milliseconds(500)));
}
