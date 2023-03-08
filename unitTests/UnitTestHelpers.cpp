#include "UnitTestHelpers.h"

bool UnitTestHelpers::waitUntil(const std::function<bool()> &function, std::chrono::milliseconds timeout,
                                std::chrono::milliseconds sleepFor) {
    std::chrono::milliseconds timeLeft = timeout;
    while (timeLeft > std::chrono::milliseconds(0)) {
        if (function()) {
            return true;
        }

        std::this_thread::sleep_for(sleepFor);
        timeLeft -= sleepFor;
    }
    return function();
}
