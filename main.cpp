

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

#include <iostream>

int main() {
/*
    for (int i=0;i<1000;i++) {
        size_t loss = rand() % 100 + 1;
        std::cout << unsigned(loss) << std::endl;
    }

    return EXIT_SUCCESS;
*/

    int returnCode=EXIT_SUCCESS;

    
    UnitTest1 unitTest1;
    if (!unitTest1.startUnitTest()) {
        std::cout << "Unit test 1 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest2 unitTest2;
    if (!unitTest2.startUnitTest()) {
        std::cout << "Unit test 2 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest3 unitTest3;
    if (!unitTest3.startUnitTest()) {
        std::cout << "Unit test 3 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest4 unitTest4;
    if (!unitTest4.startUnitTest()) {
        std::cout << "Unit test 4 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest5 unitTest5;
    if (!unitTest5.startUnitTest()) {
        std::cout << "Unit test 5 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest6 unitTest6;
    if (!unitTest6.startUnitTest()) {
        std::cout << "Unit test 6 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest7 unitTest7;
    if (!unitTest7.startUnitTest()) {
        std::cout << "Unit test 7 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest8 unitTest8;
    if (!unitTest8.startUnitTest()) {
        std::cout << "Unit test 8 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest9 unitTest9;
    if (!unitTest9.startUnitTest()) {
        std::cout << "Unit test 9 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest10 unitTest10;
    if (!unitTest10.startUnitTest()) {
        std::cout << "Unit test 10 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest11 unitTest11;
    if (!unitTest11.startUnitTest()) {
        std::cout << "Unit test 11 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest12 unitTest12;
    if (!unitTest12.startUnitTest()) {
        std::cout << "Unit test 12 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest13 unitTest13;
    if (!unitTest13.startUnitTest()) {
        std::cout << "Unit test 13 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest14 unitTest14;
    if (!unitTest14.startUnitTest()) {
        std::cout << "Unit test 14 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    UnitTest15 unitTest15;
    if (!unitTest15.startUnitTest()) {
        std::cout << "Unit test 15 failed" << std::endl;
        returnCode=EXIT_FAILURE;
    }

    //UnitTest16 unitTest16;  //WIP
    //if (!unitTest16.startUnitTest()) {
    //    std::cout << "Unit test 16 failed" << std::endl;
    //    returnCode=EXIT_FAILURE;
    //}

    return returnCode;
}
