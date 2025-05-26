#include "cwTimerManager.h"
#include <chrono>

void cwTimerManager::AddTimer(const std::string& name,
    std::function<void()> callback,
    int interval_ms,
    int max_trigger_count,
    bool debug) {
    timers_.emplace(name, cwTimer(name, callback, interval_ms, max_trigger_count, debug));
}

void cwTimerManager::CancelTimer(const std::string& name) {
    auto it = timers_.find(name);
    if (it != timers_.end()) {
        it->second.Cancel();
    }
}

void cwTimerManager::CheckAndRunTimers() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = timers_.begin(); it != timers_.end(); ) {
        if (it->second.ShouldTrigger(now)) {
            it->second.Trigger(now);
        }
        if (it->second.IsExpired()) {
            it = timers_.erase(it);
        }
        else {
            ++it;
        }
    }
}
