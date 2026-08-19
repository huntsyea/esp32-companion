#include "companion_time.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

bool companion_time_configure(const char* timezone_name) {
    if (timezone_name == NULL || strcmp(timezone_name, COMPANION_TIMEZONE_NAME) != 0) {
        return false;
    }
    if (setenv("TZ", COMPANION_TIMEZONE_POSIX, 1) != 0) {
        return false;
    }
    tzset();
    return true;
}

bool companion_time_from_epoch(int64_t epoch_seconds, bool synchronized,
                               companion_local_time_t* local_time) {
    if (local_time == NULL) {
        return false;
    }

    *local_time = (companion_local_time_t){.synchronized = synchronized};
    if (!synchronized) {
        return true;
    }

    const time_t epoch = (time_t)epoch_seconds;
    struct tm calendar_time;
    if ((int64_t)epoch != epoch_seconds || localtime_r(&epoch, &calendar_time) == NULL) {
        local_time->synchronized = false;
        return false;
    }

    local_time->year = (int16_t)(calendar_time.tm_year + 1900);
    local_time->month = (uint8_t)(calendar_time.tm_mon + 1);
    local_time->day = (uint8_t)calendar_time.tm_mday;
    local_time->weekday = (uint8_t)calendar_time.tm_wday;
    local_time->hour = (uint8_t)calendar_time.tm_hour;
    local_time->minute = (uint8_t)calendar_time.tm_min;
    local_time->is_dst = calendar_time.tm_isdst > 0;
    return true;
}
