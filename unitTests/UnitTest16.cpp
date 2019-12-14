//
// Created by Anders Cedronius on 2019-12-05.
//

//UnitTest16
//Random size from 1 to 1000000



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
                ElasticFrameMessages info = myEFPReciever->unpack(x, 0);
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

    ElasticFrameMessages info = myEFPReciever->unpack(subPacket, 0);
    if (info != ElasticFrameMessages::noError) {
        std::cout << "Error-> " << signed(info) << std::endl;
        unitTestFailed = true;
        unitTestActive = false;
    }
}

void
UnitTest16::gotData(ElasticFrameProtocol::pFramePtr &packet, ElasticFrameContent content, bool broken, uint64_t pts,
                    uint32_t code, uint8_t stream, uint8_t flags) {

    testDataMtx.lock();
    bool isLoss = false;
    do {
        TestProps currentProps = testData[unitTestPacketNumberReciever];
        isLoss = currentProps.loss;
        unitTestPacketNumberReciever++;
    } while (isLoss);

    testDataMtx.unlock();


    if (!broken) {
        uint8_t vectorChecker = 0;
        for (int x = 0; x < packet->frameSize; x++) {
            if (packet->framedata[x] != vectorChecker++) {
                std::cout << "Vector failed for packet " << unsigned(pts) << std::endl;
                unitTestFailed = true;
                unitTestActive = false;
                return;
            }
        }
    }

    unitTestPacketNumberReciever++;


    debugPrintMutex.lock();
    std::cout << "Got -> " << unsigned(pts);
    std::cout << " broken " << broken;
    std::cout << " code " << code;
    std::cout << std::endl;
    debugPrintMutex.unlock();

}

bool UnitTest16::waitForCompletion() {
    int breakOut = 0;
    while (unitTestActive) {
        usleep(1000 * 250); //quarter of a second
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
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    randEng =  std::default_random_engine(seed);
    unitTestFailed = false;
    unitTestActive = false;
    ElasticFrameMessages result;
    std::vector<uint8_t> mydata;
    uint8_t streamID = 1;
    myEFPReciever = new(std::nothrow) ElasticFrameProtocol();
    myEFPPacker = new(std::nothrow) ElasticFrameProtocol(MTU, ElasticFrameProtocolModeNamespace::packer);
    if (myEFPReciever == nullptr || myEFPPacker == nullptr) {
        if (myEFPReciever) delete myEFPReciever;
        if (myEFPPacker) delete myEFPPacker;
        return false;
    }
    myEFPPacker->sendCallback = std::bind(&UnitTest16::sendData, this, std::placeholders::_1);
    myEFPReciever->recieveCallback = std::bind(&UnitTest16::gotData, this, std::placeholders::_1, std::placeholders::_2,
                                               std::placeholders::_3, std::placeholders::_4, std::placeholders::_5,
                                               std::placeholders::_6, std::placeholders::_7);
    myEFPReciever->startUnpacker(5, 2);

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

        result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber, 'ANXB', streamID, NO_FLAGS);
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest)
                      << " Failed in the packAndSend method. Error-> " << signed(result)
                      << std::endl;
            myEFPReciever->stopUnpacker();
            delete myEFPReciever;
            delete myEFPPacker;
            return false;
        }
    }

    if (waitForCompletion()) {
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return false;
    } else {
        myEFPReciever->stopUnpacker();
        delete myEFPReciever;
        delete myEFPPacker;
        return true;
    }
}