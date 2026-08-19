#ifndef RECONNECT_CONTROLLER_H
#define RECONNECT_CONTROLLER_H

#include <cstdint>
#include <functional>

struct ReconnectPolicy {
    uint32_t initial_delay_ms = 500;
    uint32_t maximum_delay_ms = 30000;
    uint8_t jitter_percent = 20;
};

class ReconnectController {
public:
    using Clock = std::function<uint64_t()>;
    using Random = std::function<uint32_t()>;

    ReconnectController(Clock clock, Random random, ReconnectPolicy policy = {});

    bool Ready() const;
    uint32_t RecordFailure();
    void RecordSuccess();
    uint32_t attempt() const { return attempt_; }
    uint64_t next_attempt_time_ms() const { return next_attempt_time_ms_; }

private:
    Clock clock_;
    Random random_;
    ReconnectPolicy policy_;
    uint32_t attempt_ = 0;
    uint64_t next_attempt_time_ms_ = 0;
};

#endif  // RECONNECT_CONTROLLER_H
