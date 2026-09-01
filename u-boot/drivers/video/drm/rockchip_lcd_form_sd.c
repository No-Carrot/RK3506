#include <drm/rockchip_lcd_form_sd.h>
#include <common.h>
#include <dm.h>
#include <dm/root.h>
#include <boot_rkimg.h>
#include <image.h>
#define CUSTOM_PARTITION_NAME   "baseparameter"
#define LCD_PARAM_MAX_COUNT     26
#define LCD_BUF_LENGTH  512
#define BASE_OFFSET 512   //512*block     1block=512B

int lcdParam[LCD_PARAM_MAX_COUNT];
char param_buf_temp[LCD_BUF_LENGTH] = {0};

ulong fdt_getprop_u32(const void *fdt, int node, const char *prop)
{
        const u32 *cell;
        int len;
        cell = fdt_getprop(fdt, node, prop, &len);
        if (!cell || len != sizeof(*cell))
                return FDT_ERROR;
        return fdt32_to_cpu(*cell);
}

int get_lcdparam_info_from_custom_partition(struct display_fixup_data *data)
{
        struct blk_desc *dev_desc;
        disk_partition_t part_info;
        char *boot_partname = CUSTOM_PARTITION_NAME;
        int ret,i;

        dev_desc = rockchip_get_bootdev();
        if (!dev_desc) {
                printf("-Gobro- %s: dev_desc is NULL!\n", __func__);
                return -ENODEV;
        }

        ret = part_get_info_by_name(dev_desc, boot_partname, &part_info);
        if (ret < 0) {
                printf("-Gobro- %s: failed to get %s part, ret=%d\n",
                                __func__, boot_partname, ret);
                /* RKIMG can support part table without 'boot' */
                return -1;
        }

        printf("-Gobro- block num: %lu, name %s ,type %s,block size :%lu\n",part_info.size,part_info.name,part_info.type,part_info.blksz);

        ret = blk_dread(dev_desc, part_info.start + BASE_OFFSET, 2, param_buf_temp);
        if (ret != 2) {
                printf("-Gobro- %s: failed to read screen parameter, ret=%d\n",
                                __func__, ret);
                return -1;
        }

        for (i = 0; i < LCD_PARAM_MAX_COUNT; i++) {
                lcdParam[i] = param_buf_temp[i * 4];
                lcdParam[i] =  (lcdParam[i] << 8) + param_buf_temp[i * 4 + 1];
                lcdParam[i] =  (lcdParam[i] << 8) + param_buf_temp[i * 4 + 2];
                lcdParam[i] =  (lcdParam[i] << 8) + param_buf_temp[i * 4 + 3];
                if(lcdParam[i] < 0)
                        lcdParam[i] = -lcdParam[i];

                if(lcdParam[i] > 100000 && i != 9)
                        lcdParam[i] = 0;

                printf("-Gobro- lcd_param[%d] %d\n",i, lcdParam[i]);
        }

        if((lcdParam[14] == 0 || lcdParam[10] == 0) && (lcdParam[0] != PANEL_TYPE_HDMI) )
                return -1;

        printf("-Gobro- crc32 = 0X%02X%02X%02X%02X\n",
                        param_buf_temp[LCD_BUF_LENGTH - 4], param_buf_temp[LCD_BUF_LENGTH - 3],
                        param_buf_temp[LCD_BUF_LENGTH - 2], param_buf_temp[LCD_BUF_LENGTH - 1]);

        if(!param_buf_temp[LCD_BUF_LENGTH - 4] && !param_buf_temp[LCD_BUF_LENGTH - 3] &&
                        !param_buf_temp[LCD_BUF_LENGTH - 3] && !param_buf_temp[LCD_BUF_LENGTH - 1])
                return -1;

        for(int i = 0 ; i < LCD_PARAM_MAX_COUNT ; i++)
                *((int *)data + i) = lcdParam[i];

        data->init_cmd =  malloc(sizeof(*(data->init_cmd)) * data->init_cmd_length);
        for(i = 0; i < data->init_cmd_length; i++){
                *(data->init_cmd + i) = param_buf_temp[(LCD_PARAM_MAX_COUNT * 4) + i];
                printf("init cmd = %x\n",param_buf_temp[(LCD_PARAM_MAX_COUNT * 4) + i]);
        }

        return 0;
}

static int __maybe_unused fdt_fixup_node_status(void *blob, int node, enum fdt_status status)
{
        int ret;

        printf("fixup %s %d\n", fdt_get_name(blob, node, NULL), status);

set_status:
        ret = fdt_set_node_status(blob, node, status, 0);
        if (ret == -FDT_ERR_NOSPACE) {
                printf("==============> -FDT_ERR_NOSPACE\n");
                ret = fdt_increase_size(blob, 512);
                if (!ret)
                        goto set_status;
                else
                        goto err_size;
        } else if (ret < 0) {
                printf("Can't set node status: %s\n", fdt_strerror(ret));
                return ret;
        }

        return 0;

err_size:
        printf("Can't increase blob size: %s\n", fdt_strerror(ret));
        return ret;
}

static int find_connector_node(void *blob, int node, enum fdt_status status)
{
        int phandle, remote;
        int nodedepth;
        phandle = fdt_getprop_u32_default_node(blob, node, 0,
                        "remote-endpoint", -1);

        remote = fdt_node_offset_by_phandle(blob, phandle);

        fdt_fixup_node_status(blob, remote, status);

        nodedepth = fdt_node_depth(blob, remote);

        return fdt_supernode_atdepth_offset(blob, remote,
                        nodedepth - 3, NULL);
}

static int get_panel_node(const void *blob, int conn_node)
{
        int panel, ports, port, ep, remote, ph, nodedepth;
        int reg_val;
        panel = fdt_subnode_offset(blob, conn_node, "panel");
        if (panel > 0)
                return panel;

        ports = fdt_subnode_offset(blob, conn_node, "ports");
        if (ports < 0)
                return -ENODEV;

        fdt_for_each_subnode(port, blob, ports) {
                reg_val  = fdt_getprop_u32(blob, port, "reg");
                if(reg_val != 1)
                        continue;
                fdt_for_each_subnode(ep, blob, port) {
                        ph = fdt_getprop_u32_default_node(blob, ep, 0,
                                        "remote-endpoint", 0);
                        if (!ph)
                                continue;

                        remote = fdt_node_offset_by_phandle(blob, ph);

                        nodedepth = fdt_node_depth(blob, remote);
                        if (nodedepth < 2)
                                continue;
                        printf("nodedepth %d \n", nodedepth);
                        panel = fdt_supernode_atdepth_offset(blob, remote,
                                        nodedepth - 3,
                                        NULL);
                        break;
                }
        }

        return panel;
}

static int fdt_fixup_panel_init_sequence(void *fdt, int node,const struct display_fixup_data *data)
{
#if 0
        u8 init_buf[] = {0x05, 0x00, 0x01, 0x78, 0x15, 0x01, 0x02, 0x03, 0x04, 0x05, 0x05, 0x01, 0x14,0x39, 0x01, 0x03, 0x02, 0x29, 0x11};
        u8 exit_buf[] = {0x05, 0x64, 0x01, 0x29, 0x05, 0x64, 0x01, 0x11};
#endif
        int ret;

add_seq:
        ret = fdt_setprop(fdt, node, "panel-init-sequence", data->init_cmd, data->init_cmd_length);
        if (ret == -FDT_ERR_NOSPACE) {
                printf(" init sequence FDT_ERR_NOSPACE\n");
                ret = fdt_increase_size(fdt, data->init_cmd_length * 4);//gln the length needs precision
                if (!ret)
                        goto add_seq;
                else
                        goto err_size;
        } else if (ret < 0) {
                printf("Can't add property: %s\n", fdt_strerror(ret));
                return ret;
        }

#if 0
add_init_seq:
        ret = fdt_setprop(fdt, node, "panel-init-sequence", init_buf, sizeof(init_buf));
        if (ret == -FDT_ERR_NOSPACE) {
                printf(" init sequence FDT_ERR_NOSPACE\n");
                ret = fdt_increase_size(fdt, 512);//gln the length needs precision
                if (!ret)
                        goto add_init_seq;
                else
                        goto err_size;
        } else if (ret < 0) {
                printf("Can't add property: %s\n", fdt_strerror(ret));
                return ret;
        }
add_exit_seq:
        ret = fdt_setprop(fdt, node, "panel-exit-sequence", exit_buf, sizeof(exit_buf));
        if (ret == -FDT_ERR_NOSPACE) {
                printf(" init sequence FDT_ERR_NOSPACE\n");
                ret = fdt_increase_size(fdt, 512);//gln the length needs precision
                if (!ret)
                        goto add_exit_seq;
                else
                        goto err_size;
        } else if (ret < 0) {
                printf("Can't add property: %s\n", fdt_strerror(ret));
                return ret;
        }
#endif

        return 0;

err_size:
        printf("Can't increase blob size: %s\n", fdt_strerror(ret));
        return ret;
}

static int fdt_fixup_setprop_u32(void *fdt, int node, const char *name, u32 data)
{
        int ret;

set_prop:
        ret = fdt_setprop_u32(fdt, node, name, data);
        if (ret == -FDT_ERR_NOSPACE) {
                ret = fdt_increase_size(fdt, 512);
                if (!ret)
                        goto set_prop;
                else
                        goto err_size;
        } else if (ret < 0) {
                printf("Can't add property: %s\n", fdt_strerror(ret));
                return ret;
        }

        return 0;

err_size:
        printf("Can't increase blob size: %s\n", fdt_strerror(ret));
        return ret;
}

static void fdt_fixup_display_timing(void *blob, int node,
                const struct display_fixup_data *data)
{       printf("===============> in fdt_fixup_display_timing\n");
        fdt_fixup_setprop_u32(blob, node, "clock-frequency", data->clock_frequency);
        fdt_fixup_setprop_u32(blob, node, "hactive", data->hactive);
        fdt_fixup_setprop_u32(blob, node, "hfront-porch", data->hfront_porch);
        fdt_fixup_setprop_u32(blob, node, "hsync-len", data->hsync_len);
        fdt_fixup_setprop_u32(blob, node, "hback-porch", data->hback_porch);
        fdt_fixup_setprop_u32(blob, node, "vactive", data->vactive);
        fdt_fixup_setprop_u32(blob, node, "vfront-porch", data->vfront_porch);
        fdt_fixup_setprop_u32(blob, node, "vsync-len", data->vsync_len);
        fdt_fixup_setprop_u32(blob, node, "vback-porch", data->vback_porch);
        fdt_fixup_setprop_u32(blob, node, "hsync-active", data->hsync_active);
        fdt_fixup_setprop_u32(blob, node, "vsync-active", data->vsync_active);
        fdt_fixup_setprop_u32(blob, node, "de-active", data->de_active);
        fdt_fixup_setprop_u32(blob, node, "pixelclk-active", data->pixelclk_active);

        printf("==============> clock-frequency=%d, hactive=%d, vactive=%d\n", data->clock_frequency, data->hactive, data->vactive);
}

static void fdt_fixup_panel_node(void *blob, int node, const char *name,
                const struct display_fixup_data *data)
{
        if (!strncmp(name, "dsi", 3)) {
                fdt_setprop_u32(blob, node, "dsi,flags", data->flags);
                fdt_setprop_u32(blob, node, "dsi,format", data->format);
                fdt_setprop_u32(blob, node, "dsi,lanes", data->lanes);
                fdt_fixup_panel_init_sequence(blob, node,data);
        }
        fdt_fixup_setprop_u32(blob, node, "prepare-delay-ms", data->delay_prepare);
        fdt_fixup_setprop_u32(blob, node, "enable-delay-ms", data->delay_enable);
        fdt_fixup_setprop_u32(blob, node, "disable-delay-ms", data->delay_disable);
        fdt_fixup_setprop_u32(blob, node, "unprepare-delay-ms", data->delay_unprepare);
        fdt_fixup_setprop_u32(blob, node, "reset-delay-ms", data->delay_reset);
        fdt_fixup_setprop_u32(blob, node, "init-delay-ms", data->delay_init);
        fdt_fixup_setprop_u32(blob, node, "width-mm", data->size_width);
        fdt_fixup_setprop_u32(blob, node, "height-mm", data->size_height);

}

static int fdt_fixup_display_sub_route(void *blob, const char *name,
                enum fdt_status status,
                const struct display_fixup_data *data)
{
        int route, phandle, connect, connector, panel, dt, timing;
        char path[64];
        int ret;

        memset(path, 0, sizeof(path));

        sprintf(path, "/display-subsystem/route/route-%s", name);

        route = fdt_path_offset(blob, path);
        if (route < 0){
                printf("==========> %s node is not exist\n", path);
                return route;
        }
        printf("==========> %s node is found\n", path);

        /* fixup route status */
        ret = fdt_fixup_node_status(blob, route, status);
        if (ret < 0)
                return ret;

        phandle = fdt_getprop_u32_default_node(blob, route, 0, "connect", -1);
        if (phandle < 0)
                return phandle;

        connect = fdt_node_offset_by_phandle(blob, phandle);
        if (connect < 0)
                return connect;

        connector = find_connector_node(blob, connect, status);
        if (connector < 0)
                return connector;

        /* fixup connector status */
        ret = fdt_fixup_node_status(blob, connector, status);
        if (ret < 0)
                return ret;

        if (status != FDT_STATUS_OKAY)
                return 0;

        panel = get_panel_node(blob, connector);
        if (panel < 0)
                return panel;

        /* fixup panel info */
        fdt_fixup_panel_node(blob, panel, name, data);

        if (!strncmp(name, "dsi", 3)) {
                dt = fdt_subnode_offset(blob, panel, "display-timings");
                if (dt < 0)
                        return dt;
        }else if ( (!strncmp(name, "edp", 3)) || (!strncmp(name, "lvds", 4)) ) {
                memset(path, 0, sizeof(path));
                sprintf(path, "/%s-panel", name);
                panel = fdt_path_offset(blob, path);
                if (panel < 0){
                        return panel;
                }else{
                        dt = fdt_subnode_offset(blob, panel, "display-timings");
                        if (dt < 0)
                                return dt;
                }
        }else{
                return -1;
        }
        printf("==================> display-timings node found\n");
        timing = fdt_subnode_offset(blob, dt, "timing0");
        if (timing < 0) {
                printf("==================> timing < 0\n");
                phandle = fdt_getprop_u32_default_node(blob, dt, 0, "native-mode", -1);
                if (phandle < 0)
                        return phandle;

                timing = fdt_node_offset_by_phandle(blob, phandle);
                if (timing < 0)
                        return timing;
        }
        printf("==================> timing node okay\n");

        /* fixup panel display timing */
        fdt_fixup_display_timing(blob, timing, data);

        return 0;
}

void fdt_fixup_display_route(void *blob, const struct display_fixup_data *data)
{
        if (data->type == PANEL_TYPE_DSI) {
                printf("================> PANEL_TYPE_DSI\n");
                fdt_fixup_display_sub_route(blob, "dsi", FDT_STATUS_OKAY, data);
                fdt_fixup_display_sub_route(blob, "edp",  FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "lvds", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "hdmi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "rgb", FDT_STATUS_DISABLED, data);
        } else if (data->type == PANEL_TYPE_EDP) {
                printf("================> PANEL_TYPE_EDP\n");
                fdt_fixup_display_sub_route(blob, "dsi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "edp",  FDT_STATUS_OKAY, data);
                fdt_fixup_display_sub_route(blob, "lvds", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "hdmi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "rgb", FDT_STATUS_DISABLED, data);
        } else if (data->type == PANEL_TYPE_LVDS) {
                printf("================> PANEL_TYPE_LVDS\n");
                fdt_fixup_display_sub_route(blob, "dsi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "edp",  FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "lvds", FDT_STATUS_OKAY, data);
                fdt_fixup_display_sub_route(blob, "hdmi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "rgb", FDT_STATUS_DISABLED, data);
        } else if (data->type == PANEL_TYPE_HDMI) {
                printf("================> PANEL_TYPE_HDMI\n");
                fdt_fixup_display_sub_route(blob, "dsi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "edp",  FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "lvds", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "hdmi", FDT_STATUS_OKAY, data);
                fdt_fixup_display_sub_route(blob, "rgb", FDT_STATUS_DISABLED, data);
        } else if (data->type == PANEL_TYPE_RGB) {
                printf("================> PANEL_TYPE_RGB\n");
                fdt_fixup_display_sub_route(blob, "dsi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "edp",  FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "lvds", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "hdmi", FDT_STATUS_DISABLED, data);
                fdt_fixup_display_sub_route(blob, "rgb", FDT_STATUS_OKAY, data);
        }
}

