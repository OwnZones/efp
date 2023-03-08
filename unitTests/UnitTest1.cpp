#include <gtest/gtest.h>

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


//UnitTest1
//Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent
TEST(UnitTest1, SmallPacketShouldResultInType2Frame) {
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);

    myEFPPacker->sendCallback = [](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                   ElasticFrameProtocolContext *pCTX) {
        EXPECT_EQ(subPacket[0] & 0x0f, 2);
    };

    std::vector<uint8_t> mydata;
    mydata.resize(MTU - myEFPPacker->getType2Size());

    uint8_t streamID = 1;
    ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::adts, 1001, 1, 2, streamID,
                                                           NO_FLAGS);
    ASSERT_EQ(result, ElasticFrameMessages::noError);
}
