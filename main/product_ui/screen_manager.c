#include "screen_manager.h"
#include "view_helpers.h"

#include <stddef.h>

#define SCREEN_MANAGER_TIMER_PERIOD_MS 50U

struct screen_manager {
    product_ui_t* product_ui;
    lv_obj_t* avatar_root;
    lv_obj_t* clock_root;
    lv_obj_t* status_root;
    lv_timer_t* timer;
    screen_manager_screen_t selected_screen;
    screen_manager_screen_t visible_screen;
    product_ui_state_t conversation_state;
    uint32_t timer_epoch_ms;
    uint32_t time_ms;
    uint32_t return_deadline_ms;
    bool return_pending;
    bool push_to_talk_active;
};

static bool is_voice_activity(product_ui_state_t state) {
    return state == PRODUCT_UI_STATE_LISTENING || state == PRODUCT_UI_STATE_THINKING ||
           state == PRODUCT_UI_STATE_SPEAKING || state == PRODUCT_UI_STATE_ERROR;
}

static bool deadline_reached(uint32_t time_ms, uint32_t deadline_ms) {
    return (int32_t)(time_ms - deadline_ms) >= 0;
}

static void present(screen_manager_t* manager, screen_manager_screen_t screen) {
    manager->visible_screen = screen;
    product_ui_view_set_visible(manager->avatar_root, screen == SCREEN_MANAGER_SCREEN_AVATAR);
    product_ui_view_set_visible(manager->clock_root, screen == SCREEN_MANAGER_SCREEN_CLOCK);
    product_ui_view_set_visible(manager->status_root, screen == SCREEN_MANAGER_SCREEN_STATUS);
}

static void on_timer(lv_timer_t* timer) {
    screen_manager_t* manager = lv_timer_get_user_data(timer);
    screen_manager_update_time(manager, lv_tick_elaps(manager->timer_epoch_ms));
}

static void on_parent_deleted(lv_event_t* event) {
    screen_manager_t* manager = lv_event_get_user_data(event);
    if (manager->timer != NULL) {
        lv_timer_delete(manager->timer);
        manager->timer = NULL;
    }
    lv_free(manager);
}

screen_manager_t* screen_manager_create(product_ui_t* product_ui, clock_screen_t* clock_screen,
                                        status_screen_t* status_screen) {
    lv_obj_t* avatar_root = product_ui_get_root(product_ui);
    lv_obj_t* clock_root = clock_screen_get_root(clock_screen);
    lv_obj_t* status_root = status_screen_get_root(status_screen);
    if (avatar_root == NULL || clock_root == NULL || status_root == NULL ||
        lv_obj_get_parent(avatar_root) == NULL ||
        lv_obj_get_parent(avatar_root) != lv_obj_get_parent(clock_root) ||
        lv_obj_get_parent(avatar_root) != lv_obj_get_parent(status_root)) {
        return NULL;
    }

    screen_manager_t* manager = lv_malloc_zeroed(sizeof(*manager));
    if (manager == NULL) {
        return NULL;
    }

    manager->product_ui = product_ui;
    manager->avatar_root = avatar_root;
    manager->clock_root = clock_root;
    manager->status_root = status_root;
    manager->selected_screen = SCREEN_MANAGER_SCREEN_AVATAR;
    manager->visible_screen = SCREEN_MANAGER_SCREEN_AVATAR;
    manager->conversation_state = product_ui_get_state(product_ui);
    manager->timer_epoch_ms = lv_tick_get();
    present(manager, SCREEN_MANAGER_SCREEN_AVATAR);

    manager->timer = lv_timer_create(on_timer, SCREEN_MANAGER_TIMER_PERIOD_MS, manager);
    if (manager->timer == NULL) {
        lv_free(manager);
        return NULL;
    }
    lv_obj_add_event_cb(lv_obj_get_parent(manager->avatar_root), on_parent_deleted, LV_EVENT_DELETE,
                        manager);
    return manager;
}

bool screen_manager_handle_swipe(screen_manager_t* manager, screen_manager_swipe_t swipe) {
    if (manager == NULL || manager->push_to_talk_active ||
        is_voice_activity(manager->conversation_state) || manager->return_pending) {
        return false;
    }

    screen_manager_screen_t next_screen = manager->selected_screen;
    if (swipe == SCREEN_MANAGER_SWIPE_LEFT) {
        if (manager->selected_screen == SCREEN_MANAGER_SCREEN_AVATAR) {
            next_screen = SCREEN_MANAGER_SCREEN_CLOCK;
        } else if (manager->selected_screen == SCREEN_MANAGER_SCREEN_CLOCK) {
            next_screen = SCREEN_MANAGER_SCREEN_STATUS;
        }
    } else if (swipe == SCREEN_MANAGER_SWIPE_RIGHT) {
        if (manager->selected_screen == SCREEN_MANAGER_SCREEN_STATUS) {
            next_screen = SCREEN_MANAGER_SCREEN_CLOCK;
        } else if (manager->selected_screen == SCREEN_MANAGER_SCREEN_CLOCK) {
            next_screen = SCREEN_MANAGER_SCREEN_AVATAR;
        }
    }

    if (next_screen == manager->selected_screen) {
        return false;
    }
    manager->selected_screen = next_screen;
    present(manager, next_screen);
    return true;
}

void screen_manager_set_push_to_talk_active(screen_manager_t* manager, bool active) {
    if (manager != NULL) {
        manager->push_to_talk_active = active;
    }
}

void screen_manager_set_conversation_state(screen_manager_t* manager, product_ui_state_t state) {
    if (manager == NULL) {
        return;
    }

    const bool was_active = is_voice_activity(manager->conversation_state);
    const bool active = is_voice_activity(state);
    manager->conversation_state = state;

    if (active) {
        manager->return_pending = false;
        present(manager, SCREEN_MANAGER_SCREEN_AVATAR);
        return;
    }

    if (was_active && manager->selected_screen != SCREEN_MANAGER_SCREEN_AVATAR) {
        manager->return_deadline_ms = manager->time_ms + SCREEN_MANAGER_IDLE_RETURN_DELAY_MS;
        manager->return_pending = true;
        present(manager, SCREEN_MANAGER_SCREEN_AVATAR);
        return;
    }

    if (!manager->return_pending) {
        present(manager, manager->selected_screen);
    }
}

bool screen_manager_dispatch_event(screen_manager_t* manager, product_ui_event_t event) {
    if (manager == NULL) {
        return false;
    }
    const bool changed = product_ui_dispatch(manager->product_ui, event);
    screen_manager_set_conversation_state(manager, product_ui_get_state(manager->product_ui));
    return changed;
}

void screen_manager_update_time(screen_manager_t* manager, uint32_t time_ms) {
    if (manager == NULL) {
        return;
    }
    manager->time_ms = time_ms;
    if (manager->return_pending && deadline_reached(time_ms, manager->return_deadline_ms)) {
        manager->return_pending = false;
        present(manager, manager->selected_screen);
    }
}

screen_manager_screen_t screen_manager_get_selected_screen(const screen_manager_t* manager) {
    return manager == NULL ? SCREEN_MANAGER_SCREEN_AVATAR : manager->selected_screen;
}

screen_manager_screen_t screen_manager_get_visible_screen(const screen_manager_t* manager) {
    return manager == NULL ? SCREEN_MANAGER_SCREEN_AVATAR : manager->visible_screen;
}

bool screen_manager_is_return_pending(const screen_manager_t* manager) {
    return manager != NULL && manager->return_pending;
}

bool screen_manager_is_push_to_talk_active(const screen_manager_t* manager) {
    return manager != NULL && manager->push_to_talk_active;
}

const char* screen_manager_screen_name(screen_manager_screen_t screen) {
    switch (screen) {
        case SCREEN_MANAGER_SCREEN_CLOCK:
            return "clock";
        case SCREEN_MANAGER_SCREEN_STATUS:
            return "status";
        default:
            return "avatar";
    }
}
