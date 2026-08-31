#ifndef ROCKCHIP_MADHT1505BA1_H
#define ROCKCHIP_MADHT1505BA1_H

#include <stdbool.h>
#include <stdint.h>

#include "ecrt.h"
#include "servo_protocol.h"
/*****************************************************************************/
 
/* Master 0, Slave 0, "DS2-EC"
 * Vendor ID:       0x00000a79
 * Product code:    0x00005000
 * Revision number: 0x00010000
 */

// process data

extern const int MADHT1505BA1_vendor;
extern const int MADHT1505BA1_product_code;

// offsets for PDO entries

typedef struct  {
	unsigned int control_word;
	unsigned int modes_of_operation;
	unsigned int target_position;
	unsigned int touch_probe_function;
	unsigned int error_code;
	unsigned int status_word;
	unsigned int modes_of_operation_display;
	unsigned int position_actual_value;
	unsigned int touch_probe_status;
	unsigned int touch_probe_pos1_pos_value;
	unsigned int following_error_actual_value;
	unsigned int digital_inputs;
	unsigned int current_velocity;
	unsigned int target_velocity;

	unsigned int profile_velocity;
	unsigned int end_velocity;
	unsigned int profile_acceleration;
	unsigned int end_deceleration;

	unsigned int alias;
	unsigned int position;

	ec_slave_config_t *sc;
	ec_slave_config_state_t sc_state;
	ec_domain_t *domain;
	ec_domain_state_t domain_state;
	uint8_t *domain_pd;
	ec_pdo_entry_reg_t domain_regs[19];

    uint16_t    status;
    int8_t      opmode;
    int32_t     cur_velocity;
    int      	curpos;
    int         user_set_pos;
    int         user_target_pos;
    int         max_step_per_cycle;
    unsigned int step_numerator;
    unsigned int step_denominator;
    unsigned int step_remainder;

	int  		user_velocity;
	bool 		change_pos;
    bool        target_pos_valid;
    bool        drive_ready;
    bool        takeover_required;
    unsigned int takeover_reset_cycles;
    unsigned int ready_stable_cycles;

    /*
     * RT ownership boundary:
     * - non-RT code only publishes target_request_* and reads snapshot.
     * - the servo_rt thread is the only writer of PDO/domain fields.
     */
    int         target_request_pos;
    uint32_t    target_request_seq;
    uint32_t    target_request_consumed_seq;
    struct servo_state snapshot;
    uint32_t    snapshot_seq;

}MADHT1505BA1_object;

extern ec_pdo_info_t slave_0_pdos[];
extern ec_sync_info_t slave_0_syncs[];

/*****************************************************************************/

int MADHT1505BA1_master_init(int bind_core);
uint32_t MADHT1505BA1_cycle_hz(void);
int MADHT1505BA1_slaves_init(MADHT1505BA1_object *object);
int MADHT1505BA1_master_activate(void);
void MADHT1505BA1_request_op_retry(const char *reason);
int MADHT1505BA1_slaves_activate(MADHT1505BA1_object *object);
int MADHT1505BA1_master_deinit(void);
int MADHT1505BA1_slave_start(int cnt, ...);
int MADHT1505BA1_check_motor(MADHT1505BA1_object *object); // 1 is true  -1 is false
// int MADHT1505BA1_motor_start(MADHT1505BA1_object *object, int velocity);
// int MADHT1505BA1_motor_stop(MADHT1505BA1_object *object);
uint32_t MADHT1505BA1_time_statistics_latency_min_ns(void);
uint32_t MADHT1505BA1_time_statistics_latency_max_ns(void);
uint32_t MADHT1505BA1_time_statistics_period_min_ns(void);
uint32_t MADHT1505BA1_time_statistics_period_max_ns(void);
int MADHT1505BA1_get_state_snapshot(MADHT1505BA1_object *object,
                                    struct servo_state *state);
int MADHT1505BA1_run_position_acquisition(MADHT1505BA1_object *object);
void MADHT1505BA1_motor_set_position_run(int user_position, MADHT1505BA1_object *object);
void MADHT1505BA1_position_reset(MADHT1505BA1_object *object);

#endif
