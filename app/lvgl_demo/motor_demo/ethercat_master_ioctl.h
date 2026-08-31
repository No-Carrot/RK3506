#ifndef MOTOR_ETHERCAT_MASTER_IOCTL_H
#define MOTOR_ETHERCAT_MASTER_IOCTL_H

#include <stdint.h>
#include <sys/ioctl.h>

#define MOTOR_EC_IOCTL_TYPE 0xa4
#define MOTOR_EC_RATE_COUNT 3
#define MOTOR_EC_MAX_NUM_DEVICES 1

struct motor_ec_ioctl_device {
    uint8_t address[6];
    uint8_t attached;
    uint8_t link_state;
    uint64_t tx_count;
    uint64_t rx_count;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    int32_t tx_frame_rates[MOTOR_EC_RATE_COUNT];
    int32_t rx_frame_rates[MOTOR_EC_RATE_COUNT];
    int32_t tx_byte_rates[MOTOR_EC_RATE_COUNT];
    int32_t rx_byte_rates[MOTOR_EC_RATE_COUNT];
};

typedef struct {
    uint32_t slave_count;
    uint32_t config_count;
    uint32_t domain_count;
    uint32_t eoe_handler_count;
    uint8_t phase;
    uint8_t active;
    uint8_t scan_busy;
    struct motor_ec_ioctl_device devices[MOTOR_EC_MAX_NUM_DEVICES];
    uint32_t num_devices;
    uint64_t tx_count;
    uint64_t rx_count;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    int32_t tx_frame_rates[MOTOR_EC_RATE_COUNT];
    int32_t rx_frame_rates[MOTOR_EC_RATE_COUNT];
    int32_t tx_byte_rates[MOTOR_EC_RATE_COUNT];
    int32_t rx_byte_rates[MOTOR_EC_RATE_COUNT];
    int32_t loss_rates[MOTOR_EC_RATE_COUNT];
    uint64_t app_time;
    uint64_t dc_ref_time;
    uint16_t ref_clock;
} ec_ioctl_master_t;

#define EC_IOCTL_MASTER _IOR(MOTOR_EC_IOCTL_TYPE, 0x01, ec_ioctl_master_t)

#endif
