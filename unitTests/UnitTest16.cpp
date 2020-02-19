//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest16
//Random size from 1 to 1000000

//This unit test is WIP

#define LOSSRATE 2 // 2% lost frames
#define BROKEN 2 // 2% superframes broken
#define OUTOFORDER 10 // 10% out of order deliveries
#define NUMBER_TOTAL_PACKETS 1000 //Number of total packets sent in this unit test

#include "UnitTest16.h"

void UnitTest16::sendData(const std::vector<uint8_t> &subPacket) {

    testDataMtx.lock();
    TestProps currentProps = testData.back();
    testDataMtx.unlock();

    if (currentProps.loss) {
        return;
    }

    /*
    if (currentProps.pts == 293) {
        counter293 ++;
        debugPrintMutex.lock();
        std::cout << "293Debug -> " << unsigned(reorderBuffer.size());
        std::cout << " 293Debug " << unsigned(counter293);
        std::cout << " type " << unsigned((subPacket[0] & 0x0f));
        std::cout << std::endl;
        debugPrintMutex.unlock();
    }
*/

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
            /*
            if (currentProps.pts == 293) {
                debugPrintMutex.lock();
                std::cout << "293Debug -> " << unsigned(reorderBuffer.size());
                std::cout << " 293Debug " << unsigned(counter293);
                std::cout << std::endl;
                debugPrintMutex.unlock();
            }
             */
            for (auto const& x : reorderBuffer) {
                ElasticFrameMessages info = myEFPReciever->receiveFragment(x, 0);
                if (info != ElasticFrameMessages::noError) {
                    std::cout << "Error-> " << signed(info) << std::endl;
                    unitTestFailed = true;
                    unitTestActive = false;
                    return;
                }
            }
            //BAM!!
            //
        } else if ((subPacket[0] & 0x0f) == 3) {
            reorderBuffer.emplace_back(subPacket);
            return;
            //type3
        } else {
            unitTestFailed = true;
            unitTestActive = false;
            return;
        }
        return;
    }

    // std::cout << "Reorder: " << currentProps.reorder << " loss: " << currentProps.loss << std::endl;

    ElasticFrameMessages info = myEFPReciever->receiveFragment(subPacket, 0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void
UnitTest16::gotData(ElasticFrameProtocolReceiver::pFramePtr &packet) {

    testDataMtx.lock();
    bool isLoss = false;
    do {
        TestProps currentProps = testData[unitTestPacketNumberReciever];
        isLoss = currentProps.loss;
        unitTestPacketNumberReciever++;
    } while (isLoss);

    testDataMtx.unlock();


    if (!packet->mBroken) {
        uint8_t vectorChecker = 0;
        for (int x = 0; x < packet->mFrameSize; x++) {
            if (packet->pFrameData[x] != vectorChecker++) {
                std::cout << "Vector failed for packet " << unsigned(packet->mPts) << std::endl;
                unitTestFailed = true;
                unitTestActive = false;
                return;
            }
        }
    }

    unitTestPacketNumberReciever++;


    debugPrintMutex.lock();
    std::cout << "Got -> " << unsigned(packet->mPts);
    std::cout << " broken " << packet->mBroken;
    std::cout << " code " << packet->mCode;
    std::cout << std::endl;
    debugPrintMutex.unlock();

}

bool UnitTest16::waitForCompletion() {
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

bool UnitTest16::startUnitTest() {
    unsigned seed = (unsigned) std::chrono::system_clock::now().time_since_epoch().count();
    randEng =  std::default_random_engine(seed);
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
    myEFPPacker->sendCallback = std::bind(&UnitTest16::sendData, this, std::placeholders::_1);
    myEFPReciever->receiveCallback = std::bind(&UnitTest16::gotData, this, std::placeholders::_1);

    unitTestPacketNumberReciever = 0;

    unitTestActive = true;

    for (uint64_t packetNumber = 0; packetNumber < NUMBER_TOTAL_PACKETS; packetNumber++) {
        mydata.clear();

        size_t randSize = rand() % 1000000 + 1;

        mydata.resize(randSize);

        std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });

        TestProps myTestProps;

        myTestProps.sizeOfData = mydata.size();

        size_t loss = rand() % 100 + 1;
        if (loss <= LOSSRATE) {
            myTestProps.loss = true;
        }

        size_t broken = rand() % 100 + 1;
        if (broken <= BROKEN) {
            myTestProps.broken = true;
        }

        size_t ooo = rand() % 100 + 1;
        if (ooo <= OUTOFORDER) {
            myTestProps.reorder = true;
        }


        debugPrintMutex.lock();
        std::cout << "Send -> " << unsigned(packetNumber);
        std::cout << " broken " << myTestProps.broken;
        std::cout << " loss " << myTestProps.loss;
        std::cout << " reorder " << myTestProps.reorder;
        std::cout << " size " << unsigned(mydata.size());
        std::cout << std::endl;
        debugPrintMutex.unlock();

        myTestProps.pts = packetNumber;

        testDataMtx.lock();
        testData.emplace_back(myTestProps);
        testDataMtx.unlock();

        brokenCounter = 0;
        reorderBuffer.clear();

        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber, packetNumber+1001, EFP_CODE('A', 'N', 'X', 'B'), streamID, NO_FLAGS);
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