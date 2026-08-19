#ifndef AUDIO_DOWNLINK_GATE_H
#define AUDIO_DOWNLINK_GATE_H

#include <atomic>

class AudioDownlinkGate {
public:
    void Start() { active_.store(true); }
    void Stop() { active_.store(false); }
    bool Accept(bool session_is_current) const { return session_is_current && active_.load(); }

private:
    std::atomic<bool> active_{false};
};

#endif  // AUDIO_DOWNLINK_GATE_H
