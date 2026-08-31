/**
 * @file lv_port_init.c
 *
 */

#include <lvgl/lvgl.h>
#include <lvgl/lv_conf.h>
#include <stdlib.h>

#include "lv_port_disp.h"
#include "lv_port_indev.h"

/* 0, 90, 180, 270 */
static int g_indev_rotation = 0;

void lv_port_init(int width, int height, int rotation)
{
    lv_init();

    lv_port_disp_init(width, height, rotation);
    if (lv_disp_get_default() == NULL) {
        LV_LOG_ERROR("Display init failed");
        exit(EXIT_FAILURE);
    }
    lv_port_indev_init(g_indev_rotation + rotation);
}
