#include "cwTimer.h"
#include <iostream>

cwTimer::cwTimer(const std::string& name,
    std::function<void()> callback,
    int interval_ms,
    int max_trigger_count,
    bool debug)
    : name_(name),
    callback_(callback),
    interval_ms_(interval_ms),
    max_trigger_count_(max_trigger_count),
    debug_(debug),
    trigger_count_(0),
    last_trigger_time_(std::chrono::steady_clock::now()),
    cancelled_(false)
{}

bool cwTimer::ShouldTrigger(TimePoint now) const {
    if (cancelled_ || IsExpired()) return false;
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_trigger_time_).count() >= interval_ms_;
}

void cwTimer::Trigger(TimePoint now) {
    if (cancelled_ || IsExpired()) return;
    if (debug_) {
        std::cout << "[Timer] Triggering: " << name_ << " (count = " << trigger_count_ << ")\n";
    }
    callback_();
    ++trigger_count_;
    last_trigger_time_ = now;
}

bool cwTimer::IsExpired() const {
    return (max_trigger_count_ >= 0) && (trigger_count_ >= max_trigger_count_);
}

void cwTimer::Cancel() {
    cancelled_ = true;
    if (debug_) {
        std::cout << "[Timer] Cancelled: " << name_ << "\n";
    }
}
