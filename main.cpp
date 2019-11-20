
#include "EdgewareFrameProtocol.h"

#include <iostream>

#define MTU 1456 //SRT-max

enum unitTests : int {
    unitTestInactive,
    unitTest1,
    unitTest2,
    unitTest3,
    unitTest4,
    unitTest5,
    unitTest6,
    unitTest7,
    unitTest8,
    unitTest9,
    unitTest10,
    unitTest11,
    unitTest12,
    unitTest13,
    unitTest14,
    unitTest15,
    unitTest16,
    unitTest17,
    unitTest18,
    unitTest19,
    unitTest20,
    unitTest21,
    unitTest22,
    unitTest23,
    unitTest24,
    unitTest25
};
std::atomic<unitTests> activeUnitTest;
std::atomic_bool unitTestActive;
std::atomic_bool unitTestFailed;
std::vector<uint8_t> unitTestsSavedData;
std::vector<std::vector<uint8_t>> unitTestsSavedData2D;

std::atomic_int unitTestPacketNumberSender;
std::atomic_int unitTestPacketNumberReciever;

EdgewareFrameProtocol myEFPReciever;
EdgewareFrameProtocol myEFPPacker(MTU, EdgewareFrameProtocolModeNamespace::packer);

void sendData(const std::vector<uint8_t> &subPacket) {
    //std::cout << "Send data of size ->" << subPacket.size() << " Packet type: " << unsigned(subPacket[0]) << " Unit test number: " << unsigned(activeUnitTest) << std::endl;

    switch (activeUnitTest) {
        case unitTests::unitTest1:
            if (subPacket[0] != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest1 done" << std::endl;
            break;
        case unitTests::unitTest2:
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest3:
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest4:
            if (subPacket[0] != 1 && unitTestPacketNumberSender == 0) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (subPacket[0] != 2 && unitTestPacketNumberSender == 1) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberSender > 1) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            if (subPacket[0] == 1 && unitTestPacketNumberSender == 0) {
                //expected size of first packet
                if (subPacket.size() != MTU) {
                    unitTestFailed = true;
                    unitTestActive = false;
                }
            }
            if (subPacket[0] == 2 && unitTestPacketNumberSender == 1) {
                //expected size of first packet
                if (subPacket.size() != (myEFPPacker.geType2Size() + 1)) {
                    unitTestFailed = true;
                    unitTestActive = false;
                }
            }
            unitTestPacketNumberSender++;
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest5:
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest6:
            if (!unitTestPacketNumberSender++) {
                break;
            }
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest7:
            if (unitTestPacketNumberSender == 1) {
                unitTestPacketNumberSender++;
                unitTestsSavedData = subPacket;
                break;
            }
            if (unitTestPacketNumberSender == 2) {
                unitTestPacketNumberSender++;
                myEFPReciever.unpack(subPacket);
                myEFPReciever.unpack(unitTestsSavedData);
                break;
            }
            unitTestPacketNumberSender++;
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest8:
            unitTestPacketNumberSender++;
            if (subPacket[0] == 2) {
                unitTestPacketNumberSender = 0;
                myEFPReciever.unpack(subPacket);
                for (auto &x: unitTestsSavedData2D) {
                    if (unitTestPacketNumberSender == 1) {
                        unitTestsSavedData = x;
                    } else if (unitTestPacketNumberSender == 2) {
                        unitTestPacketNumberSender++;
                        myEFPReciever.unpack(x);
                        myEFPReciever.unpack(unitTestsSavedData);
                    } else {
                        myEFPReciever.unpack(x);
                    }
                    unitTestPacketNumberSender++;
                }
            } else {
                unitTestsSavedData2D.push_back(subPacket);
                break;
            }
            break;
        case unitTests::unitTest9:
            if (subPacket[0] == 2) {
                break;
            }
            myEFPReciever.unpack(subPacket);
            break;
        case unitTests::unitTest10:
            if (unitTestPacketNumberSender == 0) {
                unitTestsSavedData = subPacket;
            }

            if (unitTestPacketNumberSender == 1) {
                myEFPReciever.unpack(subPacket);
                myEFPReciever.unpack(unitTestsSavedData);
            }

            if (unitTestPacketNumberSender > 1) {
                unitTestFailed = true;
                unitTestActive = false;
            }
            unitTestPacketNumberSender++;
            break;
        default:
            unitTestFailed = true;
            unitTestActive = false;
            std::cout << "Unknown unit test!" << std::endl;
            break;
    }
}

void
gotData(EdgewareFrameProtocol::framePtr &packet, EdgewareFrameContent content, bool broken, uint64_t pts,
        uint32_t code) {
    //std::cout << "Got data size ->" << packet->frameSize << " Content type: " << unsigned(content) << " Broken->" << broken << std::endl;

    uint8_t vectorChecker = 0;

    switch (activeUnitTest) {
        case unitTests::unitTest2:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (content != EdgewareFrameContentNamespace::adts) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest2 done" << std::endl;
            break;
        case unitTests::unitTest3:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet->framedata[0] != 0xaa) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest3 done" << std::endl;
            break;
        case unitTests::unitTest4:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet->frameSize != (MTU - myEFPPacker.geType1Size()) + 1) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest4 done" << std::endl;
            break;
        case unitTests::unitTest5:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet->frameSize != ((MTU * 5) + (MTU / 2))) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            for (int x = 0; x < packet->frameSize; x++) {
                if (packet->framedata[x] != vectorChecker++) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest5 done" << std::endl;
            break;
        case unitTests::unitTest6:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            //One block of MTU should be gone
            if (packet->frameSize != (((MTU - myEFPPacker.geType1Size()) * 2) + 12)) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            if (!broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            vectorChecker = (MTU - myEFPPacker.geType1Size()) % 256;
            for (int x = (MTU - myEFPPacker.geType1Size()); x < packet->frameSize; x++) {
                if (packet->framedata[x] != vectorChecker++) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest6 done" << std::endl;
            break;
        case unitTests::unitTest7:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet->frameSize != (((MTU - myEFPPacker.geType1Size()) * 5) + 12)) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            for (int x = 0; x < packet->frameSize; x++) {
                if (packet->framedata[x] != vectorChecker++) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest7 done" << std::endl;
            break;
        case unitTests::unitTest8:
            if (pts != 1 || code != 2) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet->frameSize != (((MTU - myEFPPacker.geType1Size()) * 5) + 12)) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            for (int x = 0; x < packet->frameSize; x++) {
                if (packet->framedata[x] != vectorChecker++) {

                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }

            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest8 done" << std::endl;
            break;
        case unitTests::unitTest9:
            if (!broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (pts != UINT64_MAX) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (code != UINT32_MAX) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            for (int x = 0; x < ((MTU - myEFPPacker.geType1Size()) * 5); x++) {
                if (packet->framedata[x] != vectorChecker++) {

                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest9 done" << std::endl;
            break;
        case unitTests::unitTest10:
            unitTestPacketNumberReciever++;
            if (unitTestPacketNumberReciever == 1) {
                if (pts == 1) {
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 2) {
                if (pts == 2) {
                    unitTestActive = false;
                    activeUnitTest = unitTests::unitTestInactive;
                    std::cout << "unitTest10 done" << std::endl;
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestFailed = true;
            unitTestActive = false;
            break;
        default:
            unitTestFailed = true;
            unitTestActive = false;
            std::cout << "Unknown unit test!" << std::endl;
            break;
    }
}

bool waitForCompletion() {
    int breakOut = 0;
    while (unitTestActive) {
        usleep(1000 * 500); //half a second
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

int main() {

    //Set unit tests status
    unitTestFailed = false;

    //Provide a callback for handling transmission
    myEFPPacker.sendCallback = std::bind(&sendData, std::placeholders::_1);

    //startUnpacker:
    // timeout not fully recieved frames ms,
    // if there is head-of-line packets blocking ready frames.. )
    myEFPReciever.startUnpacker(10, 2);
    myEFPReciever.recieveCallback = std::bind(&gotData, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

    //vector of data used during the unit-tests
    std::vector<uint8_t> mydata;

    //The result of the packer
    EdgewareFrameMessages result;

    /*
     *
     * START OF THE UNIT TESTS
     *
     */


    //UnitTest1
    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent
    activeUnitTest = unitTests::unitTest1;
    mydata.clear();
    mydata.resize(MTU - myEFPPacker.geType2Size());
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;


    //UnitTest2
    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent and only one recieved
    activeUnitTest = unitTests::unitTest2;
    mydata.clear();
    mydata.resize(MTU - myEFPPacker.geType2Size());
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest3
    //Test sending 1 byte packet of value 0xaa and recieving it in the other end
    activeUnitTest = unitTests::unitTest3;
    mydata.clear();
    mydata.resize(1);
    mydata[0] = 0xaa;
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest4
    //Test sending a packet of MTU-headertyp1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the reciever
    activeUnitTest = unitTests::unitTest4;
    unitTestPacketNumberSender = 0;
    mydata.clear();
    mydata.resize((MTU - myEFPPacker.geType1Size()) + 1);
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest5
    //Test sending a packet of MTU*5+MTU/2 containing a linear vector -> the result should be a packet with that size containing a linear vector.
    activeUnitTest = unitTests::unitTest5;
    mydata.clear();
    mydata.resize((MTU * 5) + (MTU / 2));
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;


    //UnitTest6
    //Test sending a packet of MTU*5+MTU/2 containing a linear vector drop the first packet -> the result should be a packet with a hole of MTU-headertype1
    //then a linear vector of data starting with the number (MTU-headertyp1) % 256. also check for broken flag is set.
    activeUnitTest = unitTests::unitTest6;
    unitTestPacketNumberSender = 0;
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker.geType1Size()) * 2) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    usleep(1000 * 20); //We need to wait for the timeout of 10ms of delivering broken frames
    if (waitForCompletion()) return EXIT_FAILURE;


    //UnitTest7
    //Test sending packets, 5 type 1 + 1 type 2.. Reorder type1 packet 3 and 2 so the delivery order is 1 3 2 4 5 6
    //then check for correct length and correct vector in the payload
    activeUnitTest = unitTests::unitTest7;
    unitTestPacketNumberSender = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker.geType1Size()) * 5) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;


    //UnitTest8
    //Test sending packets, 5 type 1 + 1 type 2.. Send the type2 packet first to the unpacker and send the type1 packets out of order
    activeUnitTest = unitTests::unitTest8;
    unitTestPacketNumberSender = 0;
    unitTestsSavedData.clear();
    unitTestsSavedData2D.clear();
    mydata.clear();
    size_t mysize = ((MTU - myEFPPacker.geType1Size()) * 5) + 12;
    mydata.resize(mysize);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts, 1, 2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest9
    //Test sending packets, 5 type 1 + 1 type 2.. Drop the type 2 packet.
    //broken should be set, the PTS and code should be set to the illegal value and the vector should be linear for 5 times MTU - myEFPPacker.geType1Size()
    activeUnitTest = unitTests::unitTest9;
    unitTestPacketNumberSender = 0;
    unitTestPacketNumberReciever = 0;
    unitTestsSavedData.clear();
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker.geType1Size()) * 5) + 12);
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest10
    //send two type 2 packets out of order and recieve them in order.
    activeUnitTest = unitTests::unitTest10;
    mydata.clear();
    mydata.resize(MTU-myEFPPacker.geType2Size());
    unitTestActive = true;

    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264b,1,0);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264b,2,0);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }

    if (waitForCompletion()) return EXIT_FAILURE;

    std::cout << "Tests completed" << std::endl;
    sleep(1);
    myEFPReciever.stopUnpacker();

    return 0;
}