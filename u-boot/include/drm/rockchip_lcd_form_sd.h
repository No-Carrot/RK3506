#ifndef _ROCKCHIP_DISPLAY_H
#define _ROCKCHIP_DISPLAY_H
struct display_fixup_data {
        int type;
        int delay_prepare;
        int delay_enable;
        int delay_disable;
        int delay_unprepare;
        int delay_reset;
        int delay_init;
        int size_width;
        int size_height;
        int clock_frequency;
        int hactive;
        int hfront_porch;
        int hsync_len;
        int hback_porch;
        int vactive;
        int vfront_porch;
        int vsync_len;
        int vback_porch;
        int hsync_active;
        int vsync_active;
        int de_active;
        int pixelclk_active;
        int flags;
        int format;
        int lanes;
        int init_cmd_length;
        char *init_cmd;
};

enum {
        PANEL_TYPE_DSI=0,
        PANEL_TYPE_EDP,
        PANEL_TYPE_LVDS,
        PANEL_TYPE_HDMI,
        PANEL_TYPE_RGB,
};

int get_lcdparam_info_from_custom_partition(struct display_fixup_data *data);
void fdt_fixup_display_route(void *blob, const struct display_fixup_data *data);
#endif

