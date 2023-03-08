#pragma once

#include <functional>
#include <chrono>
#include <thread>

namespace UnitTestHelpers {

    ///
    /// @brief Wait until a condition is met
    /// @param function The function used to check if the condition is met
    /// @param timeout The maximum time to wait for the condition to be met
    /// @param sleepFor Time to sleep between each check if the condition is met
    /// @return True if the condition was met within the time frame, false otherwise
    bool waitUntil(const std::function<bool()> &function,
                   std::chrono::milliseconds timeout,
                   std::chrono::milliseconds sleepFor = std::chrono::milliseconds(10));

} // namespace UnitTestHelpers