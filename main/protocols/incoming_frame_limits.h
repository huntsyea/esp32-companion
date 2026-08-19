#ifndef INCOMING_FRAME_LIMITS_H
#define INCOMING_FRAME_LIMITS_H

#include <cstddef>

inline bool IsValidIncomingPayload(size_t payload_size, size_t maximum_size) {
    return payload_size > 0 && payload_size <= maximum_size;
}

inline bool IsValidIncomingFramedPayload(size_t frame_size, size_t header_size, size_t payload_size,
                                         size_t maximum_size) {
    return frame_size >= header_size && IsValidIncomingPayload(payload_size, maximum_size) &&
           payload_size == frame_size - header_size;
}

#endif  // INCOMING_FRAME_LIMITS_H
