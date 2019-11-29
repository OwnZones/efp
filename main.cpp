
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
std::vector<std::vector<std::vector<uint8_t>>> unitTestsSavedData3D;
uint64_t expectedPTS;

std::atomic_int unitTestPacketNumberSender;
std::atomic_int unitTestPacketNumberReciever;

EdgewareFrameProtocol myEFPReciever;
EdgewareFrameProtocol myEFPPacker(MTU, EdgewareFrameProtocolModeNamespace::packer);

void sendData(const std::vector<uint8_t> &subPacket) {
    //std::cout << "Send data of size ->" << subPacket.size() << " Packet type: " << unsigned(subPacket[0]) << " Unit test number: " << unsigned(activeUnitTest) << std::endl;

    EdgewareFrameMessages info;

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
            info = myEFPReciever.unpack(subPacket,0);
            if (info != EdgewareFrameMessages::noError) {
                std::cout << "Error-> " << unsigned(info) << std::endl;
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            break;
        case unitTests::unitTest3:
            info = myEFPReciever.unpack(subPacket,0);
            if (info != EdgewareFrameMessages::noError) {
                std::cout << "Error-> " << unsigned(info) << std::endl;
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
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
            myEFPReciever.unpack(subPacket,0);
            break;
        case unitTests::unitTest5:
            myEFPReciever.unpack(subPacket,0);
            break;
        case unitTests::unitTest6:
            if (!unitTestPacketNumberSender++) {
                break;
            }
            myEFPReciever.unpack(subPacket,0);
            break;
        case unitTests::unitTest7:
            if (unitTestPacketNumberSender == 1) {
                unitTestPacketNumberSender++;
                unitTestsSavedData = subPacket;
                break;
            }
            if (unitTestPacketNumberSender == 2) {
                unitTestPacketNumberSender++;
                myEFPReciever.unpack(subPacket,0);
                myEFPReciever.unpack(unitTestsSavedData,0);
                break;
            }
            unitTestPacketNumberSender++;
            myEFPReciever.unpack(subPacket,0);
            break;
        case unitTests::unitTest8:
            unitTestPacketNumberSender++;
            if (subPacket[0] == 2) {
                unitTestPacketNumberSender = 0;
                myEFPReciever.unpack(subPacket,0);
                for (auto &x: unitTestsSavedData2D) {
                    if (unitTestPacketNumberSender == 1) {
                        unitTestsSavedData = x;
                    } else if (unitTestPacketNumberSender == 2) {
                        unitTestPacketNumberSender++;
                        myEFPReciever.unpack(x,0);
                        myEFPReciever.unpack(unitTestsSavedData,0);
                    } else {
                        myEFPReciever.unpack(x,0);
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
            myEFPReciever.unpack(subPacket,0);
            break;
        case unitTests::unitTest10:
            if (unitTestPacketNumberSender == 0) {
                unitTestsSavedData = subPacket;
            }

            if (unitTestPacketNumberSender == 1) {
                myEFPReciever.unpack(subPacket,0);
                myEFPReciever.unpack(unitTestsSavedData,0);
            }

            if (unitTestPacketNumberSender > 1) {
                unitTestFailed = true;
                unitTestActive = false;
            }
            unitTestPacketNumberSender++;
            break;
        case unitTests::unitTest11:
            if (subPacket[0] == 2) {
                unitTestPacketNumberSender++;
                unitTestsSavedData2D.push_back(subPacket);
                unitTestsSavedData3D.push_back(unitTestsSavedData2D);
                if (unitTestPacketNumberSender == 5) {
                    for (int item=unitTestsSavedData3D.size();item > 0;item--) {
                        int pakCnt=0;
                        for (auto &x: unitTestsSavedData3D[item-1]) {
                            if (item != 3) {
                                myEFPReciever.unpack(x,0);
                            }
                        }
                    }
                }
                unitTestsSavedData2D.clear();
                break;
            }
            unitTestsSavedData2D.push_back(subPacket);
            break;
        case unitTests::unitTest12:
            if (subPacket[0] == 2) {
                unitTestPacketNumberSender++;
                unitTestsSavedData2D.push_back(subPacket);
                unitTestsSavedData3D.push_back(unitTestsSavedData2D);
                if (unitTestPacketNumberSender == 5) {
                    for (int item=unitTestsSavedData3D.size();item > 0;item--) {
                        std::vector<std::vector<uint8_t>> unitTestsSavedData2DLocal=unitTestsSavedData3D[item-1];
                        for (int fragment=unitTestsSavedData2DLocal.size();fragment > 0;fragment--) {
                            if (item != 3) {
                                myEFPReciever.unpack(unitTestsSavedData2DLocal[fragment-1],0);
                            }
                        }
                    }
                }
                unitTestsSavedData2D.clear();
                break;
            }
            unitTestsSavedData2D.push_back(subPacket);
            break;
        case unitTests::unitTest13:
            myEFPReciever.unpack(subPacket,0);
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
        uint32_t code, uint8_t stream, uint8_t flags) {
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

            //Code can be whatever here

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
        case unitTests::unitTest11:
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            unitTestPacketNumberReciever++;
            if (unitTestPacketNumberReciever == 1) {
                if (pts == 1) {
                    expectedPTS=2;
                    break;
                }
                if (pts == 2) {
                    expectedPTS=4;
                    break;
                }
                if (pts == 4) {
                    expectedPTS=5;
                    break;
                }
                if (pts == 5) {
                    unitTestActive = false;
                    activeUnitTest = unitTests::unitTestInactive;
                    std::cout << "unitTest11 done" << std::endl;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 2) {
                if (expectedPTS == pts) {
                    if (pts == 2) {
                        expectedPTS=4;
                    }
                    if (pts == 4) {
                        expectedPTS=5;
                    }
                    if (pts == 5) {
                        unitTestActive = false;
                        activeUnitTest = unitTests::unitTestInactive;
                        std::cout << "unitTest11 done" << std::endl;
                    }
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 3) {
                if (expectedPTS == pts) {
                    if (pts == 4) {
                        expectedPTS=5;
                    }
                    if (pts == 5) {
                        unitTestActive = false;
                        activeUnitTest = unitTests::unitTestInactive;
                        std::cout << "unitTest11 done" << std::endl;
                    }
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 4) {
                if (expectedPTS == pts) {
                    unitTestActive = false;
                    activeUnitTest = unitTests::unitTestInactive;
                    std::cout << "unitTest11 done" << std::endl;
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestFailed = true;
            unitTestActive = false;
            break;
        case unitTests::unitTest12:
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            unitTestPacketNumberReciever++;
            if (unitTestPacketNumberReciever == 1) {
                if (pts == 1) {
                    expectedPTS=2;
                    break;
                }
                if (pts == 2) {
                    expectedPTS=4;
                    break;
                }
                if (pts == 4) {
                    expectedPTS=5;
                    break;
                }
                if (pts == 5) {
                    unitTestActive = false;
                    activeUnitTest = unitTests::unitTestInactive;
                    std::cout << "unitTest12 done" << std::endl;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 2) {
                if (expectedPTS == pts) {
                    if (pts == 2) {
                        expectedPTS=4;
                    }
                    if (pts == 4) {
                        expectedPTS=5;
                    }
                    if (pts == 5) {
                        unitTestActive = false;
                        activeUnitTest = unitTests::unitTestInactive;
                        std::cout << "unitTest12 done" << std::endl;
                    }
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 3) {
                if (expectedPTS == pts) {
                    if (pts == 4) {
                        expectedPTS=5;
                    }
                    if (pts == 5) {
                        unitTestActive = false;
                        activeUnitTest = unitTests::unitTestInactive;
                        std::cout << "unitTest12 done" << std::endl;
                    }
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (unitTestPacketNumberReciever == 4) {
                if (expectedPTS == pts) {
                    unitTestActive = false;
                    activeUnitTest = unitTests::unitTestInactive;
                    std::cout << "unitTest12 done" << std::endl;
                    break;
                }
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestFailed = true;
            unitTestActive = false;
            break;
        case unitTests::unitTest13:
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestPacketNumberReciever++;

            if (unitTestPacketNumberReciever != pts) {
                std::cout << "bug got " << unsigned(pts) << " Expected " << unsigned(unitTestPacketNumberReciever) << std::endl;
                unitTestPacketNumberReciever = pts;
            }

            if (unitTestPacketNumberReciever < 100000) {
                if (!(unitTestPacketNumberReciever % 1000)) {
                    std::cout << "Got packet number " << unsigned(unitTestPacketNumberReciever) << std::endl;
                }
                break;
            }

            if (unitTestPacketNumberReciever == 100000) {
                unitTestActive = false;
                activeUnitTest = unitTests::unitTestInactive;
                std::cout << "unitTest13 done" << std::endl;
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

    myEFPReciever.startUnpacker(5, 2);

    myEFPReciever.recieveCallback = std::bind(&gotData, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);

    //vector of data used during the unit-tests
    std::vector<uint8_t> mydata;

    //The result of the packer
    EdgewareFrameMessages result;

    /*
     *
     * START OF THE UNIT TESTS
     *
     */

    uint8_t streamID=1;

    //UnitTest1
    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent
    activeUnitTest = unitTests::unitTest1;
    mydata.clear();
    mydata.resize(MTU - myEFPPacker.geType2Size());
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts, 1, 2,streamID,NO_FLAGS);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts,1,2,streamID,NO_FLAGS);
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

    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264,1,0,streamID,NO_FLAGS);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264,2,0,streamID,NO_FLAGS);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }

    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest11
    //Test sending 5 packets, 5 type 1 + 1 type 2..
    //Reverse the packets to the unpacker and drop the middle packet (packet 3)
    //This is testing the out of order head of line blocking mechanism
    //The result should be deliver packet 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
    activeUnitTest = unitTests::unitTest11;
    mydata.clear();
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 0;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;
    mydata.resize(((MTU - myEFPPacker.geType1Size()) * 5) + 12);
    unitTestActive = true;

    for (int packetNumber=0;packetNumber < 5; packetNumber++) {
        result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264, packetNumber+1, 0,streamID,NO_FLAGS);
        if (result != EdgewareFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                      << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (waitForCompletion()) return EXIT_FAILURE;


    //UnitTest12
    //Test sending 5 packets, 5 type 1 + 1 type 2..
    //Reverse the packets to the unpacker and drop the middle packet (packet 3) also deliver the fragments reversed meaning packet 5 last fragment first..
    //This is testing the out of order head of line blocking mechanism
    //The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
    activeUnitTest = unitTests::unitTest12;
    mydata.clear();
    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 0;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;
    mydata.resize(((MTU - myEFPPacker.geType1Size()) * 5) + 12);
    unitTestActive = true;

    for (int packetNumber=0;packetNumber < 5; packetNumber++) {
        result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264, packetNumber+1, 0,streamID,NO_FLAGS);
        if (result != EdgewareFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                      << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (waitForCompletion()) return EXIT_FAILURE;



    //UnitTest13
    //Test sending 100 000 superframes of size from 500 to 10.000 bytes
    //Reverse the packets to the unpacker and drop the middle packet (packet 3) also deliver the fragments reversed meaning packet 5 last fragment first..
    //This is testing the out of order head of line blocking mechanism
    //The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
    activeUnitTest = unitTests::unitTest13;

    std::cout << "here1 " << std::endl;

    unitTestsSavedData2D.clear();
    unitTestsSavedData3D.clear();
    expectedPTS = 0;
    unitTestPacketNumberSender=0;
    unitTestPacketNumberReciever = 0;

    unitTestActive = true;
    for (int packetNumber=0;packetNumber < 100000; packetNumber++) {

        mydata.clear();
        mydata.resize(((MTU - myEFPPacker.geType1Size()) * 5) + 12);

        //std::cout << "Pack " << unsigned(packetNumber) << std::endl;

        result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::h264, packetNumber+1, 0, streamID, INLINE_PAYLOAD);
        if (result != EdgewareFrameMessages::noError) {
            std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                      << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (waitForCompletion()) return EXIT_FAILURE;

    std::cout << "Tests completed" << std::endl;
    sleep(1);
    myEFPReciever.stopUnpacker();

    return 0;
}