#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"


namespace {

    class SomeContextIWantInMySender {
    public:
        int mSomeValue = 3;
        std::string mSomeString = "Some String";
    };

    class SomeContextIWantInMyReceiver {
    public:
        int mSomeOtherValue = 7;
        std::string mSomeOtherString = "Some Other String";
    };
} // namespace

//UnitTest19
//Testing the optional context that can be used in the callbacks
TEST(UnitTest19, OptionalContextInCallbacks) {
    // ---------------------------------------------------------------------------
    //Sender Context part ----------------
    // ---------------------------------------------------------------------------

    //The std::any part
    auto mySenderCTX = std::make_shared<ElasticFrameProtocolContext>();
    mySenderCTX->mObject = std::make_shared<SomeContextIWantInMySender>();

    //The void* part
    auto getBackMySndObject = std::any_cast<std::shared_ptr<SomeContextIWantInMySender> &>(mySenderCTX->mObject);
    mySenderCTX->mUnsafePointer = getBackMySndObject.get();

    //The value part
    mySenderCTX->mValue = 100;

    // ---------------------------------------------------------------------------
    // Receiver Context part ----------------
    // ---------------------------------------------------------------------------

    //The std::any part
    auto myReceiverCTX = std::make_shared<ElasticFrameProtocolContext>();
    myReceiverCTX->mObject = std::make_shared<SomeContextIWantInMyReceiver>();

    //The void* part
    auto getBackMyRcvObject = std::any_cast<std::shared_ptr<SomeContextIWantInMyReceiver> &>(myReceiverCTX->mObject);
    myReceiverCTX->mUnsafePointer = getBackMyRcvObject.get();

    //The value part
    myReceiverCTX->mValue = 200;


    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(100,
                                                                                                                 40,
                                                                                                                 myReceiverCTX);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU,
                                                                                                           mySenderCTX);
    std::atomic<size_t> dataReceived = 0;

    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t lStreamID,
                                    ElasticFrameProtocolContext *pCTX) {

        //Did we even get the context?
        EXPECT_NE(pCTX, nullptr);
        EXPECT_EQ(pCTX->mValue, 100);

        //Cast the safe shared pointer
        auto lMyContext = std::any_cast<std::shared_ptr<SomeContextIWantInMySender> &>(pCTX->mObject);
        EXPECT_EQ(lMyContext->mSomeValue, 3);

        EXPECT_EQ(lMyContext->mSomeString, "Some String");

        //Now cast the unsafe pointer
        SomeContextIWantInMySender *lWeakContext = (SomeContextIWantInMySender *) pCTX->mUnsafePointer;
        EXPECT_EQ(lWeakContext->mSomeValue, 3);
        EXPECT_EQ(lWeakContext->mSomeString, "Some String");

        // Pass the fragments to the receiver
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        EXPECT_EQ(info, ElasticFrameMessages::noError);
    };

    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &packet,
                                         ElasticFrameProtocolContext *pCTX) {
        EXPECT_NE(pCTX, nullptr);
        EXPECT_EQ(pCTX->mValue, 200);

        //Cast the safe shared pointer
        auto lMyContext = std::any_cast<std::shared_ptr<SomeContextIWantInMyReceiver> &>(pCTX->mObject);
        EXPECT_EQ(lMyContext->mSomeOtherValue, 7);
        EXPECT_EQ(lMyContext->mSomeOtherString, "Some Other String");

        //Now cast the unsafe pointer
        SomeContextIWantInMyReceiver *lWeakContext = (SomeContextIWantInMyReceiver *) pCTX->mUnsafePointer;
        EXPECT_EQ(lWeakContext->mSomeOtherValue, 7);
        EXPECT_EQ(lWeakContext->mSomeOtherString, "Some Other String");

        dataReceived++;
    };

    uint8_t streamID = 1;
    for (int packetNumber = 0; packetNumber < 5; packetNumber++) {
        std::vector<uint8_t> mydata;
        size_t randSize = rand() % 10000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001,
                                                               packetNumber, EFP_CODE('A', 'N', 'X', 'B'),
                                                               streamID, NO_FLAGS);
        EXPECT_EQ(result, ElasticFrameMessages::noError);
    }

    EXPECT_TRUE(UnitTestHelpers::waitUntil([&]() {
        return dataReceived.load() == 5;
    }, std::chrono::milliseconds(500)));
}
