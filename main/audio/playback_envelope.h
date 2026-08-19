#ifndef PLAYBACK_ENVELOPE_H
#define PLAYBACK_ENVELOPE_H

#include <cstddef>
#include <cstdint>

inline uint16_t CalculatePlaybackEnvelope(const int16_t* pcm, size_t sample_count) {
    uint32_t peak = 0;
    for (size_t index = 0; pcm != nullptr && index < sample_count; ++index) {
        const int32_t sample = pcm[index];
        const uint32_t magnitude =
            sample < 0 ? static_cast<uint32_t>(-sample) : static_cast<uint32_t>(sample);
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    const uint32_t level = (peak * 1000U + 16383U) / 32767U;
    return static_cast<uint16_t>(level > 1000U ? 1000U : level);
}

#endif  // PLAYBACK_ENVELOPE_H
