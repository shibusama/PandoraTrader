#pragma once

#include "cwTimer.h"
#include <unordered_map>
#include <string>

class cwTimerManager {
public:
    void AddTimer(const std::string& name,
        std::function<void()> callback,
        int interval_ms,
        int max_trigger_count = -1,
        bool debug = false);

    void CancelTimer(const std::string& name);
    void CheckAndRunTimers();

private:
    std::unordered_map<std::string, cwTimer> timers_;
};
