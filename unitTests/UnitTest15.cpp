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

//UnitTest15
//This is the crazy-monkey test1. We randomize the size for 1000 packets. We store the size in a private struct and embed it.
//when we receive the packet we check the size saved in the embedded data and also the linear vector in the payload.
TEST(UnitTest15, SendPrivateEmbeddedDataWithSizeOfPayload) {
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(100,
                                                                                                                 40);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    std::vector<std::vector<uint8_t>> keptBackFragments;
    std::vector<std::vector<std::vector<uint8_t>>> keptBackSuperFrames;
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
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, EFP_CODE('A', 'N', 'X', 'B'));

        EXPECT_EQ(packet->mPts, receivedFrameNumber + 1000);
        EXPECT_TRUE(packet->mFlags & INLINE_PAYLOAD);

        ElasticFrameMessages info;
        std::vector<std::vector<uint8_t>> embeddedData;
        std::vector<uint8_t> embeddedContentFlag;
        size_t payloadDataPosition = 0;
        info = myEFPReceiver->extractEmbeddedData(packet, &embeddedData, &embeddedContentFlag,
                                                  &payloadDataPosition);
        EXPECT_EQ(info, ElasticFrameMessages::noError);

        for (size_t x = 0; x < embeddedData.size(); x++) {
            EXPECT_EQ(embeddedContentFlag[x], ElasticEmbeddedFrameContent::embeddedprivatedata);
            std::vector<uint8_t> thisVector = embeddedData[x];
            PrivateData myPrivateData = *reinterpret_cast<PrivateData *>(thisVector.data());
            EXPECT_EQ(myPrivateData.sizeOfData, packet->mFrameSize);
            EXPECT_EQ(myPrivateData.myPrivateInteger, 10);
            EXPECT_EQ(myPrivateData.myPrivateUint8_t, 44);

        }

        uint8_t vectorChecker = 0;
        for (size_t x = payloadDataPosition; x < packet->mFrameSize; x++) {
            EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
        }

        EXPECT_LE(receivedFrameNumber, 1000);
        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 1000; packetNumber++) {
        size_t randSize = rand() % 1000000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        PrivateData myPrivateData;
        myPrivateData.sizeOfData = mydata.size() + sizeof(PrivateData) + 4; //4 is the embedded frame header size
        myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData),
                                     ElasticEmbeddedFrameContent::embeddedprivatedata, true);
        ASSERT_EQ(myPrivateData.sizeOfData, mydata.size());

        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, EFP_CODE('A', 'N', 'X', 'B'), streamID,
                                                               INLINE_PAYLOAD);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 1000;
    }, std::chrono::seconds(30)));
}
