#ifndef COMPANION_TIME_H
#define COMPANION_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMPANION_TIMEZONE_NAME "America/Chicago"
#define COMPANION_TIMEZONE_POSIX "CST6CDT,M3.2.0/2,M11.1.0/2"

typedef struct {
    int16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    bool is_dst;
    bool synchronized;
} companion_local_time_t;

/** Map a supported IANA name to the process-local POSIX time-zone rule. */
bool companion_time_configure(const char* timezone_name);

/** Convert a UTC system epoch to local calendar fields. */
bool companion_time_from_epoch(int64_t epoch_seconds, bool synchronized,
                               companion_local_time_t* local_time);

#ifdef __cplusplus
}
#endif

#endif  // COMPANION_TIME_H
