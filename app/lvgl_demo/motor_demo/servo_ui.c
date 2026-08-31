/* LVGL frontend for the single-axis EtherCAT servo controller. */
#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#include "main.h"
#include "servo_protocol.h"
#include "ui_scaler.h"

#define STEP_ANGLE 10
#define FULL_TURN_DEGREES SERVO_DEGREES_PER_REV
#define FULL_TURN_PULSES SERVO_ENCODER_PULSES_PER_REV
#define METER_TICK_COUNT 12
#define METER_TICK_LABEL_RADIUS 108
#define METER_TICK_DOT_RADIUS 126
#define DEGREE_SYMBOL "\xC2\xB0"
#define HEADER_STATUS_PANEL_RIGHT 702
#define HEADER_STATUS_PANEL_WIDTH 258
#define HEADER_STATUS_PANEL_X (HEADER_STATUS_PANEL_RIGHT - HEADER_STATUS_PANEL_WIDTH)
#define SERVO_STATE_POLL_MS 100
#define SERVO_OFFLINE_POLL_MS 500
#define SERVO_STATE_TIMEOUT_MS 20
#define SERVO_COMMAND_TIMEOUT_MS 80

enum ui_command_id {
    UI_CMD_STOP_OR_RESET = -1,
};

struct ui_command {
    int32_t command;
    int32_t argument;
    int argument_is_degrees;
};

static const struct ui_command command_zero = {
    SERVO_CMD_ZERO_CALIBRATE, 0, 0
};
static const struct ui_command command_home = {
    SERVO_CMD_HOME, 0, 0
};
static const struct ui_command command_step_negative = {
    SERVO_CMD_STEP_POSITION, -STEP_ANGLE, 1
};
static const struct ui_command command_step_positive = {
    SERVO_CMD_STEP_POSITION, STEP_ANGLE, 1
};
static const struct ui_command command_self_test = {
    SERVO_CMD_SELF_TEST, 0, 0
};
static const struct ui_command command_jog_forward = {
    SERVO_CMD_JOG_FORWARD, 0, 0
};
static const struct ui_command command_jog_reverse = {
    SERVO_CMD_JOG_REVERSE, 0, 0
};
static const struct ui_command command_stop_or_reset = {
    UI_CMD_STOP_OR_RESET, 0, 0
};

static volatile sig_atomic_t running = 1;
static void *ui_scaler;
static lv_ft_info_t font_title;
static lv_ft_info_t font_header_title;
static lv_ft_info_t font_header_subtitle;
static lv_ft_info_t font_large;
static lv_ft_info_t font_button;
static lv_ft_info_t font_body;
static lv_ft_info_t font_status_label;
static lv_ft_info_t font_status_value;
static struct servo_state state;
static int backend_online;
static int meter_dragging;
static lv_timer_t *state_timer;
static lv_obj_t *meter_arc;
static lv_obj_t *meter_progress_arc;
static lv_obj_t *needle;
static lv_obj_t *target_needle;
static lv_point_t target_needle_points[2];
static lv_obj_t *angle_label;
static lv_obj_t *run_label;
static lv_obj_t *run_panel;
static lv_obj_t *value_position;
static lv_obj_t *value_target;
static lv_obj_t *value_drive;
static lv_obj_t *value_bus;
static lv_obj_t *value_mode;
static lv_obj_t *value_word;
static lv_obj_t *value_encoder;
static lv_obj_t *value_arrived;
static lv_obj_t *value_fault;
static lv_obj_t *value_comms;
static lv_obj_t *value_jitter;
static lv_obj_t *value_comm_frequency;
static lv_obj_t *button_zero;
static lv_obj_t *button_home;
static lv_obj_t *button_step_negative;
static lv_obj_t *button_step_positive;
static lv_obj_t *button_self_test;
static lv_obj_t *button_jog_forward;
static lv_obj_t *button_jog_reverse;
static lv_obj_t *button_stop;
static lv_obj_t *label_stop;
static uint32_t next_jitter_refresh_ms;
static int jitter_display_valid;

#define SCALE(x) ui_scaler_calc(ui_scaler, (x))

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void bind_current_process_to_cpu(int target_cpu)
{
    cpu_set_t mask;
    long cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    if (target_cpu < 0 || (cpu_count > 0 && target_cpu >= cpu_count))
        return;

    CPU_ZERO(&mask);
    CPU_SET(target_cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
        perror("motor_demo: sched_setaffinity");
}

static int parse_cpu_env(const char *name, int default_cpu)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (!value || !*value)
        return default_cpu;
    errno = 0;
    parsed = strtol(value, &end, 0);
    if (errno || end == value || *end || parsed < 0 || parsed > INT_MAX)
        return default_cpu;
    return (int)parsed;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t width, lv_coord_t height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, SCALE(8), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, SCALE(1), LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xD9E3E3), LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, SCALE(0), LV_PART_MAIN);
    return panel;
}

static int degree_from_position(int32_t position)
{
    int64_t wrapped = position % FULL_TURN_PULSES;
    int degree;

    if (wrapped < 0)
        wrapped += FULL_TURN_PULSES;
    degree = (int)((wrapped * FULL_TURN_DEGREES + FULL_TURN_PULSES / 2) /
                   FULL_TURN_PULSES);
    if (degree >= FULL_TURN_DEGREES)
        degree -= FULL_TURN_DEGREES;
    return degree;
}

static int32_t pulses_from_degrees(int32_t degrees)
{
    int64_t pulses = (int64_t)degrees * FULL_TURN_PULSES;

    if (pulses >= 0)
        pulses += FULL_TURN_DEGREES / 2;
    else
        pulses -= FULL_TURN_DEGREES / 2;
    pulses /= FULL_TURN_DEGREES;
    if (pulses > INT32_MAX)
        return INT32_MAX;
    if (pulses < INT32_MIN)
        return INT32_MIN;
    return (int32_t)pulses;
}

static void update_needle_line(lv_obj_t *line, lv_point_t *points, int degree,
                               int inner_radius, int outer_radius)
{
    const int center = 127;
    float radians;
    float sine;
    float cosine;

    degree %= FULL_TURN_DEGREES;
    if (degree < 0)
        degree += FULL_TURN_DEGREES;
    radians = (90.0f + degree) * PI / 180.0f;
    sine = sinf(radians);
    cosine = cosf(radians);
    points[0].x = SCALE(center + (int)lroundf(cosine * inner_radius));
    points[0].y = SCALE(center + (int)lroundf(sine * inner_radius));
    points[1].x = SCALE(center + (int)lroundf(cosine * outer_radius));
    points[1].y = SCALE(center + (int)lroundf(sine * outer_radius));
    lv_line_set_points(line, points, 2);
}

static void update_actual_display(int degree)
{
    degree %= FULL_TURN_DEGREES;
    if (degree < 0)
        degree += FULL_TURN_DEGREES;
    lv_arc_set_value(meter_progress_arc, degree);
    lv_img_set_angle(needle, (90 + degree) * 10);
    if (angle_label)
        lv_label_set_text_fmt(angle_label, "%d" DEGREE_SYMBOL, degree);
}

static void update_target_display(int degree)
{
    degree %= FULL_TURN_DEGREES;
    if (degree < 0)
        degree += FULL_TURN_DEGREES;
    lv_arc_set_value(meter_arc, degree);
    update_needle_line(target_needle, target_needle_points, degree, 24, 108);
}

static int32_t position_for_dial_degree(int degree)
{
    int64_t current = state.actual_position;
    int64_t revolution = current / FULL_TURN_PULSES;
    int64_t target;

    if (current < 0 && current % FULL_TURN_PULSES)
        revolution--;
    target = revolution * FULL_TURN_PULSES + pulses_from_degrees(degree);
    if (target - current > FULL_TURN_PULSES / 2)
        target -= FULL_TURN_PULSES;
    else if (current - target > FULL_TURN_PULSES / 2)
        target += FULL_TURN_PULSES;
    if (target > INT32_MAX)
        return INT32_MAX;
    if (target < INT32_MIN)
        return INT32_MIN;
    return target;
}

static int position_reached(void)
{
    int64_t delta = (int64_t)state.target_position - state.actual_position;

    return delta >= -SERVO_UI_ARRIVED_TOLERANCE_PULSES &&
        delta <= SERVO_UI_ARRIVED_TOLERANCE_PULSES;
}

static int drive_link_ready(void)
{
    return backend_online && (state.flags & SERVO_FLAG_LINK_UP);
}

static int drive_motion_ready(void)
{
    return drive_link_ready() &&
        (state.flags & SERVO_FLAG_BUS_OP) &&
        (state.flags & SERVO_FLAG_ENABLED) &&
        !(state.flags & SERVO_FLAG_FAULT);
}

static int drive_faulted(void)
{
    return drive_link_ready() && (state.flags & SERVO_FLAG_FAULT);
}

static int command_allowed(int32_t command)
{
    switch (command) {
    case SERVO_CMD_STOP:
    case SERVO_CMD_FAULT_RESET:
        return drive_link_ready();
    case SERVO_CMD_ZERO_CALIBRATE:
    case SERVO_CMD_HOME:
    case SERVO_CMD_STEP_POSITION:
    case SERVO_CMD_SELF_TEST:
    case SERVO_CMD_JOG_FORWARD:
    case SERVO_CMD_JOG_REVERSE:
    case SERVO_CMD_SET_POSITION:
        return drive_motion_ready();
    default:
        return 0;
    }
}

static void set_control_enabled(lv_obj_t *object, int enabled)
{
    if (!object)
        return;
    if (enabled)
        lv_obj_clear_state(object, LV_STATE_DISABLED);
    else
        lv_obj_add_state(object, LV_STATE_DISABLED);
}

static void refresh_controls(void)
{
    int motion_ready = drive_motion_ready();

    set_control_enabled(button_zero, motion_ready);
    set_control_enabled(button_home, motion_ready);
    set_control_enabled(button_step_negative, motion_ready);
    set_control_enabled(button_step_positive, motion_ready);
    set_control_enabled(button_self_test, motion_ready);
    set_control_enabled(button_jog_forward, motion_ready);
    set_control_enabled(button_jog_reverse, motion_ready);
    set_control_enabled(button_stop, drive_link_ready());
    set_control_enabled(meter_arc, motion_ready);

    if (meter_arc) {
        if (motion_ready)
            lv_obj_add_flag(meter_arc, LV_OBJ_FLAG_CLICKABLE);
        else
            lv_obj_clear_flag(meter_arc, LV_OBJ_FLAG_CLICKABLE);
    }

    if (!motion_ready)
        meter_dragging = 0;
    if (label_stop)
        lv_label_set_text(label_stop, drive_faulted() ? "故障复位" : "停止");
}

static void set_socket_timeout(int fd, int timeout_ms)
{
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static int exchange_with_backend(uint32_t type, int32_t command, int32_t argument)
{
    struct sockaddr_un address;
    struct servo_request request = {
        .version = SERVO_PROTOCOL_VERSION,
        .type = type,
        .command = command,
        .argument = argument,
    };
    int fd;
    ssize_t count;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
        return -1;
    set_socket_timeout(fd, type == SERVO_REQUEST_STATE ?
                       SERVO_STATE_TIMEOUT_MS : SERVO_COMMAND_TIMEOUT_MS);
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SERVO_SOCKET_PATH, sizeof(address.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    count = send(fd, &request, sizeof(request), MSG_NOSIGNAL);
    if (count == (ssize_t)sizeof(request))
        count = recv(fd, &state, sizeof(state), 0);
    close(fd);
    return count == (ssize_t)sizeof(state) && state.version == SERVO_PROTOCOL_VERSION ? 0 : -1;
}

static const char *drive_text(void)
{
    if (!backend_online || !(state.flags & SERVO_FLAG_LINK_UP))
        return "OFFLINE";
    if (!(state.flags & SERVO_FLAG_BUS_OP))
        return "SAFEOP";
    if (state.flags & SERVO_FLAG_FAULT)
        return "FAULT";
    if (state.flags & SERVO_FLAG_MOVING)
        return "RUNNING";
    if (state.flags & SERVO_FLAG_ENABLED)
        return "READY";
    return "DISABLED";
}

static const char *mode_text(int8_t mode)
{
    switch (mode) {
    case 8:
        return "CSP";
    case 9:
        return "CSV";
    default:
        return "UNKNOWN";
    }
}

static void update_header_status(void)
{
    const char *status;
    lv_color_t fill;
    lv_color_t border;
    lv_color_t text = lv_color_hex(0xFFFFFF);

    if (!backend_online || !(state.flags & SERVO_FLAG_LINK_UP)) {
        status = "OFFLINE";
        fill = lv_color_hex(0x667085);
        border = lv_color_hex(0x475467);
    } else if (!(state.flags & SERVO_FLAG_BUS_OP)) {
        status = "SAFEOP";
        fill = lv_color_hex(0xD97706);
        border = lv_color_hex(0x92400E);
    } else if (state.flags & SERVO_FLAG_FAULT) {
        status = "FAULT";
        fill = lv_color_hex(0xB42318);
        border = lv_color_hex(0x7A271A);
    } else if (state.flags & SERVO_FLAG_MOVING) {
        status = "RUNNING";
        fill = lv_color_hex(0x0B875A);
        border = lv_color_hex(0x065F46);
    } else if (state.flags & SERVO_FLAG_ENABLED) {
        status = "READY";
        fill = lv_color_hex(0x087C89);
        border = lv_color_hex(0x0E5964);
    } else {
        status = "STANDBY";
        fill = lv_color_hex(0x8A6116);
        border = lv_color_hex(0x66450D);
    }

    lv_label_set_text(run_label, status);
    lv_obj_set_style_text_color(run_label, text, LV_PART_MAIN);
    lv_obj_set_style_bg_color(run_panel, fill, LV_PART_MAIN);
    lv_obj_set_style_border_color(run_panel, border, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(run_panel, border, LV_PART_MAIN);
}

static void refresh_jitter_display(int force)
{
    uint32_t now_ms = lv_tick_get();

    if (!force && jitter_display_valid &&
        (int32_t)(now_ms - next_jitter_refresh_ms) < 0)
        return;

    jitter_display_valid = 1;
    next_jitter_refresh_ms = now_ms + SERVO_JITTER_REFRESH_MS;
    if (state.ethercat_period_min_us && state.ethercat_period_max_us) {
        lv_label_set_text_fmt(value_jitter, "%u us\n%u-%u us",
                              state.jitter_us,
                              state.ethercat_period_min_us,
                              state.ethercat_period_max_us);
    } else {
        lv_label_set_text_fmt(value_jitter, "%u us", state.jitter_us);
    }
}

static void refresh_ui(void)
{
    int32_t degree;
    uint32_t flags = state.flags;

    update_header_status();
    refresh_controls();

    if (!backend_online || !(flags & SERVO_FLAG_LINK_UP)) {
        lv_label_set_text(value_position, "--");
        lv_label_set_text(value_target, "--");
        lv_label_set_text(value_drive, "--");
        lv_label_set_text(value_bus, "--");
        lv_label_set_text(value_mode, "--");
        lv_label_set_text(value_word, "--");
        lv_label_set_text(value_encoder, "--");
        lv_label_set_text(value_arrived, "--");
        lv_label_set_text(value_fault, "--");
        lv_label_set_text(value_comms, "--");
        lv_label_set_text(value_jitter, "--");
        lv_label_set_text(value_comm_frequency, "--");
        jitter_display_valid = 0;
        next_jitter_refresh_ms = 0;
        return;
    }

    degree = degree_from_position(state.actual_position);
    update_actual_display(degree);
    if (!meter_dragging)
        update_target_display(degree_from_position(state.target_position));
    lv_label_set_text_fmt(value_position, "%ld" DEGREE_SYMBOL, (long)degree);
    lv_label_set_text_fmt(value_target, "%ld" DEGREE_SYMBOL,
                          (long)degree_from_position(state.target_position));
    lv_label_set_text(value_drive, drive_text());
    lv_label_set_text(value_bus, flags & SERVO_FLAG_BUS_OP ? "OP" : "SAFEOP");
    lv_label_set_text_fmt(value_mode, "%s / %d",
                          mode_text(state.mode_display), state.mode_display);
    lv_label_set_text_fmt(value_word, "0x%04X", state.status_word);
    lv_label_set_text(value_encoder, flags & SERVO_FLAG_LINK_UP ? "ON" : "OFF");
    lv_label_set_text(value_arrived, position_reached() &&
                      !(flags & SERVO_FLAG_MOVING) ? "YES" : "MOVING");
    lv_label_set_text(value_fault, state.error_code ? "FAULT" : "OK");
    lv_label_set_text_fmt(value_comms, "OK / %u", state.responding_slaves);
    refresh_jitter_display(0);
    lv_label_set_text_fmt(value_comm_frequency, "%u Hz",
                          state.ethercat_frequency_hz ?
                          state.ethercat_frequency_hz :
                          SERVO_ETHERCAT_CYCLE_HZ);
}

static void command_cb(lv_event_t *event)
{
    const struct ui_command *ui_command = lv_event_get_user_data(event);
    int32_t command;
    int32_t argument;

    if (!ui_command)
        return;

    command = ui_command->command;
    if (command == UI_CMD_STOP_OR_RESET)
        command = drive_faulted() ? SERVO_CMD_FAULT_RESET : SERVO_CMD_STOP;
    if (!command_allowed(command)) {
        refresh_ui();
        return;
    }

    argument = ui_command->argument_is_degrees ?
        pulses_from_degrees(ui_command->argument) : ui_command->argument;
    backend_online = exchange_with_backend(SERVO_REQUEST_COMMAND, command, argument) == 0;
    refresh_ui();
}

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    backend_online = exchange_with_backend(SERVO_REQUEST_STATE, SERVO_CMD_NONE, 0) == 0;
    if (timer)
        lv_timer_set_period(timer, backend_online ?
                            SERVO_STATE_POLL_MS : SERVO_OFFLINE_POLL_MS);
    refresh_ui();
}

static void meter_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        if (!drive_motion_ready())
            return;
        meter_dragging = 1;
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        if (!drive_motion_ready())
            return;
        update_target_display(lv_arc_get_value(meter_arc));
    } else if (code == LV_EVENT_RELEASED) {
        int degree = lv_arc_get_value(meter_arc);

        meter_dragging = 0;
        if (!command_allowed(SERVO_CMD_SET_POSITION)) {
            refresh_ui();
            return;
        }
        backend_online = exchange_with_backend(SERVO_REQUEST_COMMAND,
                                              SERVO_CMD_SET_POSITION,
                                              position_for_dial_degree(degree)) == 0;
        refresh_ui();
    } else if (code == LV_EVENT_PRESS_LOST) {
        meter_dragging = 0;
        refresh_ui();
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, lv_color_t color,
                             lv_coord_t x, lv_coord_t y,
                             const struct ui_command *command,
                             lv_obj_t **label_out)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;

    lv_obj_set_size(button, SCALE(166), SCALE(72));
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, SCALE(8), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, SCALE(4), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x405354), LV_PART_MAIN);
    lv_obj_set_style_opa(button, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_event_cb(button, command_cb, LV_EVENT_CLICKED, (void *)command);
    label = make_label(button, text, font_button.font, lv_color_white());
    lv_obj_center(label);
    if (label_out)
        *label_out = label;
    return button;
}

static void status_pair(lv_obj_t *parent, const char *name, lv_obj_t **value,
                        lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *title = make_label(parent, name, font_status_label.font, lv_color_hex(0x526770));
    lv_obj_set_pos(title, x, y);
    *value = make_label(parent, "--", font_status_value.font, lv_color_hex(0x18313B));
    lv_obj_set_pos(*value, x, y + SCALE(30));
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *band = lv_obj_create(screen);
    lv_obj_t *accent_bar;
    lv_obj_t *divider;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    struct utsname kernel_info;
    const char *kernel_release = "unknown";
    char subtitle_text[96];

    if (uname(&kernel_info) == 0)
        kernel_release = kernel_info.release;
    snprintf(subtitle_text, sizeof(subtitle_text), "RK3506/PREEMPT_RT%s",
             kernel_release);

    lv_obj_set_size(band, SCALE(720), SCALE(90));
    lv_obj_set_pos(band, 0, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(band, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(band, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(band, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(band, 0, LV_PART_MAIN);

    accent_bar = lv_obj_create(band);
    lv_obj_set_size(accent_bar, SCALE(4), SCALE(70));
    lv_obj_set_pos(accent_bar, SCALE(18), SCALE(12));
    lv_obj_clear_flag(accent_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(accent_bar, lv_color_hex(0x0F8498), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(accent_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(accent_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(accent_bar, SCALE(2), LV_PART_MAIN);
    lv_obj_set_style_pad_all(accent_bar, 0, LV_PART_MAIN);

    title = make_label(band, "EtherCAT IgH主站单轴伺服控制", font_header_title.font,
                       lv_color_hex(0x183E46));
    lv_obj_set_pos(title, SCALE(34), SCALE(16));
    subtitle = make_label(band, subtitle_text,
                          font_header_subtitle.font, lv_color_hex(0x62777C));
    lv_obj_set_pos(subtitle, SCALE(35), SCALE(56));

    run_panel = lv_obj_create(band);
    lv_obj_set_size(run_panel, SCALE(HEADER_STATUS_PANEL_WIDTH), SCALE(60));
    lv_obj_set_pos(run_panel, SCALE(HEADER_STATUS_PANEL_X), SCALE(14));
    lv_obj_clear_flag(run_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(run_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(run_panel, SCALE(8), LV_PART_MAIN);
    lv_obj_set_style_border_width(run_panel, SCALE(1), LV_PART_MAIN);
    lv_obj_set_style_pad_all(run_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(run_panel, SCALE(4), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(run_panel, LV_OPA_20, LV_PART_MAIN);

    run_label = make_label(run_panel, "OFFLINE", font_large.font,
                           lv_color_hex(0xFFFFFF));
    lv_obj_center(run_label);

    divider = lv_obj_create(band);
    lv_obj_set_size(divider, SCALE(684), SCALE(1));
    lv_obj_set_pos(divider, SCALE(18), SCALE(89));
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xD9E5E6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);

    update_header_status();
}

static void create_meter_ticks(lv_obj_t *panel)
{
    for (int i = 0; i < METER_TICK_COUNT; ++i) {
        int degree = i * (FULL_TURN_DEGREES / METER_TICK_COUNT);
        float radians = (90.0f + degree) * PI / 180.0f;
        float cosine = cosf(radians);
        float sine = sinf(radians);
        lv_obj_t *dot = lv_obj_create(panel);
        lv_obj_t *label;

        lv_obj_set_size(dot, SCALE(5), SCALE(5));
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x286B60), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(dot, LV_ALIGN_CENTER,
                     SCALE((int)lroundf(cosine * METER_TICK_DOT_RADIUS)),
                     SCALE(18 + (int)lroundf(sine * METER_TICK_DOT_RADIUS)));

        label = make_label(panel, "", font_body.font, lv_color_hex(0x486B6E));
        lv_label_set_text_fmt(label, "%d", degree);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(label, LV_ALIGN_CENTER,
                     SCALE((int)lroundf(cosine * METER_TICK_LABEL_RADIUS)),
                     SCALE(18 + (int)lroundf(sine * METER_TICK_LABEL_RADIUS)));
    }
}

static void create_meter(lv_obj_t *screen)
{
    lv_obj_t *panel = make_panel(screen, SCALE(370), SCALE(390));
    lv_obj_t *dial_background;
    lv_obj_set_pos(panel, SCALE(18), SCALE(96));
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);

    dial_background = lv_obj_create(panel);
    lv_obj_set_size(dial_background, SCALE(312), SCALE(312));
    lv_obj_set_style_bg_color(dial_background, lv_color_hex(0xE5F0EF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dial_background, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dial_background, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dial_background, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dial_background, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dial_background, SCALE(8), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(dial_background, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(dial_background, lv_color_hex(0x8CA9A8), LV_PART_MAIN);
    lv_obj_clear_flag(dial_background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(dial_background, LV_ALIGN_CENTER, 0, SCALE(18));

    meter_arc = lv_arc_create(panel);
    lv_obj_set_size(meter_arc, SCALE(272), SCALE(272));
    lv_arc_set_rotation(meter_arc, 90);
    lv_arc_set_bg_angles(meter_arc, 0, 360);
    lv_arc_set_range(meter_arc, 0, 359);
    lv_arc_set_value(meter_arc, 0);
    lv_obj_set_style_arc_width(meter_arc, SCALE(2), LV_PART_MAIN);
    lv_obj_set_style_arc_color(meter_arc, lv_color_hex(0x4AA48E), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(meter_arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meter_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_ext_click_area(meter_arc, SCALE(18));
    lv_obj_add_event_cb(meter_arc, meter_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_align(meter_arc, LV_ALIGN_CENTER, 0, SCALE(18));

    meter_progress_arc = lv_arc_create(panel);
    lv_obj_set_size(meter_progress_arc, SCALE(254), SCALE(254));
    lv_arc_set_rotation(meter_progress_arc, 90);
    lv_arc_set_bg_angles(meter_progress_arc, 0, 360);
    lv_arc_set_range(meter_progress_arc, 0, 359);
    lv_arc_set_value(meter_progress_arc, 0);
    lv_obj_set_style_arc_opa(meter_progress_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(meter_progress_arc, SCALE(6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(meter_progress_arc, lv_color_hex(0x12B58B), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(meter_progress_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meter_progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(meter_progress_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(meter_progress_arc, LV_ALIGN_CENTER, 0, SCALE(18));

    needle = lv_img_create(panel);
    lv_img_set_src(needle, SRC_PNG(needle));
    lv_img_set_zoom(needle, 128);
    lv_img_set_pivot(needle, 285, 285);
    lv_img_set_angle(needle, 900);
    lv_obj_clear_flag(needle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(needle, LV_ALIGN_CENTER, 0, SCALE(18));

    target_needle = lv_line_create(panel);
    lv_obj_set_size(target_needle, SCALE(254), SCALE(254));
    lv_obj_align(target_needle, LV_ALIGN_CENTER, 0, SCALE(18));
    lv_obj_set_style_line_width(target_needle, SCALE(3), LV_PART_MAIN);
    lv_obj_set_style_line_color(target_needle, lv_color_hex(0xD55D1B), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(target_needle, true, LV_PART_MAIN);
    lv_obj_clear_flag(target_needle, LV_OBJ_FLAG_CLICKABLE);
    update_target_display(0);

    angle_label = make_label(panel, "0" DEGREE_SYMBOL, font_title.font, lv_color_hex(0x16353A));
    lv_obj_clear_flag(angle_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(angle_label, LV_ALIGN_CENTER, 0, SCALE(18));
    update_actual_display(0);

    create_meter_ticks(panel);
}

static void create_status(lv_obj_t *screen)
{
    lv_obj_t *panel = make_panel(screen, SCALE(302), SCALE(422));
    lv_obj_set_pos(panel, SCALE(400), SCALE(96));
    status_pair(panel, "当前位置", &value_position, SCALE(18), SCALE(16));
    status_pair(panel, "目标位置", &value_target, SCALE(160), SCALE(16));
    status_pair(panel, "运行状态", &value_drive, SCALE(18), SCALE(84));
    status_pair(panel, "总线状态", &value_bus, SCALE(160), SCALE(84));
    status_pair(panel, "控制模式", &value_mode, SCALE(18), SCALE(152));
    status_pair(panel, "状态字", &value_word, SCALE(160), SCALE(152));
    status_pair(panel, "编码器状态", &value_encoder, SCALE(18), SCALE(220));
    status_pair(panel, "到位状态", &value_arrived, SCALE(160), SCALE(220));
    status_pair(panel, "故障状态", &value_fault, SCALE(18), SCALE(288));
    status_pair(panel, "通信", &value_comms, SCALE(160), SCALE(288));
    status_pair(panel, "抖动", &value_jitter, SCALE(18), SCALE(356));
    status_pair(panel, "总线频率", &value_comm_frequency, SCALE(160), SCALE(356));
}

static void create_controls(lv_obj_t *screen)
{
    lv_coord_t x = SCALE(18);
    lv_coord_t y = SCALE(546);
    button_zero = make_button(screen, "零点校准", lv_color_hex(0x0F8498), x, y,
                              &command_zero, NULL);
    button_home = make_button(screen, "回零", lv_color_hex(0x0C9A96),
                              x + SCALE(176), y, &command_home, NULL);
    button_step_negative = make_button(screen, "-10" DEGREE_SYMBOL,
                                       lv_color_hex(0xA83A63),
                                       x + SCALE(352), y,
                                       &command_step_negative, NULL);
    button_step_positive = make_button(screen, "+10" DEGREE_SYMBOL,
                                       lv_color_hex(0xD55D1B),
                                       x + SCALE(528), y,
                                       &command_step_positive, NULL);
    button_self_test = make_button(screen, "一键测试", lv_color_hex(0x31518A),
                                   x, y + SCALE(84), &command_self_test,
                                   NULL);
    button_jog_forward = make_button(screen, "正转连续", lv_color_hex(0x0B718A),
                                     x + SCALE(176), y + SCALE(84),
                                     &command_jog_forward, NULL);
    button_jog_reverse = make_button(screen, "反转连续", lv_color_hex(0x2D7C4D),
                                     x + SCALE(352), y + SCALE(84),
                                     &command_jog_reverse, NULL);
    button_stop = make_button(screen, "停止", lv_color_hex(0xB42318),
                              x + SCALE(528), y + SCALE(84),
                              &command_stop_or_reset, &label_stop);
    refresh_controls();
}

static void create_ui(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F6F6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    create_header(screen);
    create_meter(screen);
    create_status(screen);
    create_controls(screen);
}

int main(void)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    bind_current_process_to_cpu(parse_cpu_env("SERVO_UI_CPU",
                                              SERVO_UI_CPU));
    lv_port_init(0, 0, 0);
    ui_scaler = ui_scaler_new(720, 720);
    ui_scaler_set_refer_size(ui_scaler, LV_HOR_RES, LV_VER_RES);
    lv_freetype_init(64, 1, 0);
    font_title.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_title.weight = SCALE(32);
    font_title.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_title);
    font_header_title.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_header_title.weight = SCALE(28);
    font_header_title.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_header_title);
    font_header_subtitle.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_header_subtitle.weight = SCALE(22);
    font_header_subtitle.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_header_subtitle);
    font_large.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_large.weight = SCALE(22);
    font_large.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_large);
    font_button.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_button.weight = SCALE(22);
    font_button.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_button);
    font_body.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_body.weight = SCALE(14);
    font_body.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_body);
    font_status_label.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_status_label.weight = SCALE(20);
    font_status_label.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_status_label);
    font_status_value.name = SRC_FONT("SmileySans-Oblique.ttf");
    font_status_value.weight = SCALE(26);
    font_status_value.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_status_value);
    create_ui();
    lv_task_handler();
    state_timer = lv_timer_create(update_timer_cb, SERVO_STATE_POLL_MS, NULL);
    while (running) {
        lv_task_handler();
        usleep(5000);
    }
    return 0;
}
