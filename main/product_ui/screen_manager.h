#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "clock_screen.h"
#include "product_ui.h"
#include "status_screen.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCREEN_MANAGER_IDLE_RETURN_DELAY_MS 1500U

typedef struct screen_manager screen_manager_t;

typedef enum {
    SCREEN_MANAGER_SCREEN_AVATAR,
    SCREEN_MANAGER_SCREEN_CLOCK,
    SCREEN_MANAGER_SCREEN_STATUS,
} screen_manager_screen_t;

typedef enum {
    SCREEN_MANAGER_SWIPE_LEFT,
    SCREEN_MANAGER_SWIPE_RIGHT,
} screen_manager_swipe_t;

/**
 * Create the sole owner of Avatar, Clock, and Status Screen visibility and
 * selection.
 *
 * All screens must already exist under the same LVGL parent. The initial
 * remembered and visible screen is the Avatar Screen.
 */
screen_manager_t* screen_manager_create(product_ui_t* product_ui, clock_screen_t* clock_screen,
                                        status_screen_t* status_screen);

/**
 * Apply a non-wrapping horizontal swipe.
 *
 * Swipes are ignored at the first or last screen, during push-to-talk, and
 * while conversation activity temporarily forces the Avatar Screen.
 */
bool screen_manager_handle_swipe(screen_manager_t* manager, screen_manager_swipe_t swipe);

/** Prevent navigation gestures while a push-to-talk control owns the touch action. */
void screen_manager_set_push_to_talk_active(screen_manager_t* manager, bool active);

/** Apply semantic conversation state without exposing LVGL to the caller. */
void screen_manager_set_conversation_state(screen_manager_t* manager, product_ui_state_t state);

/** Dispatch one semantic event and update Product UI state and screen focus atomically. */
bool screen_manager_dispatch_event(screen_manager_t* manager, product_ui_event_t event);

/** Advance the deterministic manager clock used for the delayed idle return. */
void screen_manager_update_time(screen_manager_t* manager, uint32_t time_ms);

/** Return the last screen explicitly selected by a valid user swipe. */
screen_manager_screen_t screen_manager_get_selected_screen(const screen_manager_t* manager);

/** Return the screen currently presented to the user. */
screen_manager_screen_t screen_manager_get_visible_screen(const screen_manager_t* manager);

/** Return true while an idle return delay is pending. */
bool screen_manager_is_return_pending(const screen_manager_t* manager);

/** Return true while push-to-talk blocks navigation. */
bool screen_manager_is_push_to_talk_active(const screen_manager_t* manager);

/** Return a stable diagnostic name for a screen. */
const char* screen_manager_screen_name(screen_manager_screen_t screen);

#ifdef __cplusplus
}
#endif

#endif  // SCREEN_MANAGER_H
