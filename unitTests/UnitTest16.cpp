#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"

const uint32_t LOSSRATE = 2; // 2% lost frames
const uint32_t BROKEN = 2; // 2% superframes broken
const uint32_t OUTOFORDER = 10; // 10% out of order deliveries
const uint32_t NUMBER_TOTAL_PACKETS = 200; //Number of total packets sent in this unit test

namespace {
    struct TestProps {
        size_t sizeOfData = 0;
        uint64_t pts;
        bool reorder = false;
        bool loss = false;
        bool broken = false;
    };
} // namespace

//UnitTest16
//Random size from 1 to 1000000
//
//This unit test is WIP
TEST(UnitTest16, SendAndRecieveWithLostAndBrokenFramesOutOfOrder) {
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(50,
                                                                                                                 20);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    std::mutex testDataMtx;
    std::vector<TestProps> testData;
    uint64_t brokenCounter = 0;
    std::vector<std::vector<uint8_t>> reorderBuffer;
    unsigned seed = (unsigned) std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine randEng{seed};
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        testDataMtx.lock();
        TestProps currentProps = testData.back();
        testDataMtx.unlock();

        if (currentProps.loss) {
            return;
        }

        if (currentProps.broken) {
            if (!(brokenCounter % 5)) {
                brokenCounter++;
                return;
            } else {
                brokenCounter++;
            }
        }

        if (currentProps.reorder) {
            if ((subPacket[0] & 0x0f) == 1) {
                //type1
                reorderBuffer.emplace_back(subPacket);
                return;
            } else if ((subPacket[0] & 0x0f) == 2) {
                //type2
                reorderBuffer.emplace_back(subPacket);
                std::shuffle(reorderBuffer.begin(), reorderBuffer.end(), randEng);
                for (auto const &x: reorderBuffer) {
                    ElasticFrameMessages info = myEFPReceiver->receiveFragment(x, 0);
                    EXPECT_EQ(info, ElasticFrameMessages::noError);
                }
                //BAM!!
                //
            } else if ((subPacket[0] & 0x0f) == 3) {
                //type3
                reorderBuffer.emplace_back(subPacket);
                return;
            } else {
                FAIL();
            }
            return;
        }

        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    size_t receivedFrameNumber = 0;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        EXPECT_EQ(packet->mStreamID, 1);
        EXPECT_EQ(packet->mCode, EFP_CODE('A', 'N', 'X', 'B'));

        testDataMtx.lock();
        bool isLoss = false;
        do {
            TestProps currentProps = testData[receivedFrameNumber];
            isLoss = currentProps.loss;
            receivedFrameNumber++;
        } while (isLoss);

        testDataMtx.unlock();

        if (!packet->mBroken) {
            uint8_t vectorChecker = 0;
            for (size_t x = 0; x < packet->mFrameSize; x++) {
                EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
            }
        }

        receivedFrameNumber++;
        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(((MTU - myEFPPacker->getType1Size()) * 5) + 12);

    uint8_t streamID = 1;
    size_t sentNotLost = 0;
    for (size_t packetNumber = 0; packetNumber < NUMBER_TOTAL_PACKETS; packetNumber++) {
        mydata.clear();
        size_t randSize = rand() % 1000000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        TestProps myTestProps;
        myTestProps.sizeOfData = mydata.size();

        size_t loss = rand() % 100 + 1;
        if (loss <= LOSSRATE) {
            myTestProps.loss = true;
        } else {
            sentNotLost++;
        }

        size_t broken = rand() % 100 + 1;
        if (broken <= BROKEN) {
            myTestProps.broken = true;
        }

        size_t ooo = rand() % 100 + 1;
        if (ooo <= OUTOFORDER) {
            myTestProps.reorder = true;
        }

        myTestProps.pts = packetNumber;

        testDataMtx.lock();
        testData.emplace_back(myTestProps);
        testDataMtx.unlock();

        brokenCounter = 0;
        reorderBuffer.clear();

        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber + 1, EFP_CODE('A', 'N', 'X', 'B'),
                                                               streamID, NO_FLAGS);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == sentNotLost;
    }, std::chrono::seconds(30)));
}
