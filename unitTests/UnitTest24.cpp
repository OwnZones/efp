#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"

// UnitTest24
// Test receiving corrupted packages
TEST(UnitTest24, SendPacketFrameType1AndFrameType2) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned int> dis(0, 255);

  std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver =
      std::make_unique<ElasticFrameProtocolReceiver>(50, 20);
  std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker =
      std::make_unique<ElasticFrameProtocolSender>(MTU);

  for (int i = 0; i < 10000; i++) {
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket,
                                    uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {
      std::vector<uint8_t> garbage = subPacket;

      std::generate(garbage.begin(), garbage.end(),
                    [&]() { return static_cast<uint8_t>(dis(gen)); });

      myEFPReceiver->receiveFragment(garbage, 0);
    };

    std::vector<uint8_t> mydata;
    size_t randSize = rand() % 10000 + 1;
    mydata.resize(randSize);

    uint8_t streamID = 4;
    ElasticFrameMessages result = myEFPPacker->packAndSend(
        mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID, NO_FLAGS);
    EXPECT_EQ(result, ElasticFrameMessages::noError);
  }
}
