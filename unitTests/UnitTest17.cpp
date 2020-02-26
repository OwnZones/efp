//
// Created by Anders Cedronius on 2020-02-26.
//

//UnitTest17
//Send 100 frames... Stop the sender. Delete the sender. delete the reciever
//start a new sender and force the superframe counter to != 0 start the reciever

#define PACKETS_1_ROUND 100
#define PACKETS_2_ROUND 100


#include "UnitTest17.h"



void UnitTest17::sendData(const std::vector<uint8_t> &subPacket) {
  ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket, 0);
  if (info != ElasticFrameMessages::noError) {
    std::cout << "Error-> " << signed(info) << std::endl;
    unitTestFailed = true;
    unitTestActive = false;
    return;
  }
}

void
UnitTest17::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {
    if (packet->mBroken) {
      std::cout << " broken " << packet->mBroken;
      unitTestFailed = true;
      unitTestActive = false;
    }

    if (packet->mPts == 1100 && packet->mStreamID == 2) {
      unitTestActive = false;
      std::cout << "UnitTest " << unsigned(activeUnitTest) << " done." << std::endl;
    }
}

bool UnitTest17::waitForCompletion() {
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

bool UnitTest17::startUnitTest() {

    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(5, 2);
    myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest17::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest17::gotData, this, std::placeholders::_1);
    unitTestActive = true;

  std::cout << "UnitTest 17 first run " << std::endl;

    for (uint64_t packetNumber = 0; packetNumber < PACKETS_1_ROUND; packetNumber++) {
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

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::cout << "UnitTest 17 second run " << std::endl;

  delete myEFPPacker;
  myEFPPacker = new(std::nothrow) ElasticFrameProtocolSender(MTU);
  myEFPPacker->sendCallback = std::bind(&UnitTest17::sendData, this, std::placeholders::_1);
  myEFPPacker->setSuperFrameNo(4567);
  delete myEFPReciever;
  myEFPReciever = new(std::nothrow) ElasticFrameProtocolReceiver(5, 2);
  myEFPReciever->receiveCallback = std::bind(&UnitTest17::gotData, this, std::placeholders::_1);

  for (uint64_t packetNumber = 0; packetNumber < PACKETS_2_ROUND; packetNumber++) {

    streamID = 2;

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

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

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