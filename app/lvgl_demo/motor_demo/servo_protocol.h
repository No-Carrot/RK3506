#ifndef SERVO_PROTOCOL_H
#define SERVO_PROTOCOL_H

#include <stdint.h>

#define SERVO_SOCKET_PATH "/run/servo_backend.sock"
#define SERVO_PROTOCOL_VERSION 4U
#define SERVO_ETHERCAT_CYCLE_HZ_2K 2000U
#define SERVO_ETHERCAT_CYCLE_HZ_4K 4000U
#define SERVO_ETHERCAT_CYCLE_HZ SERVO_ETHERCAT_CYCLE_HZ_2K
#define SERVO_REALTIME_CPU 2
#define SERVO_BACKEND_CPU 0
#define SERVO_UI_CPU 1
#define SERVO_JITTER_REFRESH_MS 500U
/*
 * DS2-E command units follow object 6091. This machine uses the 17-bit
 * magnetic encoder configuration, 6091-01 = 131072 and 6091-02 = 10000, so
 * one commanded shaft revolution is 10000 position units.
 */
#define SERVO_MAGNETIC_ENCODER_COUNTS_PER_REV 131072
#define SERVO_COMMAND_PULSES_PER_REV 10000
#define SERVO_ENCODER_PULSES_PER_REV SERVO_COMMAND_PULSES_PER_REV
#define SERVO_DEGREES_PER_REV 360
#define SERVO_DEFAULT_MOTION_RPM 3000
#define SERVO_DEFAULT_JOG_RPM 6
#define SERVO_DEFAULT_MOTION_PULSES_PER_SEC \
    ((SERVO_DEFAULT_MOTION_RPM * SERVO_COMMAND_PULSES_PER_REV) / 60)
#define SERVO_POSITION_TOLERANCE_PULSES \
    (SERVO_ENCODER_PULSES_PER_REV / (SERVO_DEGREES_PER_REV * 10))
#define SERVO_UI_ARRIVED_TOLERANCE_PULSES \
    (SERVO_ENCODER_PULSES_PER_REV / (SERVO_DEGREES_PER_REV * 2))

enum servo_request_type {
    SERVO_REQUEST_STATE = 1,
    SERVO_REQUEST_COMMAND,
};

enum servo_command {
    SERVO_CMD_NONE = 0,
    SERVO_CMD_ZERO_CALIBRATE,
    SERVO_CMD_HOME,
    SERVO_CMD_STEP_POSITION,
    SERVO_CMD_SELF_TEST,
    SERVO_CMD_JOG_FORWARD,
    SERVO_CMD_JOG_REVERSE,
    SERVO_CMD_STOP,
    SERVO_CMD_FAULT_RESET,
    SERVO_CMD_SET_POSITION,
};

enum servo_flags {
    SERVO_FLAG_LINK_UP = 1U << 0,
    SERVO_FLAG_BUS_OP = 1U << 1,
    SERVO_FLAG_ENABLED = 1U << 2,
    SERVO_FLAG_HOMED = 1U << 3,
    SERVO_FLAG_FAULT = 1U << 4,
    SERVO_FLAG_MOVING = 1U << 5,
};

struct servo_request {
    uint32_t version;
    uint32_t type;
    int32_t command;
    int32_t argument;
};

struct servo_state {
    uint32_t version;
    uint32_t flags;
    int32_t actual_position;      /* CiA402 0x6064, command pulses */
    int32_t target_position;      /* CiA402 0x607A, command pulses */
    int32_t actual_velocity;      /* CiA402 0x606C, pulses/s */
    int32_t target_velocity;      /* CiA402 0x60FF, pulses/s */
    int32_t following_error;      /* CiA402 0x60F4, command pulses */
    uint16_t status_word;         /* CiA402 0x6041 */
    uint16_t error_code;          /* CiA402 0x603F */
    uint16_t control_word;        /* CiA402 0x6040 */
    int8_t mode_display;          /* CiA402 0x6061 */
    uint8_t responding_slaves;
    uint16_t jitter_us;           /* EtherCAT cycle jitter, max abs error */
    uint16_t ethercat_frequency_hz;
    uint16_t ethercat_period_min_us;
    uint16_t ethercat_period_max_us;
    uint16_t wakeup_latency_max_us;
    uint16_t cycle_exec_max_us;
    uint32_t timing_samples;
    uint32_t deadline_misses;
    uint8_t al_state;              /* EtherCAT AL state from slave config */
    uint8_t slave_online;
    uint8_t slave_operational;
    uint8_t domain_wc_state;
    uint32_t domain_working_counter;
};

#endif
