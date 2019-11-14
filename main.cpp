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
            if (!unitTestPacketNumberSender++){
                break;
            }
            myEFPReciever.unpack(subPacket);
            break;
        default:
            unitTestFailed = true;
            unitTestActive = false;
            std::cout << "Unknown unit test!" << std::endl;
            break;
    }
}

void gotData(const std::vector<uint8_t> &packet, EdgewareFrameContent content, bool broken) {
    //std::cout << "Got data size ->" << packet.size() << " Content type: " << unsigned(content) << " Broken->" << broken << std::endl;
    uint8_t vectorChecker=0;

    switch (activeUnitTest) {
        case unitTests::unitTest2:
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
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet[0] != 0xaa) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest3 done" << std::endl;
            break;
        case unitTests::unitTest4:
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet.size() != (MTU - myEFPPacker.geType1Size()) + 1) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest4 done" << std::endl;
            break;
        case unitTests::unitTest5:
            if (broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            if (packet.size() != ((MTU*5) + (MTU/2))) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            for (auto &x: packet) {
                if (x != vectorChecker++) {
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
            //One block of MTU should be gone
            if (packet.size() != (((MTU - myEFPPacker.geType1Size())*1) + 12)) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }

            if (!broken) {
                unitTestFailed = true;
                unitTestActive = false;
                break;
            }
            vectorChecker = (MTU-myEFPPacker.geType1Size()) % 256;
            for (auto &x: packet) {
                if (x != vectorChecker++) {
                    unitTestFailed = true;
                    unitTestActive = false;
                    break;
                }
            }
            unitTestActive = false;
            activeUnitTest = unitTests::unitTestInactive;
            std::cout << "unitTest6 done" << std::endl;
            break;
        default:
            unitTestFailed = true;
            unitTestActive = false;
            std::cout << "Unknown unit test!" << std::endl;
            break;
    }
}

bool waitForCompletion() {
    while (unitTestActive) {
        usleep(1000 * 500); //half a second
    }
    if (unitTestFailed) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed." << std::endl;
        return true;
    }
    return false;
}

int main() {

    unitTestFailed = false;

    //Create a packer and provide a callback for handling transmission
    myEFPPacker.sendCallback = std::bind(&sendData, std::placeholders::_1);


    //startUnpacker:
    // timeout not fully recieved frames ms,
    // if there is head-of-line packets blocking ready frames.. )
    myEFPReciever.startUnpacker(10, 2);
    myEFPReciever.recieveCallback = std::bind(&gotData, std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3);

    /*
        std::generate(mydata.begin(), mydata.end(), [n = 0] () mutable { return n++; });
     */

    std::vector<uint8_t> mydata;
    EdgewareFrameMessages result;

    //UnitTest1
    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent
    activeUnitTest = unitTests::unitTest1;
    mydata.clear();
    mydata.resize(MTU - myEFPPacker.geType2Size());
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
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
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    if (waitForCompletion()) return EXIT_FAILURE;

    //UnitTest6
    //Test sending a packet of MTU*5+MTU/2 containing a linear vector drop the first packet -> the result should be a packet with that size containing a linear vector
    //starting with the number (MTU-headertyp1) % 256. also check for broken flag is set.
    activeUnitTest = unitTests::unitTest6;
    unitTestPacketNumberSender = 0;
    mydata.clear();
    mydata.resize(((MTU - myEFPPacker.geType1Size())*2) + 12); //(MTU / 2));
    std::generate(mydata.begin(), mydata.end(), [n = 0]() mutable { return n++; });
    unitTestActive = true;
    result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);
    if (result != EdgewareFrameMessages::noError) {
        std::cout << "Unit test number: " << unsigned(activeUnitTest) << " Failed in the packAndSend method"
                  << std::endl;
        return EXIT_FAILURE;
    }
    usleep(1000 * 20); //We need to wait for the timeout of 10ms of delivering broken frames
    if (waitForCompletion()) return EXIT_FAILURE;

    std::cout << "Tests completed" << std::endl;
    sleep(1);
    myEFPReciever.stopUnpacker();

    return 0;
}