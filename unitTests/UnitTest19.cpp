//
// Created by Anders Cedronius on 2020-09-15.
//

//UnitTest19

#include "UnitTest19.h"

//UnitTest19
//Testing the optional context that can be used in the callbacks

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

void UnitTest19::sendData(const std::vector<uint8_t> &subPacket, uint8_t streamID, ElasticFrameProtocolContext* pCTX) {

    //Did we even get the context?
    if (!pCTX) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    //Is the value correct?
    if (pCTX->mValue != 100) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    //Cast the safe shared pointer
    auto lMyContext = std::any_cast<std::shared_ptr<SomeContextIWantInMySender>&>(pCTX->mObject);

    //Are we getting the data as expected?
    if (lMyContext->mSomeValue != 3) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    if (lMyContext->mSomeString.compare("Some String") != 0) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    //Now cast the unsafe pointer
    SomeContextIWantInMySender* lWeakContext = (SomeContextIWantInMySender*) pCTX->mUnsafePointer;

    //Are we getting the data as expected?
    if (lWeakContext->mSomeValue != 3) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    if (lWeakContext->mSomeString.compare("Some String") != 0) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    // Pass the fragments to the receiver
    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket, 0);
    if (info != ElasticFrameMessages::noError) {
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void UnitTest19::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet, ElasticFrameProtocolContext* pCTX) {
//    std::cout << "DTS: " << unsigned (packet->mDts) << std::endl;


    //Did we even get the context?
    if (!pCTX) {
        unitTestFailed = true;
        unitTestActive = false;
        return;
    }

    //Is the value correct?
    if (pCTX->mValue != 200) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    //Cast the safe shared pointer
    auto lMyContext = std::any_cast<std::shared_ptr<SomeContextIWantInMyReceiver>&>(pCTX->mObject);

    //Are we getting the data as expected?
    if (lMyContext->mSomeOtherValue != 7) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    if (lMyContext->mSomeOtherString.compare("Some Other String") != 0) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    //Now cast the unsafe pointer
    SomeContextIWantInMyReceiver* lWeakContext = (SomeContextIWantInMyReceiver*) pCTX->mUnsafePointer;

    //Are we getting the data as expected?
    if (lWeakContext->mSomeOtherValue != 7) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    if (lWeakContext->mSomeOtherString.compare("Some Other String") != 0) {
        unitTestFailed = true;
        unitTestActive = false;
    }

    //End the test?
    if (packet->mDts == 4) {
        std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
        unitTestActive = false;
    }
}


bool UnitTest19::waitForCompletion() {
    int breakOut = 0;
    while (unitTestActive) {
        //quarter of a second
        std::this_thread::sleep_for(std::chrono::microseconds(1000 * 250));
        if (breakOut++ == 10) {
            std::cout << "waitForCompletion did wait for 5 seconds. fail the test." << std::endl;
            unitTestFailed = true;
            unitTestActive = false;
        }
    }
    if (unitTestFailed) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed." << std::endl;
        return true;
    }
    return false;
}

bool UnitTest19::startUnitTest() {
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;

    // ---------------------------------------------------------------------------
    //Sender Context part ----------------
    // ---------------------------------------------------------------------------

    //The std::any part
    auto mySenderCTX = std::make_shared<ElasticFrameProtocolContext>();
    mySenderCTX->mObject = std::make_shared<SomeContextIWantInMySender>();

    //The void* part
    auto getBackMySndObject = std::any_cast<std::shared_ptr<SomeContextIWantInMySender>&>(mySenderCTX->mObject);
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
    auto getBackMyRcvObject = std::any_cast<std::shared_ptr<SomeContextIWantInMyReceiver>&>(myReceiverCTX->mObject);
    myReceiverCTX->mUnsafePointer = getBackMyRcvObject.get();

    //The value part
    myReceiverCTX->mValue = 200;

    myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(100, 40, myReceiverCTX);
    myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU, mySenderCTX);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest19::sendData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    myEFPReciever->receiveCallback = std::bind(&UnitTest19::gotData, this, std::placeholders::_1, std::placeholders::_2);
    unitTestActive = true;
    for (int packetNumber = 0; packetNumber < 5; packetNumber++) {
        mydata.clear();
        size_t randSize = rand() % 10000 + 1;
        mydata.resize(randSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber + 1001, packetNumber, EFP_CODE('A', 'N', 'X', 'B'), streamID, NO_FLAGS);
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            delete myEFPPacker;
            delete myEFPReciever;
            return false;
        }
    }

    if (waitForCompletion()) {
        delete myEFPPacker;
        delete myEFPReciever;
        return false;
    } else {
        delete myEFPPacker;
        delete myEFPReciever;
        return true;
    }
}