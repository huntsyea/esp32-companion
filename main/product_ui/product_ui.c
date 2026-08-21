#include "product_ui.h"

#include "product_identity.h"
#include "view_helpers.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PRODUCT_UI_ANIMATION_PERIOD_MS 33
#define PRODUCT_UI_ENVELOPE_MAX 1000
#define PRODUCT_UI_ENVELOPE_ATTACK_PER_MS 4
#define PRODUCT_UI_ENVELOPE_RELEASE_PER_MS 2

#define PRODUCT_UI_LISTENING_COLOR 0x3972A6
#define PRODUCT_UI_THINKING_COLOR 0x4E83AA
#define PRODUCT_UI_OFFLINE_COLOR 0x74838A
#define PRODUCT_UI_ERROR_COLOR 0xD94A4A
#define PRODUCT_UI_CONNECTED_COLOR 0x248A5A
#define PRODUCT_UI_DISCONNECTED_COLOR 0xA04747

struct product_ui {
    lv_obj_t* root;
    lv_obj_t* body;
    lv_obj_t* eyes[2];
    lv_obj_t* side_marks[4];
    lv_obj_t* top_marks[3];
    lv_obj_t* wifi_status_label;
    lv_obj_t* hermes_status_label;
    lv_obj_t* transcript_label;
    lv_timer_t* animation_timer;
    product_ui_state_t state;
    product_ui_state_t recovery_state;
    bool wifi_connected;
    bool hermes_connected;
    uint32_t timer_epoch_ms;
    uint32_t animation_time_ms;
    uint16_t envelope_target;
    uint16_t envelope_smoothed;
    char transcript[PRODUCT_UI_TRANSCRIPT_MAX_BYTES + 1U];
};

static void render(product_ui_t* ui);

static void render_connection_status(product_ui_t* ui) {
    lv_label_set_text(ui->wifi_status_label, ui->wifi_connected ? "WIFI ON" : "WIFI OFF");
    lv_obj_set_style_text_color(
        ui->wifi_status_label,
        lv_color_hex(ui->wifi_connected ? PRODUCT_UI_CONNECTED_COLOR
                                        : PRODUCT_UI_DISCONNECTED_COLOR),
        LV_PART_MAIN);
    lv_label_set_text(ui->hermes_status_label,
                      ui->hermes_connected ? "HERMES ON" : "HERMES OFF");
    lv_obj_set_style_text_color(
        ui->hermes_status_label,
        lv_color_hex(ui->hermes_connected ? PRODUCT_UI_CONNECTED_COLOR
                                          : PRODUCT_UI_DISCONNECTED_COLOR),
        LV_PART_MAIN);
}

static lv_obj_t* create_eye(lv_obj_t* parent, int32_t x, int32_t y, int32_t width, int32_t height) {
    lv_obj_t* eye = product_ui_view_create_shape(
        parent, x, y, width, height, PRODUCT_IDENTITY_EYE_COLOR, PRODUCT_IDENTITY_EYE_RADIUS);
    lv_obj_set_style_transform_pivot_x(eye, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(eye, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(eye, PRODUCT_IDENTITY_EYE_ROTATION, LV_PART_MAIN);
    return eye;
}

static void hide_state_marks(product_ui_t* ui) {
    for (size_t index = 0; index < 4; ++index) {
        product_ui_view_set_visible(ui->side_marks[index], false);
    }
    for (size_t index = 0; index < 3; ++index) {
        product_ui_view_set_visible(ui->top_marks[index], false);
    }
}

static int32_t triangle_wave(uint32_t time_ms, uint32_t period_ms, int32_t amplitude) {
    const uint32_t phase = time_ms % period_ms;
    const uint32_t half_period = period_ms / 2U;
    if (phase <= half_period) {
        return (int32_t)((phase * (uint32_t)amplitude) / half_period);
    }
    return (int32_t)(((period_ms - phase) * (uint32_t)amplitude) / half_period);
}

static int32_t idle_bob(uint32_t time_ms) {
    const uint32_t phase = time_ms % 3400U;
    if (phase < 850U) {
        return -(int32_t)((phase * 2U) / 850U);
    }
    if (phase < 1700U) {
        return -2 + (int32_t)(((phase - 850U) * 2U) / 850U);
    }
    if (phase < 2550U) {
        return (int32_t)(((phase - 1700U) * 2U) / 850U);
    }
    return 2 - (int32_t)(((phase - 2550U) * 2U) / 850U);
}

static int32_t idle_glance(uint32_t time_ms) {
    const uint32_t phase = time_ms % 7000U;
    if (phase >= 2200U && phase < 2800U) {
        return (int32_t)(((phase - 2200U) * 4U) / 600U);
    }
    if (phase >= 2800U && phase < 3600U) {
        return 4;
    }
    if (phase >= 3600U && phase < 4200U) {
        return 4 - (int32_t)(((phase - 3600U) * 4U) / 600U);
    }
    return 0;
}

static int32_t idle_eye_height(uint32_t time_ms) {
    const uint32_t phase = time_ms % 4800U;
    if (phase >= 4300U && phase < 4400U) {
        return PRODUCT_IDENTITY_LEFT_EYE_HEIGHT - (int32_t)(((phase - 4300U) * 32U) / 100U);
    }
    if (phase >= 4400U && phase < 4500U) {
        return 6 + (int32_t)(((phase - 4400U) * 32U) / 100U);
    }
    return PRODUCT_IDENTITY_LEFT_EYE_HEIGHT;
}

static void set_mark(lv_obj_t* mark, int32_t x, int32_t y, int32_t width, int32_t height,
                     uint32_t color, int32_t radius, int32_t rotation) {
    lv_obj_set_pos(mark, x, y);
    lv_obj_set_size(mark, width, height);
    lv_obj_set_style_bg_color(mark, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(mark, radius, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(mark, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(mark, LV_PCT(50), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(mark, rotation, LV_PART_MAIN);
    product_ui_view_set_visible(mark, true);
}

static void render_idle(product_ui_t* ui) {
    const int32_t breathe_width = triangle_wave(ui->animation_time_ms, 2800U, 4);
    const int32_t breathe_height = triangle_wave(ui->animation_time_ms, 2800U, 2);
    const int32_t bob = idle_bob(ui->animation_time_ms);
    const int32_t glance = idle_glance(ui->animation_time_ms);
    const int32_t eye_height = idle_eye_height(ui->animation_time_ms);
    const int32_t eye_y =
        PRODUCT_IDENTITY_LEFT_EYE_Y + bob + ((PRODUCT_IDENTITY_LEFT_EYE_HEIGHT - eye_height) / 2);

    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X - (breathe_width / 2),
                   PRODUCT_IDENTITY_BODY_Y + bob + (breathe_height / 2));
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH + breathe_width,
                    PRODUCT_IDENTITY_BODY_HEIGHT - breathe_height);
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X + glance, eye_y);
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH, eye_height);
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X + glance, eye_y);
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH, eye_height);
}

static void render_listening(product_ui_t* ui) {
    const int32_t pulse = triangle_wave(ui->animation_time_ms, 1000U, 10);
    const int32_t outer_height = 14 + pulse;
    const int32_t inner_height = 24 + pulse;

    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X - 2, PRODUCT_IDENTITY_BODY_Y - 2);
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH + 4, PRODUCT_IDENTITY_BODY_HEIGHT + 4);
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X - 2, PRODUCT_IDENTITY_LEFT_EYE_Y - 4);
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH + 2,
                    PRODUCT_IDENTITY_LEFT_EYE_HEIGHT + 8);
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X, PRODUCT_IDENTITY_RIGHT_EYE_Y - 4);
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH + 2,
                    PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT + 8);

    set_mark(ui->side_marks[0], 19, 123 - (outer_height / 2), 5, outer_height,
             PRODUCT_UI_LISTENING_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[1], 27, 123 - (inner_height / 2), 5, inner_height,
             PRODUCT_UI_LISTENING_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[2], 208, 123 - (inner_height / 2), 5, inner_height,
             PRODUCT_UI_LISTENING_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[3], 216, 123 - (outer_height / 2), 5, outer_height,
             PRODUCT_UI_LISTENING_COLOR, LV_RADIUS_CIRCLE, 0);
}

static void render_thinking(product_ui_t* ui) {
    const uint32_t active_dot = (ui->animation_time_ms / 300U) % 3U;
    const int32_t glance = ((int32_t)active_dot - 1) * 4;

    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X, PRODUCT_IDENTITY_BODY_Y);
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH, PRODUCT_IDENTITY_BODY_HEIGHT);
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X + glance,
                   PRODUCT_IDENTITY_LEFT_EYE_Y + 4);
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH,
                    PRODUCT_IDENTITY_LEFT_EYE_HEIGHT - 8);
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X + glance,
                   PRODUCT_IDENTITY_RIGHT_EYE_Y + 4);
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH,
                    PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT - 8);

    for (size_t index = 0; index < 3; ++index) {
        const int32_t size = index == active_dot ? 13 : 9;
        const int32_t center_x = 99 + ((int32_t)index * 21);
        set_mark(ui->top_marks[index], center_x - (size / 2), 51 - (size / 2), size, size,
                 index == active_dot ? PRODUCT_IDENTITY_EYE_COLOR : PRODUCT_UI_THINKING_COLOR,
                 LV_RADIUS_CIRCLE, 0);
    }
}

static void render_speaking(product_ui_t* ui) {
    const int32_t level = ui->envelope_smoothed;
    const int32_t growth = (level * 16) / PRODUCT_UI_ENVELOPE_MAX;
    const int32_t width_growth = (level * 6) / PRODUCT_UI_ENVELOPE_MAX;
    const int32_t bar_height = 5 + ((level * 28) / PRODUCT_UI_ENVELOPE_MAX);

    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X - (width_growth / 2),
                   PRODUCT_IDENTITY_BODY_Y - (growth / 2));
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH + width_growth,
                    PRODUCT_IDENTITY_BODY_HEIGHT + growth);
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X,
                   PRODUCT_IDENTITY_LEFT_EYE_Y - (growth / 4));
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH,
                    PRODUCT_IDENTITY_LEFT_EYE_HEIGHT + (growth / 2));
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X,
                   PRODUCT_IDENTITY_RIGHT_EYE_Y - (growth / 4));
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH,
                    PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT + (growth / 2));

    set_mark(ui->side_marks[0], 18, 123 - (bar_height / 2), 5, bar_height,
             PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[1], 26, 123 - ((bar_height + 6) / 2), 5, bar_height + 6,
             PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[2], 209, 123 - ((bar_height + 6) / 2), 5, bar_height + 6,
             PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->side_marks[3], 217, 123 - (bar_height / 2), 5, bar_height,
             PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE, 0);
}

static void render_offline(product_ui_t* ui) {
    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X, PRODUCT_IDENTITY_BODY_Y + 6);
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH, PRODUCT_IDENTITY_BODY_HEIGHT);
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X, PRODUCT_IDENTITY_LEFT_EYE_Y + 20);
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH, 6);
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X, PRODUCT_IDENTITY_RIGHT_EYE_Y + 20);
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH, 6);

    set_mark(ui->top_marks[0], 106, 48, 29, 5, PRODUCT_UI_OFFLINE_COLOR, LV_RADIUS_CIRCLE, 450);
    set_mark(ui->top_marks[1], 106, 48, 29, 5, PRODUCT_UI_OFFLINE_COLOR, LV_RADIUS_CIRCLE, -450);
}

static void render_error(product_ui_t* ui) {
    const int32_t pulse = triangle_wave(ui->animation_time_ms, 800U, 3);

    lv_obj_set_pos(ui->body, PRODUCT_IDENTITY_BODY_X - pulse, PRODUCT_IDENTITY_BODY_Y - pulse);
    lv_obj_set_size(ui->body, PRODUCT_IDENTITY_BODY_WIDTH + (pulse * 2),
                    PRODUCT_IDENTITY_BODY_HEIGHT + (pulse * 2));
    lv_obj_set_pos(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_X - 3, PRODUCT_IDENTITY_LEFT_EYE_Y + 6);
    lv_obj_set_size(ui->eyes[0], PRODUCT_IDENTITY_LEFT_EYE_WIDTH,
                    PRODUCT_IDENTITY_LEFT_EYE_HEIGHT - 12);
    lv_obj_set_pos(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_X + 3, PRODUCT_IDENTITY_RIGHT_EYE_Y + 6);
    lv_obj_set_size(ui->eyes[1], PRODUCT_IDENTITY_RIGHT_EYE_WIDTH,
                    PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT - 12);

    set_mark(ui->top_marks[0], 117, 38, 7, 24, PRODUCT_UI_ERROR_COLOR, LV_RADIUS_CIRCLE, 0);
    set_mark(ui->top_marks[1], 117, 67, 7, 7, PRODUCT_UI_ERROR_COLOR, LV_RADIUS_CIRCLE, 0);
}

static void render(product_ui_t* ui) {
    hide_state_marks(ui);
    switch (ui->state) {
        case PRODUCT_UI_STATE_OFFLINE:
            render_offline(ui);
            break;
        case PRODUCT_UI_STATE_IDLE:
            render_idle(ui);
            break;
        case PRODUCT_UI_STATE_LISTENING:
            render_listening(ui);
            break;
        case PRODUCT_UI_STATE_THINKING:
            render_thinking(ui);
            break;
        case PRODUCT_UI_STATE_SPEAKING:
            render_speaking(ui);
            break;
        case PRODUCT_UI_STATE_ERROR:
            render_error(ui);
            break;
    }
}

static void set_state(product_ui_t* ui, product_ui_state_t state) {
    const product_ui_state_t previous_state = ui->state;
    ui->state = state;
    ui->envelope_target = 0;
    ui->envelope_smoothed = 0;
    render(ui);
    if (ui->animation_timer == NULL) {
        return;
    }
    if (state == PRODUCT_UI_STATE_OFFLINE) {
        lv_timer_pause(ui->animation_timer);
    } else if (previous_state == PRODUCT_UI_STATE_OFFLINE) {
        ui->timer_epoch_ms = lv_tick_get();
        ui->animation_time_ms = 0;
        lv_timer_resume(ui->animation_timer);
    }
}

static void on_animation_timer(lv_timer_t* timer) {
    product_ui_t* ui = lv_timer_get_user_data(timer);
    product_ui_set_animation_time(ui, lv_tick_elaps(ui->timer_epoch_ms));
}

static void on_root_deleted(lv_event_t* event) {
    product_ui_t* ui = lv_event_get_user_data(event);
    if (ui->animation_timer != NULL) {
        lv_timer_delete(ui->animation_timer);
        ui->animation_timer = NULL;
    }
    lv_free(ui);
}

product_ui_t* product_ui_create(lv_obj_t* parent) {
    if (parent == NULL) {
        return NULL;
    }

    product_ui_t* ui = lv_malloc_zeroed(sizeof(*ui));
    if (ui == NULL) {
        return NULL;
    }

    ui->root = lv_obj_create(parent);
    lv_obj_set_size(ui->root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(ui->root, LV_ALIGN_CENTER, 0, 0);
    product_ui_view_apply_surface_style(ui->root, PRODUCT_IDENTITY_BACKGROUND_COLOR, 0);

    ui->body = product_ui_view_create_shape(
        ui->root, PRODUCT_IDENTITY_BODY_X, PRODUCT_IDENTITY_BODY_Y, PRODUCT_IDENTITY_BODY_WIDTH,
        PRODUCT_IDENTITY_BODY_HEIGHT, PRODUCT_IDENTITY_BODY_COLOR, LV_RADIUS_CIRCLE);
    ui->eyes[0] = create_eye(ui->root, PRODUCT_IDENTITY_LEFT_EYE_X, PRODUCT_IDENTITY_LEFT_EYE_Y,
                             PRODUCT_IDENTITY_LEFT_EYE_WIDTH, PRODUCT_IDENTITY_LEFT_EYE_HEIGHT);
    ui->eyes[1] = create_eye(ui->root, PRODUCT_IDENTITY_RIGHT_EYE_X, PRODUCT_IDENTITY_RIGHT_EYE_Y,
                             PRODUCT_IDENTITY_RIGHT_EYE_WIDTH, PRODUCT_IDENTITY_RIGHT_EYE_HEIGHT);

    for (size_t index = 0; index < 4; ++index) {
        ui->side_marks[index] = product_ui_view_create_shape(
            ui->root, 0, 0, 1, 1, PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE);
    }
    for (size_t index = 0; index < 3; ++index) {
        ui->top_marks[index] = product_ui_view_create_shape(
            ui->root, 0, 0, 1, 1, PRODUCT_IDENTITY_EYE_COLOR, LV_RADIUS_CIRCLE);
    }
    ui->wifi_status_label = lv_label_create(ui->root);
    lv_obj_set_pos(ui->wifi_status_label, 12, 14);
    lv_obj_set_size(ui->wifi_status_label, 90, 18);
    product_ui_view_make_event_transparent(ui->wifi_status_label);

    ui->hermes_status_label = lv_label_create(ui->root);
    lv_obj_set_pos(ui->hermes_status_label, 108, 14);
    lv_obj_set_size(ui->hermes_status_label, 120, 18);
    lv_obj_set_style_text_align(ui->hermes_status_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    product_ui_view_make_event_transparent(ui->hermes_status_label);

    ui->transcript_label = lv_label_create(ui->root);
    lv_obj_set_pos(ui->transcript_label, 15, 210);
    lv_obj_set_size(ui->transcript_label, 210, 22);
    lv_label_set_long_mode(ui->transcript_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(ui->transcript_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->transcript_label, lv_color_hex(PRODUCT_IDENTITY_EYE_COLOR),
                                LV_PART_MAIN);
    product_ui_view_make_event_transparent(ui->transcript_label);
    product_ui_view_set_visible(ui->transcript_label, false);

    ui->state = PRODUCT_UI_STATE_OFFLINE;
    ui->recovery_state = PRODUCT_UI_STATE_OFFLINE;
    ui->timer_epoch_ms = lv_tick_get();
    render_connection_status(ui);
    render(ui);
    lv_obj_add_event_cb(ui->root, on_root_deleted, LV_EVENT_DELETE, ui);
    ui->animation_timer = lv_timer_create(on_animation_timer, PRODUCT_UI_ANIMATION_PERIOD_MS, ui);
    if (ui->animation_timer == NULL) {
        lv_obj_delete(ui->root);
        return NULL;
    }
    lv_timer_pause(ui->animation_timer);
    return ui;
}

lv_obj_t* product_ui_get_root(const product_ui_t* ui) { return ui == NULL ? NULL : ui->root; }

bool product_ui_dispatch(product_ui_t* ui, product_ui_event_t event) {
    if (ui == NULL) {
        return false;
    }

    product_ui_state_t next_state = ui->state;
    bool connection_changed = false;
    switch (event) {
        case PRODUCT_UI_EVENT_CONNECTED:
            connection_changed = !ui->hermes_connected;
            ui->hermes_connected = true;
            render_connection_status(ui);
            if (ui->state == PRODUCT_UI_STATE_OFFLINE) {
                next_state = PRODUCT_UI_STATE_IDLE;
            }
            break;
        case PRODUCT_UI_EVENT_LISTEN_STARTED:
            if (ui->state == PRODUCT_UI_STATE_IDLE) {
                product_ui_set_transcript(ui, NULL);
                next_state = PRODUCT_UI_STATE_LISTENING;
            }
            break;
        case PRODUCT_UI_EVENT_RECORDING_ENDED:
            if (ui->state == PRODUCT_UI_STATE_LISTENING) {
                next_state = PRODUCT_UI_STATE_THINKING;
            }
            break;
        case PRODUCT_UI_EVENT_TTS_STARTED:
            if (ui->state == PRODUCT_UI_STATE_THINKING) {
                next_state = PRODUCT_UI_STATE_SPEAKING;
            }
            break;
        case PRODUCT_UI_EVENT_TTS_STOPPED:
            if (ui->state == PRODUCT_UI_STATE_SPEAKING) {
                next_state = PRODUCT_UI_STATE_IDLE;
            }
            break;
        case PRODUCT_UI_EVENT_CANCELLED:
            product_ui_set_transcript(ui, NULL);
            if (ui->state == PRODUCT_UI_STATE_LISTENING || ui->state == PRODUCT_UI_STATE_THINKING ||
                ui->state == PRODUCT_UI_STATE_SPEAKING ||
                (ui->state == PRODUCT_UI_STATE_ERROR &&
                 ui->recovery_state != PRODUCT_UI_STATE_OFFLINE)) {
                next_state = PRODUCT_UI_STATE_IDLE;
            } else if (ui->state == PRODUCT_UI_STATE_ERROR) {
                next_state = PRODUCT_UI_STATE_OFFLINE;
            }
            break;
        case PRODUCT_UI_EVENT_DISCONNECTED:
            connection_changed = ui->hermes_connected;
            ui->hermes_connected = false;
            render_connection_status(ui);
            product_ui_set_transcript(ui, NULL);
            next_state = PRODUCT_UI_STATE_OFFLINE;
            break;
        case PRODUCT_UI_EVENT_TEMPORARY_ERROR:
            product_ui_set_transcript(ui, NULL);
            if (ui->state != PRODUCT_UI_STATE_ERROR) {
                ui->recovery_state = ui->state;
                next_state = PRODUCT_UI_STATE_ERROR;
            }
            break;
        case PRODUCT_UI_EVENT_ERROR_RECOVERED:
            if (ui->state == PRODUCT_UI_STATE_ERROR) {
                next_state = ui->recovery_state;
            }
            break;
    }

    if (next_state == ui->state) {
        // No conversation-state transition, but a connection indicator may still
        // have changed (see product_ui_dispatch() return contract).
        return connection_changed;
    }
    set_state(ui, next_state);
    return true;
}

product_ui_state_t product_ui_get_state(const product_ui_t* ui) {
    return ui == NULL ? PRODUCT_UI_STATE_OFFLINE : ui->state;
}

const char* product_ui_state_name(product_ui_state_t state) {
    switch (state) {
        case PRODUCT_UI_STATE_OFFLINE:
            return "offline";
        case PRODUCT_UI_STATE_IDLE:
            return "idle";
        case PRODUCT_UI_STATE_LISTENING:
            return "listening";
        case PRODUCT_UI_STATE_THINKING:
            return "thinking";
        case PRODUCT_UI_STATE_SPEAKING:
            return "speaking";
        case PRODUCT_UI_STATE_ERROR:
            return "error";
    }
    return "offline";
}

void product_ui_set_wifi_connected(product_ui_t* ui, bool connected) {
    if (ui == NULL || ui->wifi_connected == connected) {
        return;
    }
    ui->wifi_connected = connected;
    render_connection_status(ui);
}

bool product_ui_is_wifi_connected(const product_ui_t* ui) {
    return ui != NULL && ui->wifi_connected;
}

bool product_ui_is_hermes_connected(const product_ui_t* ui) {
    return ui != NULL && ui->hermes_connected;
}

void product_ui_set_playback_envelope(product_ui_t* ui, uint16_t level_milli) {
    if (ui == NULL) {
        return;
    }
    ui->envelope_target =
        level_milli > PRODUCT_UI_ENVELOPE_MAX ? PRODUCT_UI_ENVELOPE_MAX : level_milli;
}

uint16_t product_ui_get_smoothed_envelope(const product_ui_t* ui) {
    return ui == NULL ? 0 : ui->envelope_smoothed;
}

void product_ui_set_transcript(product_ui_t* ui, const char* transcript) {
    if (ui == NULL) {
        return;
    }
    ui->transcript[0] = '\0';
    if (transcript == NULL || transcript[0] == '\0') {
        lv_label_set_text(ui->transcript_label, "");
        product_ui_view_set_visible(ui->transcript_label, false);
        return;
    }

    const size_t length = strlen(transcript);
    if (length <= PRODUCT_UI_TRANSCRIPT_MAX_BYTES) {
        memcpy(ui->transcript, transcript, length + 1U);
    } else {
        const size_t content_limit = PRODUCT_UI_TRANSCRIPT_MAX_BYTES - 3U;
        size_t copy_length = 0;
        while (copy_length < content_limit && transcript[copy_length] != '\0') {
            const unsigned char byte = (unsigned char)transcript[copy_length];
            size_t codepoint_length = 1;
            if ((byte & 0xE0U) == 0xC0U) {
                codepoint_length = 2;
            } else if ((byte & 0xF0U) == 0xE0U) {
                codepoint_length = 3;
            } else if ((byte & 0xF8U) == 0xF0U) {
                codepoint_length = 4;
            }
            if (copy_length + codepoint_length > content_limit) {
                break;
            }
            copy_length += codepoint_length;
        }
        memcpy(ui->transcript, transcript, copy_length);
        memcpy(ui->transcript + copy_length, "...", 4U);
    }
    lv_label_set_text(ui->transcript_label, ui->transcript);
    product_ui_view_set_visible(ui->transcript_label, true);
}

const char* product_ui_get_transcript(const product_ui_t* ui) {
    return ui == NULL ? "" : ui->transcript;
}

void product_ui_set_background_color(product_ui_t* ui, uint32_t color) {
    if (ui == NULL || ui->root == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(ui->root, lv_color_hex(color), LV_PART_MAIN);
}

void product_ui_set_animation_time(product_ui_t* ui, uint32_t time_ms) {
    if (ui == NULL) {
        return;
    }

    const uint32_t elapsed_ms =
        time_ms >= ui->animation_time_ms ? time_ms - ui->animation_time_ms : 0;
    ui->animation_time_ms = time_ms;

    if (ui->state == PRODUCT_UI_STATE_SPEAKING) {
        if (ui->envelope_target > ui->envelope_smoothed) {
            const uint16_t remaining = ui->envelope_target - ui->envelope_smoothed;
            const uint32_t time_to_target = (remaining + PRODUCT_UI_ENVELOPE_ATTACK_PER_MS - 1U) /
                                            PRODUCT_UI_ENVELOPE_ATTACK_PER_MS;
            ui->envelope_smoothed =
                elapsed_ms >= time_to_target
                    ? ui->envelope_target
                    : (uint16_t)(ui->envelope_smoothed +
                                 (elapsed_ms * PRODUCT_UI_ENVELOPE_ATTACK_PER_MS));
        } else if (ui->envelope_target < ui->envelope_smoothed) {
            const uint16_t remaining = ui->envelope_smoothed - ui->envelope_target;
            const uint32_t time_to_target = (remaining + PRODUCT_UI_ENVELOPE_RELEASE_PER_MS - 1U) /
                                            PRODUCT_UI_ENVELOPE_RELEASE_PER_MS;
            ui->envelope_smoothed =
                elapsed_ms >= time_to_target
                    ? ui->envelope_target
                    : (uint16_t)(ui->envelope_smoothed -
                                 (elapsed_ms * PRODUCT_UI_ENVELOPE_RELEASE_PER_MS));
        }
    } else {
        ui->envelope_target = 0;
        ui->envelope_smoothed = 0;
    }
    render(ui);
}
