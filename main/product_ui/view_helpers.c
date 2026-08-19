#include "view_helpers.h"

void product_ui_view_make_event_transparent(lv_obj_t* object) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

void product_ui_view_apply_surface_style(lv_obj_t* object, uint32_t color, int32_t radius) {
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
    product_ui_view_make_event_transparent(object);
}

lv_obj_t* product_ui_view_create_shape(lv_obj_t* parent, int32_t x, int32_t y, int32_t width,
                                       int32_t height, uint32_t color, int32_t radius) {
    lv_obj_t* shape = lv_obj_create(parent);
    lv_obj_set_pos(shape, x, y);
    lv_obj_set_size(shape, width, height);
    product_ui_view_apply_surface_style(shape, color, radius);
    return shape;
}

void product_ui_view_set_visible(lv_obj_t* object, bool visible) {
    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}
