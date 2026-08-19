#include "reconnect_controller.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

ReconnectController::ReconnectController(Clock clock, Random random, ReconnectPolicy policy)
    : clock_(std::move(clock)), random_(std::move(random)), policy_(policy) {
    if (!clock_ || !random_ || policy_.initial_delay_ms == 0 || policy_.maximum_delay_ms == 0 ||
        policy_.initial_delay_ms > policy_.maximum_delay_ms || policy_.jitter_percent > 100) {
        throw std::invalid_argument("invalid_reconnect_policy");
    }
}

bool ReconnectController::Ready() const { return clock_() >= next_attempt_time_ms_; }

uint32_t ReconnectController::RecordFailure() {
    uint64_t base_delay = policy_.initial_delay_ms;
    for (uint32_t index = 0; index < attempt_ && base_delay < policy_.maximum_delay_ms; ++index) {
        base_delay = std::min<uint64_t>(base_delay * 2, policy_.maximum_delay_ms);
    }

    const uint64_t jitter_range = base_delay * policy_.jitter_percent / 100;
    const double unit =
        static_cast<double>(random_()) / static_cast<double>(std::numeric_limits<uint32_t>::max());
    const int64_t signed_jitter =
        static_cast<int64_t>(unit * static_cast<double>(jitter_range * 2)) -
        static_cast<int64_t>(jitter_range);
    const uint32_t delay = static_cast<uint32_t>(static_cast<int64_t>(base_delay) + signed_jitter);

    if (attempt_ < 31) {
        ++attempt_;
    }
    next_attempt_time_ms_ = clock_() + delay;
    return delay;
}

void ReconnectController::RecordSuccess() {
    attempt_ = 0;
    next_attempt_time_ms_ = 0;
}
