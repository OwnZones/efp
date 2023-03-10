#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"

//UnitTest22
//
//Send 4 frames
//Each frame is split into 6 fragments
//Deliver frame 1 as is
//Deliver frame 2 with two missing fragments (fragment 2 and fragment 5) drop fragment 2 and save fragment 5 for later
//Deliver frame 3 as is
//wait for 60ms (passing the time out) and then send the saved fragment 5 from frame 2
//Deliver frame 4 as is
//Send fragment 5 from frame 2 again
//
//Expected result is that all frames should be delivered in order (EFP is set to HOL-mode) but mark frame 2 as broken
//When passing fragment 5 again (meaning sending late fragments) EFP should respond with too old.
TEST(UnitTest22, HeadOfLineBlocking) {
    const size_t FRAME_SIZE = ((MTU - ElasticFrameProtocolSender::getType1Size()) * 5) + 12;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(20, 20,
                                                                                                                 nullptr,
                                                                                                                 ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    std::atomic<size_t> dataReceived = 0;

    size_t sentFragmentNumber = 0;
    std::vector<uint8_t> savedFragment;
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
        sentFragmentNumber++;

        if (sentFragmentNumber == 8) {
            // Skip fragment 8 (second fragment in second super frame)
            return;
        }

        if (sentFragmentNumber == 11) {
            // Save fragment 11 for later
            savedFragment = subPacket;
            return;
        }

        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);

        if (sentFragmentNumber == 18) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // Send saved fragment
            info = myEFPReceiver->receiveFragment(savedFragment, 0);
            EXPECT_EQ(info, ElasticFrameMessages::noError);
            return;
        }

        if (sentFragmentNumber == 24) {
            // Send saved old fragment again
            info = myEFPReceiver->receiveFragment(savedFragment, 0);
            //The fragment is too old, so it should be signaled as such
            EXPECT_EQ(info, ElasticFrameMessages::tooOldFragment);
            return;
        }
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *) {
        if (packet->mPts == 1002) {
            EXPECT_TRUE(packet->mBroken);
        } else {
            EXPECT_FALSE(packet->mBroken);

            uint8_t vectorChecker = 0;
            for (size_t x = 0; x < FRAME_SIZE; x++) {
                EXPECT_EQ(packet->pFrameData[x], vectorChecker++);
            }
        }

        dataReceived++;
    };

    std::vector<uint8_t> mydata;
    mydata.resize(FRAME_SIZE);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

    uint8_t streamID = 1;
    uint64_t ptsValue = 1001;
    for (int x = 0; x < 4; x++) {
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, ptsValue++, 1, 2,
                                                               streamID, NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_EQ(dataReceived.load(), 4);
}