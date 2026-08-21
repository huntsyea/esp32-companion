#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#include <lvgl.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct status_screen status_screen_t;

/** Create an interaction-transparent 240 by 240 connection status surface. */
status_screen_t* status_screen_create(lv_obj_t* parent);

/** Return the root object for screen selection and deterministic snapshots. */
lv_obj_t* status_screen_get_root(const status_screen_t* screen);

/**
 * Update the connection status line.
 *
 * wifi_connected reflects the network link; hermes_connected reflects the
 * WebSocket/Hermes session. Passing both false clears the line.
 */
void status_screen_set_connection_status(status_screen_t* screen, bool wifi_connected,
                                         bool hermes_connected);

#ifdef __cplusplus
}
#endif

#endif  // STATUS_SCREEN_H
