#ifndef CLOCK_SCREEN_H
#define CLOCK_SCREEN_H

#include "companion_time.h"

#include <lvgl.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clock_screen clock_screen_t;

typedef struct {
    char time[6];
    char period[3];
    char weekday[4];
    char date[13];
    bool is_dst;
    bool synchronized;
} clock_screen_model_t;

/** Create an interaction-transparent 240 by 240 clock surface. */
clock_screen_t* clock_screen_create(lv_obj_t* parent);

/** Return the root object for screen selection and deterministic snapshots. */
lv_obj_t* clock_screen_get_root(const clock_screen_t* screen);

/** Update existing LVGL objects. Equal minute values cause no redraw. */
bool clock_screen_update(clock_screen_t* screen, const companion_local_time_t* local_time);

/** Format local calendar fields without depending on the configured time zone. */
bool clock_screen_format(const companion_local_time_t* local_time, clock_screen_model_t* model);

/** Return the last model displayed by the clock. */
const clock_screen_model_t* clock_screen_get_model(const clock_screen_t* screen);

#ifdef __cplusplus
}
#endif

#endif  // CLOCK_SCREEN_H
