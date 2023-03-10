#include <gtest/gtest.h>

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

//UnitTest14
//Send 15 packets with embeddedPrivateData. Odd packet numbers will have two embedded private data fields. Also check for not broken and correct FourCC code.
//The reminder of the packet is a vector. Check it's integrity
TEST(UnitTest14, SendPrivateEmbeddedData) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
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

        EXPECT_TRUE(packet->mFlags & INLINE_PAYLOAD);
        std::vector<std::vector<uint8_t>> embeddedData;
        std::vector<uint8_t> embeddedContentFlag;
        size_t payloadDataPosition = 0;
        ElasticFrameMessages info = myEFPReceiver->extractEmbeddedData(packet, &embeddedData, &embeddedContentFlag,
                                                                       &payloadDataPosition);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
        EXPECT_EQ(embeddedData.size(), embeddedContentFlag.size());
        if (receivedFrameNumber & 1) {
            EXPECT_EQ(embeddedData.size(), 2);
        }

        for (size_t x = 0; x < embeddedData.size(); x++) {
            EXPECT_EQ(embeddedContentFlag[x], ElasticEmbeddedFrameContent::embeddedprivatedata);
            std::vector<uint8_t> thisVector = embeddedData[x];
            PrivateData myPrivateData = *reinterpret_cast<PrivateData *>(thisVector.data());
            EXPECT_EQ(myPrivateData.myPrivateInteger, 10);
            EXPECT_EQ(myPrivateData.myPrivateUint8_t, 44);
        }

        uint8_t vectorChecker = 0;
        for (size_t x = payloadDataPosition; x < packet->mFrameSize; x++) {
            EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
        }
        EXPECT_LE(receivedFrameNumber, 15);
        dataReceived++;
    };


    uint8_t streamID = 1;
    for (size_t packetNumber = 0; packetNumber < 15; packetNumber++) {
        std::vector<uint8_t> mydata;
        mydata.resize(FRAME_SIZE);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        PrivateData myPrivateData;
        PrivateData myPrivateDataExtra;
        if (!(packetNumber & 1)) {
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateDataExtra, sizeof(myPrivateDataExtra),
                                         ElasticEmbeddedFrameContent::embeddedprivatedata, true);
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData),
                                         ElasticEmbeddedFrameContent::embeddedprivatedata, false);
        } else {
            myEFPPacker->addEmbeddedData(&mydata, &myPrivateData, sizeof(PrivateData),
                                         ElasticEmbeddedFrameContent::embeddedprivatedata, true);
        }


        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1,
                                                               EFP_CODE('A', 'N', 'X', 'B'), streamID, INLINE_PAYLOAD);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 15;
    }, std::chrono::milliseconds(500)));
}
