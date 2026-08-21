#ifndef PRODUCT_UI_H
#define PRODUCT_UI_H

#include <lvgl.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct product_ui product_ui_t;

#define PRODUCT_UI_TRANSCRIPT_MAX_BYTES 96U

typedef enum {
    PRODUCT_UI_STATE_OFFLINE,
    PRODUCT_UI_STATE_IDLE,
    PRODUCT_UI_STATE_LISTENING,
    PRODUCT_UI_STATE_THINKING,
    PRODUCT_UI_STATE_SPEAKING,
    PRODUCT_UI_STATE_ERROR,
} product_ui_state_t;

typedef enum {
    PRODUCT_UI_EVENT_CONNECTED,
    PRODUCT_UI_EVENT_LISTEN_STARTED,
    PRODUCT_UI_EVENT_RECORDING_ENDED,
    PRODUCT_UI_EVENT_TTS_STARTED,
    PRODUCT_UI_EVENT_TTS_STOPPED,
    PRODUCT_UI_EVENT_CANCELLED,
    PRODUCT_UI_EVENT_DISCONNECTED,
    PRODUCT_UI_EVENT_TEMPORARY_ERROR,
    PRODUCT_UI_EVENT_ERROR_RECOVERED,
} product_ui_event_t;

/**
 * Create the platform-independent Product UI on the supplied LVGL parent.
 *
 * LVGL must be initialized before this function is called. The caller must
 * hold the platform LVGL lock when its display adapter requires one. The UI
 * starts offline and owns a non-blocking LVGL animation timer.
 *
 * @return An opaque Product UI context, or NULL when parent is NULL or memory
 *         allocation fails.
 */
product_ui_t* product_ui_create(lv_obj_t* parent);

/** Return the Product UI root object for screen management and snapshots. */
lv_obj_t* product_ui_get_root(const product_ui_t* ui);

/**
 * Apply one semantic conversation event. Duplicate or invalid events are ignored.
 *
 * Returns true if the event changed any visible UI state (conversation state and/or
 * the persistent connection indicators), false if nothing changed. Callers that only
 * care about a conversation-state transition should compare product_ui_get_state()
 * before/after instead of relying on the boolean alone.
 */
bool product_ui_dispatch(product_ui_t* ui, product_ui_event_t event);

/** Return the current single source of truth for visible conversation state. */
product_ui_state_t product_ui_get_state(const product_ui_t* ui);

/** Return a stable diagnostic name for a conversation state. */
const char* product_ui_state_name(product_ui_state_t state);

/**
 * Update the persistent network indicator shown as the "WIFI" label.
 *
 * This tracks the network/radio link only; it is independent of the Hermes session
 * connection (product_ui_is_hermes_connected). "wifi" here is the label used on the
 * companion board and may also represent wired/cellular connectivity elsewhere.
 */
void product_ui_set_wifi_connected(product_ui_t* ui, bool connected);

/** Return whether the UI currently reports Wi-Fi as connected. */
bool product_ui_is_wifi_connected(const product_ui_t* ui);

/** Return whether the UI currently reports Hermes as connected. */
bool product_ui_is_hermes_connected(const product_ui_t* ui);

/**
 * Set decoded playback level from 0 (silent) to 1000 (full scale).
 *
 * The Product UI applies bounded attack and release smoothing locally. Values
 * above 1000 are clamped. Levels affect motion only while speaking.
 */
void product_ui_set_playback_envelope(product_ui_t* ui, uint16_t level_milli);

/** Return the locally smoothed playback level for diagnostics. */
uint16_t product_ui_get_smoothed_envelope(const product_ui_t* ui);

/** Show bounded transcript text on the Avatar Screen. NULL clears it. */
void product_ui_set_transcript(product_ui_t* ui, const char* transcript);

/** Return the bounded transcript model for diagnostics and host tests. */
const char* product_ui_get_transcript(const product_ui_t* ui);

/**
 * Render a deterministic animation time in milliseconds.
 *
 * Production uses the internal LVGL timer. The simulator and tests can inject
 * an exact time through this function without sleeping.
 */
void product_ui_set_animation_time(product_ui_t* ui, uint32_t time_ms);

#ifdef __cplusplus
}
#endif

#endif  // PRODUCT_UI_H
