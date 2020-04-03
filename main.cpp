

#include "unitTests/UnitTest1.h"
#include "unitTests/UnitTest2.h"
#include "unitTests/UnitTest3.h"
#include "unitTests/UnitTest4.h"
#include "unitTests/UnitTest5.h"
#include "unitTests/UnitTest6.h"
#include "unitTests/UnitTest7.h"
#include "unitTests/UnitTest8.h"
#include "unitTests/UnitTest9.h"
#include "unitTests/UnitTest10.h"
#include "unitTests/UnitTest11.h"
#include "unitTests/UnitTest12.h"
#include "unitTests/UnitTest13.h"
#include "unitTests/UnitTest14.h"
#include "unitTests/UnitTest15.h"
#include "unitTests/UnitTest16.h"
#include "unitTests/UnitTest17.h"
#include "unitTests/UnitTest18.h"
#include "unitTests/PerformanceLab.h"

#include <iostream>

int main() {

    //PerformanceLab sends/receives a endless stream of packets (Used when profiling the code)
    //PerformanceLab myPerformanceLab;
    //myPerformanceLab.startUnitTest();
    //code will never get to here

    int returnCode = EXIT_SUCCESS;

    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent
    UnitTest1 unitTest1;
    if (!unitTest1.startUnitTest()) {
        std::cout << "Unit test 1 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending a packet less than MTU + header - > Expected result is one type2 frame only sent and only one recieved
    UnitTest2 unitTest2;
    if (!unitTest2.startUnitTest()) {
        std::cout << "Unit test 2 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending 1 byte packet of value 0xaa and recieving it in the other end
    UnitTest3 unitTest3;
    if (!unitTest3.startUnitTest()) {
        std::cout << "Unit test 3 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending a packet of MTU-headertyp1+1 > result should be one frame type1 and a frame type 2, MTU+1 at the reciever
    UnitTest4 unitTest4;
    if (!unitTest4.startUnitTest()) {
        std::cout << "Unit test 4 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending a packet of MTU*5+MTU/2 containing a linear vector -> the result should be a packet with that size containing a linear vector.
    UnitTest5 unitTest5;
    if (!unitTest5.startUnitTest()) {
        std::cout << "Unit test 5 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending a packet of MTU*5+MTU/2 containing a linear vector drop the first packet -> the result should be a packet with a hole of MTU-headertype1
    //then a linear vector of data starting with the number (MTU-headertyp1) % 256. also check for broken flag is set.
    UnitTest6 unitTest6;
    if (!unitTest6.startUnitTest()) {
        std::cout << "Unit test 6 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending packets, 5 type 1 + 1 type 2.. Reorder type1 packet 3 and 2 so the delivery order is 1 3 2 4 5 6
    //then check for correct length and correct vector in the payload
    UnitTest7 unitTest7;
    if (!unitTest7.startUnitTest()) {
        std::cout << "Unit test 7 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending packets, 5 type 1 + 1 type 2.. Send the type2 packet first to the unpacker and send the type1 packets out of order
    UnitTest8 unitTest8;
    if (!unitTest8.startUnitTest()) {
        std::cout << "Unit test 8 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending packets, 5 type 1 + 1 type 2.. Drop the type 2 packet.
    //broken should be set, the PTS and code should be set to the illegal value and the vector should be linear for 5 times MTU - myEFPPacker.geType1Size()
    UnitTest9 unitTest9;
    if (!unitTest9.startUnitTest()) {
        std::cout << "Unit test 9 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //send two type 2 packets out of order and recieve them in order.
    UnitTest10 unitTest10;
    if (!unitTest10.startUnitTest()) {
        std::cout << "Unit test 10 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending 5 packets, 5 type 1 + 1 type 2..
    //Reverse the packets to the unpacker and drop the middle packet (packet 3)
    //This is testing the out of order head of line blocking mechanism
    //The result should be deliver packet 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
    UnitTest11 unitTest11;
    if (!unitTest11.startUnitTest()) {
        std::cout << "Unit test 11 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Test sending 5 packets, 5 type 1 + 1 type 2..
    //Reverse the packets to the unpacker and drop the middle packet (packet 3) also deliver the fragments reversed meaning packet 5 last fragment first..
    //This is testing the out of order head of line blocking mechanism
    //The result should be deliver packer 1,2,4,5 even though we gave the unpacker them in order 5,4,2,1.
    UnitTest12 unitTest12;
    if (!unitTest12.startUnitTest()) {
        std::cout << "Unit test 12 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }


    //Test sending 100 000 superframes
    //Check PTS and DTS values sent are also received
    UnitTest13 unitTest13;
    if (!unitTest13.startUnitTest()) {
        std::cout << "Unit test 13 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //Send 15 packets with embeddedPrivateData. odd packet numbers will have two embedded private data fields. Also check for not broken and correct fourcc code.
    //the reminder of the packet is a vector. Check it's integrity
    UnitTest14 unitTest14;
    if (!unitTest14.startUnitTest()) {
        std::cout << "Unit test 14 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    //This is the crazy-monkey test1. We randomize the size for 1000 packets. We store the size in a private struct and embedd it.
    //when we receive the packet we check the size saved in the embedded data and also the linear vector in the payload.
    //This test triggers the type3 fragment
    UnitTest15 unitTest15;
    if (!unitTest15.startUnitTest()) {
        std::cout << "Unit test 15 failed" << std::endl;
        returnCode = EXIT_FAILURE;


        //UnitTest16 unitTest16;  //WIP
        //if (!unitTest16.startUnitTest()) {
        //    std::cout << "Unit test 16 failed" << std::endl;
        //    returnCode=EXIT_FAILURE;
        //}
    }

    UnitTest17 unitTest17;
    if (!unitTest17.startUnitTest()) {
        std::cout << "Unit test 17 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    UnitTest18 unitTest18;
    if (!unitTest18.startUnitTest()) {
        std::cout << "Unit test 18 failed" << std::endl;
        returnCode = EXIT_FAILURE;
    }

    return returnCode;
}
