#pragma once

#include <functional>
#include <chrono>
#include <string>

class cwTimer {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    cwTimer(const std::string& name,
        std::function<void()> callback,
        int interval_ms,
        int max_trigger_count = -1, // -1 表示无限次
        bool debug = false);

    bool ShouldTrigger(TimePoint now) const;
    void Trigger(TimePoint now);
    bool IsExpired() const;
    void Cancel();

    const std::string& Name() const { return name_; }
    int TriggerCount() const { return trigger_count_; }

private:
    std::string name_;
    std::function<void()> callback_;
    int interval_ms_;
    int max_trigger_count_;
    bool debug_;
    int trigger_count_;
    TimePoint last_trigger_time_;
    bool cancelled_;
};
