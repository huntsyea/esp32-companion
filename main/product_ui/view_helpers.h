#ifndef PRODUCT_UI_VIEW_HELPERS_H
#define PRODUCT_UI_VIEW_HELPERS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void product_ui_view_make_event_transparent(lv_obj_t* object);
void product_ui_view_apply_surface_style(lv_obj_t* object, uint32_t color, int32_t radius);
lv_obj_t* product_ui_view_create_shape(lv_obj_t* parent, int32_t x, int32_t y, int32_t width,
                                       int32_t height, uint32_t color, int32_t radius);
void product_ui_view_set_visible(lv_obj_t* object, bool visible);

#ifdef __cplusplus
}
#endif

#endif  // PRODUCT_UI_VIEW_HELPERS_H
