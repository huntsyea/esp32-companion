#ifndef CAPTURE_DRAIN_GATE_H
#define CAPTURE_DRAIN_GATE_H

#include <cstdint>

class CaptureDrainGate {
public:
    void Begin(uint32_t interaction_epoch) {
        pending_ = true;
        interaction_epoch_ = interaction_epoch;
    }

    void Cancel() { pending_ = false; }

    bool Ready(uint32_t interaction_epoch, bool capture_idle) const {
        return pending_ && capture_idle && interaction_epoch == interaction_epoch_;
    }

    void Complete() { pending_ = false; }

private:
    uint32_t interaction_epoch_ = 0;
    bool pending_ = false;
};

#endif  // CAPTURE_DRAIN_GATE_H
