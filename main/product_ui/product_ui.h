#ifndef PRODUCT_UI_H
#define PRODUCT_UI_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the platform-independent Product UI on the supplied LVGL parent.
 *
 * LVGL must be initialized before this function is called. The caller must
 * hold the platform LVGL lock when its display adapter requires one.
 *
 * @return The Product UI root object, or NULL when parent is NULL.
 */
lv_obj_t* product_ui_create(lv_obj_t* parent);

#ifdef __cplusplus
}
#endif

#endif  // PRODUCT_UI_H
