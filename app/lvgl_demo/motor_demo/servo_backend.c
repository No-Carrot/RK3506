/*
 * Servo backend protocol adapter.
 *
 * EtherCAT and CiA402 motor control are intentionally delegated to the
 * Rockchip_MADHT1505BA1 driver copied from images/. That driver is the known
 * working bottom layer for this DS2-E axis: it owns the IgH master, PDO
 * mapping, realtime cyclic thread, startup takeover, DC sync and target
 * position exchange. This file only adapts the existing UI socket protocol to
 * that bottom-layer object.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "ethercat_master_ioctl.h"
#include "Rockchip_MADHT1505BA1.h"
#include "servo_protocol.h"

#define BACKEND_PERIOD_NS 20000000L
#define BACKEND_RETRY_CYCLES 50U
#define BACKEND_LINK_LOSS_CYCLES 50U
#define BACKEND_OP_REQUEST_STARTUP_GRACE_CYCLES 100U
#define BACKEND_OP_REQUEST_RETRY_CYCLES 50U
#define BACKEND_OP_REQUEST_MAX_RETRIES 12U
#define BACKEND_DRIVER_STARTUP_GRACE_CYCLES 500U
#define BACKEND_DEFAULT_RT_CPU SERVO_REALTIME_CPU
#define BACKEND_DEFAULT_PROCESS_CPU SERVO_BACKEND_CPU
#define BACKEND_MASTER_WAIT_US 10000
#define BACKEND_MASTER_WAIT_MAX_US 50000
#define BACKEND_MASTER_IDLE_STABLE_POLLS 2
#define BACKEND_MASTER_READY_MAX_POLLS 200U
#define MADHT_MODE_CSP 8
#define DEFAULT_MOTION_VELOCITY SERVO_DEFAULT_MOTION_PULSES_PER_SEC
#define DEFAULT_BACKEND_LOG_LEVEL 0U
#define DEFAULT_BACKEND_DIAG_CYCLES 0U
#define VELOCITY_MOVING_THRESHOLD 100
#define DEMO_MAX_ELAPSED_MS 200U
#define MOTION_TEST_TIMEOUT_MS 3000U
#define MOTION_TEST_TOLERANCE_PULSES 30
#define MOTION_TEST_STEP_PULSES \
    ((SERVO_ENCODER_PULSES_PER_REV * 10) / SERVO_DEGREES_PER_REV)

struct backend_diag_snapshot {
    uint16_t status_word;
    uint16_t control_word;
    uint16_t error_code;
    int32_t actual_position;
    int32_t target_position;
    int32_t pdo_target_position;
    int32_t drive_set_position;
    int32_t drive_target_position;
    int32_t actual_velocity;
    int8_t mode_display;
    uint8_t responding_slaves;
    uint8_t al_state;
    uint8_t slave_online;
    uint8_t slave_operational;
    uint8_t domain_wc_state;
    uint32_t domain_working_counter;
    uint32_t flags;
};

enum backend_motion_mode {
    BACKEND_MOTION_IDLE = 0,
    BACKEND_MOTION_TEST_TO_ZERO,
    BACKEND_MOTION_TEST_FORWARD,
    BACKEND_MOTION_TEST_RETURN_ZERO,
    BACKEND_MOTION_DEMO_FORWARD,
    BACKEND_MOTION_DEMO_REVERSE,
};

struct backend_runtime {
    MADHT1505BA1_object drive;
    struct servo_state state;
    struct backend_diag_snapshot last_diag;
    int diag_valid;
    int driver_started;
    int zero_calibrated;
    int restart_requested;
    int target_initialized;
    unsigned int link_down_cycles;
    unsigned int op_request_cycles;
    unsigned int op_request_retries;
    uint64_t driver_start_cycle;
    int32_t zero_offset;
    int32_t target_raw;
    int32_t motion_velocity;
    int32_t demo_rpm;
    enum backend_motion_mode motion_mode;
    int32_t motion_target_raw;
    uint64_t motion_deadline_ms;
    uint64_t demo_last_ms;
    int64_t demo_pulse_remainder;
    uint64_t cycle_count;
};

static volatile sig_atomic_t running = 1;
static unsigned int backend_log_level = DEFAULT_BACKEND_LOG_LEVEL;
static unsigned int backend_diag_cycles = DEFAULT_BACKEND_DIAG_CYCLES;
static int backend_rt_cpu = BACKEND_DEFAULT_RT_CPU;
static int backend_process_cpu = BACKEND_DEFAULT_PROCESS_CPU;

enum backend_master_phase {
    BACKEND_MASTER_PHASE_WAITING = 0,
    BACKEND_MASTER_PHASE_IDLE = 1,
    BACKEND_MASTER_PHASE_OPERATION = 2,
};

enum backend_master_wait_reason {
    BACKEND_MASTER_WAIT_NONE = 0,
    BACKEND_MASTER_WAIT_DEVICE,
    BACKEND_MASTER_WAIT_IDLE_PHASE,
    BACKEND_MASTER_WAIT_SCAN,
    BACKEND_MASTER_WAIT_ACTIVE,
    BACKEND_MASTER_WAIT_IDLE_STABLE,
    BACKEND_MASTER_WAIT_LINK,
    BACKEND_MASTER_WAIT_SLAVE,
};

static enum backend_master_wait_reason master_wait_reason =
    BACKEND_MASTER_WAIT_NONE;

static void signal_handler(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void bind_current_process_to_cpu(int target_cpu)
{
    cpu_set_t mask;
    long cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    if (target_cpu < 0 || (cpu_count > 0 && target_cpu >= cpu_count)) {
        fprintf(stderr, "servo_backend: CPU%d is not available\n", target_cpu);
        return;
    }

    CPU_ZERO(&mask);
    CPU_SET(target_cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
        perror("servo_backend: sched_setaffinity");
}

static void add_ns(struct timespec *time, long nanoseconds)
{
    time->tv_nsec += nanoseconds;
    while (time->tv_nsec >= 1000000000L) {
        time->tv_nsec -= 1000000000L;
        time->tv_sec++;
    }
}

static int64_t timespec_to_ns(const struct timespec *time)
{
    return (int64_t)time->tv_sec * 1000000000LL + time->tv_nsec;
}

static uint64_t monotonic_ms(void)
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000ULL +
        (uint64_t)time.tv_nsec / 1000000ULL;
}

static int64_t abs_i64(int64_t value)
{
    return value < 0 ? -value : value;
}

static int32_t clamp_i64_to_i32(int64_t value)
{
    if (value > INT32_MAX)
        return INT32_MAX;
    if (value < INT32_MIN)
        return INT32_MIN;
    return (int32_t)value;
}

static int32_t parse_positive_env_i32(const char *name, int32_t default_value)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (!value || !*value)
        return default_value;
    errno = 0;
    parsed = strtol(value, &end, 0);
    if (errno || end == value)
        return default_value;
    if (parsed < 0)
        parsed = -parsed;
    if (parsed == 0)
        parsed = default_value;
    if (parsed > INT32_MAX)
        return INT32_MAX;
    return (int32_t)parsed;
}

static unsigned int parse_env_u32(const char *name, unsigned int default_value,
                                  unsigned int max_value)
{
    const char *value = getenv(name);
    char *end = NULL;
    unsigned long parsed;

    if (!value || !*value)
        return default_value;
    errno = 0;
    parsed = strtoul(value, &end, 0);
    if (errno || end == value || *end)
        return default_value;
    if (parsed > max_value)
        return max_value;
    return (unsigned int)parsed;
}

static const char *command_name(int32_t command)
{
    switch (command) {
    case SERVO_CMD_NONE:
        return "none";
    case SERVO_CMD_ZERO_CALIBRATE:
        return "zero";
    case SERVO_CMD_HOME:
        return "home";
    case SERVO_CMD_STEP_POSITION:
        return "step";
    case SERVO_CMD_SELF_TEST:
        return "self_test";
    case SERVO_CMD_JOG_FORWARD:
        return "jog_forward";
    case SERVO_CMD_JOG_REVERSE:
        return "jog_reverse";
    case SERVO_CMD_STOP:
        return "stop";
    case SERVO_CMD_FAULT_RESET:
        return "fault_reset";
    case SERVO_CMD_SET_POSITION:
        return "set_position";
    default:
        return "unknown";
    }
}

static const char *motion_mode_name(enum backend_motion_mode mode)
{
    switch (mode) {
    case BACKEND_MOTION_TEST_TO_ZERO:
        return "test_zero";
    case BACKEND_MOTION_TEST_FORWARD:
        return "test_forward";
    case BACKEND_MOTION_TEST_RETURN_ZERO:
        return "test_return";
    case BACKEND_MOTION_DEMO_FORWARD:
        return "demo_forward";
    case BACKEND_MOTION_DEMO_REVERSE:
        return "demo_reverse";
    case BACKEND_MOTION_IDLE:
    default:
        return "idle";
    }
}

static const char *cia402_state_name(uint16_t status_word)
{
    if (status_word & 0x0008)
        return "fault";

    switch (status_word & 0x006f) {
    case 0x0040:
        return "switch_on_disabled";
    case 0x0021:
        return "ready_to_switch_on";
    case 0x0023:
        return "switched_on";
    case 0x0027:
        return "operation_enabled";
    case 0x0007:
        return "quick_stop_active";
    case 0x0000:
        return "not_ready";
    default:
        return "unknown";
    }
}

static const char *master_phase_name(uint8_t phase)
{
    switch (phase) {
    case BACKEND_MASTER_PHASE_WAITING:
        return "waiting";
    case BACKEND_MASTER_PHASE_IDLE:
        return "idle";
    case BACKEND_MASTER_PHASE_OPERATION:
        return "operation";
    default:
        return "unknown";
    }
}

static int query_master_info(ec_ioctl_master_t *master_info)
{
    int fd;
    int ret;

    if (!master_info)
        return -1;

    fd = open("/dev/EtherCAT0", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    memset(master_info, 0, sizeof(*master_info));
    ret = ioctl(fd, EC_IOCTL_MASTER, master_info);
    close(fd);
    return ret;
}

static int master_main_link_up(const ec_ioctl_master_t *master_info)
{
    if (!master_info || master_info->num_devices == 0)
        return 0;

    return master_info->devices[0].attached &&
        master_info->devices[0].link_state;
}

static int wait_for_master_idle(void)
{
    ec_ioctl_master_t master_info;
    unsigned int idle_stable_polls = 0;
    unsigned int ready_wait_polls = 0;
    unsigned int wait_us = BACKEND_MASTER_WAIT_US;

    while (running) {
        if (query_master_info(&master_info) != 0) {
            idle_stable_polls = 0;
            if (master_wait_reason != BACKEND_MASTER_WAIT_DEVICE) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT control device\n");
                master_wait_reason = BACKEND_MASTER_WAIT_DEVICE;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (master_info.active) {
            idle_stable_polls = 0;
            if (master_wait_reason != BACKEND_MASTER_WAIT_ACTIVE) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT master release "
                        "(phase=%s active=%u scan_busy=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy);
                master_wait_reason = BACKEND_MASTER_WAIT_ACTIVE;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (master_info.phase != BACKEND_MASTER_PHASE_IDLE) {
            idle_stable_polls = 0;
            if (master_wait_reason != BACKEND_MASTER_WAIT_IDLE_PHASE) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT master idle "
                        "(phase=%s active=%u scan_busy=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy);
                master_wait_reason = BACKEND_MASTER_WAIT_IDLE_PHASE;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (master_info.scan_busy) {
            idle_stable_polls = 0;
            if (master_wait_reason != BACKEND_MASTER_WAIT_SCAN) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT bus scan to finish "
                        "(phase=%s active=%u scan_busy=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy);
                master_wait_reason = BACKEND_MASTER_WAIT_SCAN;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (!master_main_link_up(&master_info)) {
            idle_stable_polls = 0;
            ready_wait_polls++;
            if (master_wait_reason != BACKEND_MASTER_WAIT_LINK) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT main link "
                        "(phase=%s active=%u scan_busy=%u slaves=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy,
                        master_info.slave_count);
                master_wait_reason = BACKEND_MASTER_WAIT_LINK;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            if (ready_wait_polls >= BACKEND_MASTER_READY_MAX_POLLS) {
                fprintf(stderr,
                        "servo_backend: EtherCAT main link not ready after "
                        "%u polls\n", ready_wait_polls);
                return -1;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (master_info.slave_count == 0) {
            idle_stable_polls = 0;
            ready_wait_polls++;
            if (master_wait_reason != BACKEND_MASTER_WAIT_SLAVE) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT slave scan "
                        "(phase=%s active=%u scan_busy=%u link=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy,
                        master_main_link_up(&master_info));
                master_wait_reason = BACKEND_MASTER_WAIT_SLAVE;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            if (ready_wait_polls >= BACKEND_MASTER_READY_MAX_POLLS) {
                fprintf(stderr,
                        "servo_backend: no EtherCAT slaves after %u polls\n",
                        ready_wait_polls);
                return -1;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        idle_stable_polls++;
        if (idle_stable_polls < BACKEND_MASTER_IDLE_STABLE_POLLS) {
            if (master_wait_reason != BACKEND_MASTER_WAIT_IDLE_STABLE) {
                fprintf(stderr,
                        "servo_backend: waiting for EtherCAT idle state to "
                        "stabilize (phase=%s active=%u scan_busy=%u)\n",
                        master_phase_name(master_info.phase),
                        master_info.active, master_info.scan_busy);
                master_wait_reason = BACKEND_MASTER_WAIT_IDLE_STABLE;
                wait_us = BACKEND_MASTER_WAIT_US;
            }
            usleep(wait_us);
            if (wait_us < BACKEND_MASTER_WAIT_MAX_US) {
                wait_us <<= 1;
                if (wait_us > BACKEND_MASTER_WAIT_MAX_US)
                    wait_us = BACKEND_MASTER_WAIT_MAX_US;
            }
            continue;
        }

        if (master_wait_reason != BACKEND_MASTER_WAIT_NONE) {
            fprintf(stderr,
                    "servo_backend: EtherCAT master idle state is stable\n");
            master_wait_reason = BACKEND_MASTER_WAIT_NONE;
        }
        return 0;
    }

    return -1;
}

static int create_server(void)
{
    struct sockaddr_un address;
    int fd;

    unlink(SERVO_SOCKET_PATH);
    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
        return -1;

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SERVO_SOCKET_PATH, sizeof(address.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) {
        close(fd);
        unlink(SERVO_SOCKET_PATH);
        return -1;
    }
    chmod(SERVO_SOCKET_PATH, 0660);
    return fd;
}

static void configure_drive_motion(struct backend_runtime *runtime)
{
    runtime->drive.step_numerator = (unsigned int)runtime->motion_velocity;
    runtime->drive.step_denominator = MADHT1505BA1_cycle_hz();
    runtime->drive.step_remainder = 0;
}

static void stop_driver(struct backend_runtime *runtime)
{
    if (!runtime->driver_started)
        return;

    MADHT1505BA1_master_deinit();
    runtime->driver_started = 0;
    runtime->target_initialized = 0;
    runtime->link_down_cycles = 0;
    runtime->op_request_cycles = 0;
    runtime->op_request_retries = 0;
    runtime->motion_mode = BACKEND_MOTION_IDLE;
    runtime->motion_deadline_ms = 0;
    runtime->demo_pulse_remainder = 0;
}

static int start_driver(struct backend_runtime *runtime)
{
    int ret;

    if (runtime->driver_started)
        return 0;
    if (access("/dev/EtherCAT0", R_OK | W_OK) != 0)
        return -1;

    memset(&runtime->drive, 0, sizeof(runtime->drive));
    runtime->drive.alias = 0;
    runtime->drive.position = 0;
    runtime->target_initialized = 0;
    configure_drive_motion(runtime);

    if (wait_for_master_idle() < 0)
        return -1;

    ret = MADHT1505BA1_master_init(backend_rt_cpu);
    if (ret < 0)
        return -1;

    ret = MADHT1505BA1_slaves_init(&runtime->drive);
    if (ret < 0)
        goto fail;

    /*
     * MADHT1505BA1_slaves_init() fills PDO registrations and clears runtime
     * motion fields, so install the adapter's requested velocity after that
     * step as well.
     */
    configure_drive_motion(runtime);

    ret = MADHT1505BA1_master_activate();
    if (ret < 0)
        goto fail;
    ret = MADHT1505BA1_slaves_activate(&runtime->drive);
    if (ret < 0)
        goto fail;
    ret = MADHT1505BA1_slave_start(1, &runtime->drive);
    if (ret < 0)
        goto fail;

    runtime->driver_started = 1;
    runtime->driver_start_cycle = runtime->cycle_count;
    runtime->link_down_cycles = 0;
    runtime->op_request_cycles = 0;
    runtime->op_request_retries = 0;
    fprintf(stderr,
            "servo_backend: MADHT driver started "
            "(mode=csp/8 sync=%s layout=madht-1600+1601 "
            "cycle_hz=%u motion_velocity=%ld pul/s step=%u/%u pul/cycle)\n",
            getenv("SERVO_BACKEND_SYNC") ? getenv("SERVO_BACKEND_SYNC") : "dc",
            MADHT1505BA1_cycle_hz(),
            (long)runtime->motion_velocity,
            runtime->drive.step_numerator,
            runtime->drive.step_denominator);
    return 0;

fail:
    MADHT1505BA1_master_deinit();
    fprintf(stderr, "servo_backend: MADHT driver init failed\n");
    return -1;
}

static int read_driver_state(struct backend_runtime *runtime,
                             struct servo_state *state)
{
    if (!runtime || !state)
        return -1;
    memset(state, 0, sizeof(*state));
    state->version = SERVO_PROTOCOL_VERSION;
    state->mode_display = MADHT_MODE_CSP;
    if (!runtime->driver_started)
        return -1;
    return MADHT1505BA1_get_state_snapshot(&runtime->drive, state);
}

static int state_drive_ready(const struct servo_state *state)
{
    return state && (state->flags & SERVO_FLAG_BUS_OP) &&
        !(state->flags & SERVO_FLAG_FAULT);
}

static int32_t read_actual_position(struct backend_runtime *runtime)
{
    struct servo_state state;

    if (read_driver_state(runtime, &state) == 0)
        return state.actual_position;
    return runtime->target_raw;
}

static void set_motor_target(struct backend_runtime *runtime, int32_t target_raw)
{
    runtime->target_raw = target_raw;
    runtime->target_initialized = 1;
    if (runtime->driver_started)
        MADHT1505BA1_motor_set_position_run(target_raw, &runtime->drive);
}

static void clear_motion(struct backend_runtime *runtime)
{
    runtime->motion_mode = BACKEND_MOTION_IDLE;
    runtime->motion_target_raw = runtime->target_raw;
    runtime->motion_deadline_ms = 0;
    runtime->demo_last_ms = 0;
    runtime->demo_pulse_remainder = 0;
}

static void stop_motion_at_actual(struct backend_runtime *runtime,
                                  const char *reason)
{
    int32_t actual_raw = read_actual_position(runtime);

    set_motor_target(runtime, actual_raw);
    clear_motion(runtime);
    if (reason)
        fprintf(stderr, "servo_backend: motion stopped (%s) actual=%ld\n",
                reason, (long)actual_raw);
}

static void initialize_target_from_actual(struct backend_runtime *runtime,
                                          int32_t actual_raw)
{
    if (!runtime->zero_calibrated) {
        /*
         * The known-good motor_demo captures the current encoder position as
         * its software zero when the drive first becomes ready. Keep the raw
         * target at that same position so startup never commands raw position
         * zero by accident.
         */
        runtime->zero_offset = actual_raw;
        runtime->zero_calibrated = 1;
        fprintf(stderr,
                "servo_backend: zero reference initialized from actual "
                "position %ld\n", (long)actual_raw);
    }

    if (runtime->target_initialized)
        return;

    runtime->target_raw = actual_raw;
    runtime->target_initialized = 1;
    if (runtime->driver_started)
        MADHT1505BA1_motor_set_position_run(actual_raw, &runtime->drive);
    fprintf(stderr,
            "servo_backend: target initialized from actual position %ld\n",
            (long)actual_raw);
}

static void start_test_step(struct backend_runtime *runtime,
                            enum backend_motion_mode mode,
                            int32_t target_raw)
{
    runtime->motion_mode = mode;
    runtime->motion_target_raw = target_raw;
    runtime->motion_deadline_ms = monotonic_ms() + MOTION_TEST_TIMEOUT_MS;
    set_motor_target(runtime, target_raw);
    fprintf(stderr,
            "servo_backend: test step %s target=%ld deadline=%llu\n",
            motion_mode_name(mode),
            (long)target_raw,
            (unsigned long long)runtime->motion_deadline_ms);
}

static void start_self_test(struct backend_runtime *runtime, int32_t actual_raw)
{
    if (!runtime->zero_calibrated) {
        runtime->zero_offset = actual_raw;
        runtime->zero_calibrated = 1;
    }

    clear_motion(runtime);
    start_test_step(runtime, BACKEND_MOTION_TEST_TO_ZERO,
                    runtime->zero_offset);
}

static void start_demo(struct backend_runtime *runtime, bool forward,
                       int32_t actual_raw)
{
    clear_motion(runtime);
    runtime->motion_mode = forward ? BACKEND_MOTION_DEMO_FORWARD :
        BACKEND_MOTION_DEMO_REVERSE;
    runtime->demo_last_ms = monotonic_ms();
    runtime->demo_pulse_remainder = 0;
    set_motor_target(runtime, actual_raw);
    fprintf(stderr,
            "servo_backend: %s started at raw=%ld speed=%d rpm\n",
            motion_mode_name(runtime->motion_mode),
            (long)actual_raw, runtime->demo_rpm);
}

static void advance_motion(struct backend_runtime *runtime)
{
    struct servo_state state;
    uint64_t now_ms;
    int32_t actual_raw;
    uint16_t status_word;

    if (runtime->motion_mode == BACKEND_MOTION_IDLE)
        return;

    if (read_driver_state(runtime, &state) != 0 ||
        !state_drive_ready(&state)) {
        stop_motion_at_actual(runtime, "drive_not_ready");
        return;
    }

    status_word = state.status_word;
    if (status_word & 0x0008) {
        stop_motion_at_actual(runtime, "fault");
        return;
    }

    actual_raw = state.actual_position;
    now_ms = monotonic_ms();

    switch (runtime->motion_mode) {
    case BACKEND_MOTION_TEST_TO_ZERO:
    case BACKEND_MOTION_TEST_FORWARD:
    case BACKEND_MOTION_TEST_RETURN_ZERO:
        if (now_ms > runtime->motion_deadline_ms) {
            stop_motion_at_actual(runtime, "test_timeout");
            return;
        }
        if (abs_i64((int64_t)actual_raw - runtime->motion_target_raw) >
                MOTION_TEST_TOLERANCE_PULSES ||
            !(status_word & 0x0400))
            return;

        if (runtime->motion_mode == BACKEND_MOTION_TEST_TO_ZERO) {
            start_test_step(runtime, BACKEND_MOTION_TEST_FORWARD,
                            clamp_i64_to_i32(
                                (int64_t)runtime->zero_offset +
                                MOTION_TEST_STEP_PULSES));
        } else if (runtime->motion_mode == BACKEND_MOTION_TEST_FORWARD) {
            start_test_step(runtime, BACKEND_MOTION_TEST_RETURN_ZERO,
                            runtime->zero_offset);
        } else {
            fprintf(stderr, "servo_backend: one-key test passed\n");
            clear_motion(runtime);
        }
        break;

    case BACKEND_MOTION_DEMO_FORWARD:
    case BACKEND_MOTION_DEMO_REVERSE:
    {
        uint64_t elapsed_ms = now_ms - runtime->demo_last_ms;
        int64_t pulse_delta;

        if (elapsed_ms > DEMO_MAX_ELAPSED_MS)
            elapsed_ms = DEMO_MAX_ELAPSED_MS;
        runtime->demo_last_ms = now_ms;
        runtime->demo_pulse_remainder +=
            (int64_t)elapsed_ms * SERVO_ENCODER_PULSES_PER_REV *
            runtime->demo_rpm;
        pulse_delta = runtime->demo_pulse_remainder / 60000;
        runtime->demo_pulse_remainder %= 60000;
        if (pulse_delta <= 0)
            return;
        /*
         * Keep continuous jog direction aligned with the position controls:
         * forward increases target position, reverse decreases it.
         */
        if (runtime->motion_mode == BACKEND_MOTION_DEMO_REVERSE)
            pulse_delta = -pulse_delta;
        set_motor_target(runtime, clamp_i64_to_i32(
            (int64_t)runtime->target_raw + pulse_delta));
        break;
    }

    case BACKEND_MOTION_IDLE:
    default:
        clear_motion(runtime);
        break;
    }
}

static int diag_changed(const struct backend_runtime *runtime,
                        const struct backend_diag_snapshot *current)
{
    const struct backend_diag_snapshot *last = &runtime->last_diag;

    if (!runtime->diag_valid)
        return 1;
    return current->status_word != last->status_word ||
        current->control_word != last->control_word ||
        current->error_code != last->error_code ||
        current->mode_display != last->mode_display ||
        current->responding_slaves != last->responding_slaves ||
        current->al_state != last->al_state ||
        current->slave_online != last->slave_online ||
        current->slave_operational != last->slave_operational ||
        current->domain_wc_state != last->domain_wc_state ||
        current->domain_working_counter != last->domain_working_counter ||
        current->flags != last->flags;
}

static void log_diagnostics(struct backend_runtime *runtime,
                            const struct backend_diag_snapshot *current)
{
    int periodic;

    if (backend_log_level < 2 || backend_diag_cycles == 0) {
        runtime->last_diag = *current;
        runtime->diag_valid = 1;
        return;
    }

    periodic = runtime->cycle_count % backend_diag_cycles == 0;
    if (!periodic && !diag_changed(runtime, current))
        return;

    fprintf(stderr,
            "servo_backend: diag cycle=%llu link=%u slaves=%u "
            "al=0x%02x online=%u operational=%u wc=%lu/%u "
            "mode=%d/csp sw=0x%04x(%s) cw=0x%04x "
            "err=0x%04x pos_abs actual=%ld target=%ld "
            "pos_rel actual=%ld target=%ld vel actual=%ld target=%ld "
            "pdo_target=%ld drv_set=%ld drv_target=%ld "
            "motion=%s ready=%u flags=0x%08lx\n",
            (unsigned long long)runtime->cycle_count,
            (current->flags & SERVO_FLAG_LINK_UP) != 0,
            current->responding_slaves,
            current->al_state,
            current->slave_online,
            current->slave_operational,
            (unsigned long)current->domain_working_counter,
            current->domain_wc_state,
            current->mode_display,
            current->status_word,
            cia402_state_name(current->status_word),
            current->control_word,
            current->error_code,
            (long)current->actual_position,
            (long)runtime->target_raw,
            (long)(current->actual_position - runtime->zero_offset),
            (long)(runtime->target_raw - runtime->zero_offset),
            (long)current->actual_velocity,
            (long)runtime->motion_velocity,
            (long)current->pdo_target_position,
            (long)current->drive_set_position,
            (long)current->drive_target_position,
            motion_mode_name(runtime->motion_mode),
            (current->flags & SERVO_FLAG_BUS_OP) != 0,
            (unsigned long)current->flags);

    runtime->last_diag = *current;
    runtime->diag_valid = 1;
}

static void update_state(struct backend_runtime *runtime)
{
    struct backend_diag_snapshot diag;
    struct servo_state driver_state;
    int have_state = read_driver_state(runtime, &driver_state) == 0;
    int32_t actual_raw = have_state ? driver_state.actual_position :
        runtime->target_raw;
    int32_t actual_velocity = have_state ? driver_state.actual_velocity : 0;
    uint16_t status_word = have_state ? driver_state.status_word : 0;
    int8_t mode_display = have_state ? driver_state.mode_display :
        MADHT_MODE_CSP;
    uint16_t control_word = have_state ? driver_state.control_word : 0;
    uint16_t error_code = have_state ? driver_state.error_code : 0;
    uint32_t flags = have_state ? driver_state.flags : 0;
    int ready = have_state && state_drive_ready(&driver_state);

    if (runtime->zero_calibrated &&
        abs_i64((int64_t)actual_raw - runtime->zero_offset) <=
            SERVO_POSITION_TOLERANCE_PULSES &&
        abs_i64(actual_velocity) <= VELOCITY_MOVING_THRESHOLD)
        flags |= SERVO_FLAG_HOMED;

    if (ready)
        initialize_target_from_actual(runtime, actual_raw);

    runtime->state.version = SERVO_PROTOCOL_VERSION;
    runtime->state.flags = flags;
    runtime->state.actual_position =
        clamp_i64_to_i32((int64_t)actual_raw - runtime->zero_offset);
    runtime->state.target_position =
        clamp_i64_to_i32((int64_t)runtime->target_raw -
                         runtime->zero_offset);
    runtime->state.actual_velocity = actual_velocity;
    runtime->state.target_velocity =
        abs_i64((int64_t)runtime->target_raw - actual_raw) >
        SERVO_POSITION_TOLERANCE_PULSES ? runtime->motion_velocity : 0;
    runtime->state.following_error =
        clamp_i64_to_i32((int64_t)runtime->target_raw - actual_raw);
    runtime->state.status_word = status_word;
    runtime->state.error_code = error_code;
    runtime->state.control_word = control_word;
    runtime->state.mode_display = mode_display;
    runtime->state.responding_slaves = have_state ?
        driver_state.responding_slaves : 0;
    runtime->state.jitter_us = have_state ? driver_state.jitter_us : 0;
    runtime->state.ethercat_frequency_hz = have_state ?
        driver_state.ethercat_frequency_hz : 0;
    runtime->state.ethercat_period_min_us = have_state ?
        driver_state.ethercat_period_min_us : 0;
    runtime->state.ethercat_period_max_us = have_state ?
        driver_state.ethercat_period_max_us : 0;
    runtime->state.wakeup_latency_max_us = have_state ?
        driver_state.wakeup_latency_max_us : 0;
    runtime->state.cycle_exec_max_us = have_state ?
        driver_state.cycle_exec_max_us : 0;
    runtime->state.timing_samples = have_state ?
        driver_state.timing_samples : 0;
    runtime->state.deadline_misses = have_state ?
        driver_state.deadline_misses : 0;
    runtime->state.al_state = have_state ? driver_state.al_state : 0;
    runtime->state.slave_online = have_state ? driver_state.slave_online : 0;
    runtime->state.slave_operational = have_state ?
        driver_state.slave_operational : 0;
    runtime->state.domain_wc_state = have_state ?
        driver_state.domain_wc_state : 0;
    runtime->state.domain_working_counter = have_state ?
        driver_state.domain_working_counter : 0;

    diag.status_word = status_word;
    diag.control_word = control_word;
    diag.error_code = error_code;
    diag.actual_position = actual_raw;
    diag.target_position = runtime->target_raw;
    diag.pdo_target_position = have_state ? driver_state.target_position :
        runtime->target_raw;
    diag.drive_set_position = diag.pdo_target_position;
    diag.drive_target_position = diag.pdo_target_position;
    diag.actual_velocity = actual_velocity;
    diag.mode_display = mode_display;
    diag.responding_slaves = runtime->state.responding_slaves;
    diag.al_state = runtime->state.al_state;
    diag.slave_online = runtime->state.slave_online;
    diag.slave_operational = runtime->state.slave_operational;
    diag.domain_wc_state = runtime->state.domain_wc_state;
    diag.domain_working_counter = runtime->state.domain_working_counter;
    diag.flags = flags;
    log_diagnostics(runtime, &diag);
}

static void maybe_retry_slave_op(struct backend_runtime *runtime)
{
    uint64_t driver_cycles;

    if (!runtime->driver_started) {
        runtime->op_request_cycles = 0;
        runtime->op_request_retries = 0;
        return;
    }

    if ((runtime->state.flags & SERVO_FLAG_BUS_OP) ||
        runtime->state.slave_operational ||
        runtime->state.al_state == EC_AL_STATE_OP ||
        runtime->state.domain_wc_state == EC_WC_COMPLETE) {
        runtime->op_request_cycles = 0;
        runtime->op_request_retries = 0;
        return;
    }

    if (!(runtime->state.flags & SERVO_FLAG_LINK_UP) ||
        runtime->state.responding_slaves == 0 ||
        !runtime->state.slave_online ||
        runtime->state.al_state != EC_AL_STATE_PREOP ||
        runtime->state.domain_wc_state != EC_WC_ZERO ||
        runtime->state.domain_working_counter != 0) {
        runtime->op_request_cycles = 0;
        return;
    }

    driver_cycles = runtime->cycle_count - runtime->driver_start_cycle;
    if (driver_cycles < BACKEND_OP_REQUEST_STARTUP_GRACE_CYCLES) {
        runtime->op_request_cycles = 0;
        return;
    }

    if (runtime->op_request_retries >= BACKEND_OP_REQUEST_MAX_RETRIES)
        return;

    runtime->op_request_cycles++;
    if (runtime->op_request_cycles < BACKEND_OP_REQUEST_RETRY_CYCLES)
        return;

    runtime->op_request_cycles = 0;
    runtime->op_request_retries++;
    fprintf(stderr,
            "servo_backend: retrying EtherCAT OP request %u/%u "
            "(al=0x%02x wc=%lu/%u slaves=%u flags=0x%08lx)\n",
            runtime->op_request_retries,
            BACKEND_OP_REQUEST_MAX_RETRIES,
            runtime->state.al_state,
            (unsigned long)runtime->state.domain_working_counter,
            runtime->state.domain_wc_state,
            runtime->state.responding_slaves,
            (unsigned long)runtime->state.flags);
    MADHT1505BA1_request_op_retry("backend preop-wc0 retry");
}

static int monitor_driver_link(struct backend_runtime *runtime)
{
    ec_ioctl_master_t master_info;
    uint64_t driver_cycles;

    if (!runtime->driver_started) {
        runtime->link_down_cycles = 0;
        return 0;
    }

    if ((runtime->state.flags & SERVO_FLAG_LINK_UP) ||
        runtime->state.responding_slaves > 0) {
        runtime->link_down_cycles = 0;
        return 0;
    }

    if (query_master_info(&master_info) == 0 &&
        master_main_link_up(&master_info)) {
        runtime->link_down_cycles = 0;
        return 0;
    }

    driver_cycles = runtime->cycle_count - runtime->driver_start_cycle;
    if (driver_cycles < BACKEND_DRIVER_STARTUP_GRACE_CYCLES) {
        runtime->link_down_cycles = 0;
        return 0;
    }

    runtime->link_down_cycles++;
    if (runtime->link_down_cycles < BACKEND_LINK_LOSS_CYCLES)
        return 0;

    if (query_master_info(&master_info) == 0) {
        fprintf(stderr,
                "servo_backend: EtherCAT link lost "
                "(flags=0x%08lx slaves=%u master_link=%u "
                "master_slaves=%u phase=%s active=%u scan_busy=%u), "
                "stopping driver\n",
                (unsigned long)runtime->state.flags,
                runtime->state.responding_slaves,
                master_main_link_up(&master_info),
                master_info.slave_count,
                master_phase_name(master_info.phase),
                master_info.active,
                master_info.scan_busy);
    } else {
        fprintf(stderr,
                "servo_backend: EtherCAT link lost "
                "(flags=0x%08lx slaves=%u master=unreadable), "
                "stopping driver\n",
                (unsigned long)runtime->state.flags,
                runtime->state.responding_slaves);
    }
    stop_driver(runtime);
    memset(&runtime->state, 0, sizeof(runtime->state));
    runtime->state.version = SERVO_PROTOCOL_VERSION;
    runtime->state.mode_display = MADHT_MODE_CSP;
    return 1;
}

static void handle_command(struct backend_runtime *runtime, int32_t command,
                           int32_t argument)
{
    struct servo_state state;
    int have_state = read_driver_state(runtime, &state) == 0;
    int32_t actual_raw = have_state ? state.actual_position :
        runtime->target_raw;
    int32_t target_raw = runtime->target_raw;
    uint16_t status_word = have_state ? state.status_word : 0;
    int drive_ready = have_state && state_drive_ready(&state);

    if (command != SERVO_CMD_NONE)
        clear_motion(runtime);

    if (command != SERVO_CMD_NONE &&
        command != SERVO_CMD_STOP &&
        command != SERVO_CMD_FAULT_RESET &&
        (!drive_ready || (status_word & 0x0008))) {
        if (backend_log_level >= 1)
            fprintf(stderr,
                    "servo_backend: reject command %s(%ld), drive_ready=%d "
                    "status=0x%04x\n",
                    command_name(command), (long)argument, drive_ready,
                    status_word);
        return;
    }

    switch (command) {
    case SERVO_CMD_ZERO_CALIBRATE:
        runtime->zero_offset = actual_raw;
        runtime->zero_calibrated = 1;
        target_raw = actual_raw;
        break;
    case SERVO_CMD_HOME:
        if (!runtime->zero_calibrated) {
            runtime->zero_offset = actual_raw;
            runtime->zero_calibrated = 1;
        }
        /*
         * Return to the captured raw reference. Do not wrap the target to
         * another equivalent revolution: that was the reason HOME could
         * become a no-op after a previous zero calibration.
         */
        target_raw = runtime->zero_offset;
        break;
    case SERVO_CMD_STEP_POSITION:
        target_raw = clamp_i64_to_i32((int64_t)runtime->target_raw +
                                      argument);
        break;
    case SERVO_CMD_SET_POSITION:
        target_raw = clamp_i64_to_i32((int64_t)runtime->zero_offset +
                                      argument);
        break;
    case SERVO_CMD_SELF_TEST:
        start_self_test(runtime, actual_raw);
        target_raw = runtime->target_raw;
        break;
    case SERVO_CMD_JOG_FORWARD:
        start_demo(runtime, true, actual_raw);
        target_raw = runtime->target_raw;
        break;
    case SERVO_CMD_JOG_REVERSE:
        start_demo(runtime, false, actual_raw);
        target_raw = runtime->target_raw;
        break;
    case SERVO_CMD_STOP:
        target_raw = actual_raw;
        break;
    case SERVO_CMD_FAULT_RESET:
        runtime->restart_requested = 1;
        target_raw = actual_raw;
        break;
    default:
        break;
    }

    set_motor_target(runtime, target_raw);
    if (backend_log_level >= 2 ||
        (backend_log_level >= 1 && command != SERVO_CMD_NONE)) {
        fprintf(stderr,
                "servo_backend: command %s(%ld) actual=%ld target=%ld "
                "motion=%s motion_velocity=%ld status=0x%04x\n",
                command_name(command),
                (long)argument,
                (long)actual_raw,
                (long)target_raw,
                motion_mode_name(runtime->motion_mode),
                (long)runtime->motion_velocity,
                status_word);
    }
}

static void serve_clients(int server_fd, struct backend_runtime *runtime)
{
    unsigned int handled = 0;

    while (handled++ < 8) {
        struct sockaddr_un peer;
        socklen_t peer_len = sizeof(peer);
        struct servo_request request;
        int client_fd;
        ssize_t count;

        client_fd = accept4(server_fd, (struct sockaddr *)&peer, &peer_len,
                            SOCK_NONBLOCK);
        if (client_fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                perror("servo_backend: accept");
            return;
        }

        count = recv(client_fd, &request, sizeof(request), 0);
        if (count == (ssize_t)sizeof(request) &&
            request.version == SERVO_PROTOCOL_VERSION &&
            request.type == SERVO_REQUEST_COMMAND) {
            handle_command(runtime, request.command, request.argument);
            update_state(runtime);
        }
        send(client_fd, &runtime->state, sizeof(runtime->state), MSG_NOSIGNAL);
        close(client_fd);
    }
}

int main(void)
{
    struct backend_runtime runtime;
    struct timespec wakeup_time;
    int server_fd;

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    memset(&runtime, 0, sizeof(runtime));
    backend_rt_cpu = (int)parse_env_u32("SERVO_REALTIME_CPU",
                                        BACKEND_DEFAULT_RT_CPU,
                                        CPU_SETSIZE - 1);
    backend_process_cpu = (int)parse_env_u32("SERVO_BACKEND_CPU",
                                             BACKEND_DEFAULT_PROCESS_CPU,
                                             CPU_SETSIZE - 1);
    bind_current_process_to_cpu(backend_process_cpu);
    backend_log_level = parse_env_u32("SERVO_BACKEND_LOG_LEVEL",
                                      DEFAULT_BACKEND_LOG_LEVEL, 2);
    backend_diag_cycles = parse_env_u32("SERVO_BACKEND_DIAG_CYCLES",
                                        DEFAULT_BACKEND_DIAG_CYCLES,
                                        UINT_MAX);
    runtime.motion_velocity = parse_positive_env_i32(
        "SERVO_BACKEND_MOTION_VELOCITY", DEFAULT_MOTION_VELOCITY);
    runtime.demo_rpm = parse_positive_env_i32("SERVO_BACKEND_DEMO_RPM",
                                             SERVO_DEFAULT_JOG_RPM);
    runtime.state.version = SERVO_PROTOCOL_VERSION;
    runtime.state.mode_display = MADHT_MODE_CSP;
    runtime.target_raw = 0;

    server_fd = create_server();
    if (server_fd < 0) {
        perror("servo_backend: create socket");
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stderr,
            "servo_backend: using MADHT1505BA1 bottom driver "
            "(mode=csp/8 sync=%s pdo=1600+1601 tx=1a00+1a01 "
            "motion_velocity=%ld pul/s demo_rpm=%ld "
            "backend_cpu=%d rt_cpu=%d)\n",
            getenv("SERVO_BACKEND_SYNC") ? getenv("SERVO_BACKEND_SYNC") : "dc",
            (long)runtime.motion_velocity,
            (long)runtime.demo_rpm,
            backend_process_cpu,
            backend_rt_cpu);

    clock_gettime(CLOCK_MONOTONIC, &wakeup_time);
    while (running) {
        struct timespec now;
        int64_t lateness_ns;

        add_ns(&wakeup_time, BACKEND_PERIOD_NS);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeup_time, NULL);
        clock_gettime(CLOCK_MONOTONIC, &now);
        lateness_ns = timespec_to_ns(&now) - timespec_to_ns(&wakeup_time);
        if (lateness_ns > BACKEND_PERIOD_NS)
            wakeup_time = now;

        runtime.cycle_count++;
        if (runtime.restart_requested) {
            stop_driver(&runtime);
            runtime.restart_requested = 0;
        }
        if (!runtime.driver_started &&
            (runtime.cycle_count == 1 ||
             runtime.cycle_count % BACKEND_RETRY_CYCLES == 0)) {
            start_driver(&runtime);
        }

        update_state(&runtime);
        maybe_retry_slave_op(&runtime);
        monitor_driver_link(&runtime);
        serve_clients(server_fd, &runtime);
        advance_motion(&runtime);
    }

    stop_driver(&runtime);
    close(server_fd);
    unlink(SERVO_SOCKET_PATH);
    return 0;
}
