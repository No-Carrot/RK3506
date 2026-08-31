#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "servo_protocol.h"

static const char *yes_no(int value)
{
    return value ? "yes" : "no";
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

static const char *al_state_name(uint8_t al_state)
{
    switch (al_state) {
    case 0x01:
        return "INIT";
    case 0x02:
        return "PREOP";
    case 0x04:
        return "SAFEOP";
    case 0x08:
        return "OP";
    default:
        return "unknown";
    }
}

static const char *wc_state_name(uint8_t wc_state)
{
    switch (wc_state) {
    case 0:
        return "zero";
    case 1:
        return "incomplete";
    case 2:
        return "complete";
    default:
        return "unknown";
    }
}

static int set_socket_timeout(int fd, int timeout_ms)
{
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) < 0)
        return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                   sizeof(timeout)) < 0)
        return -1;
    return 0;
}

int main(void)
{
    struct sockaddr_un address;
    struct servo_request request = {
        .version = SERVO_PROTOCOL_VERSION,
        .type = SERVO_REQUEST_STATE,
        .command = SERVO_CMD_NONE,
        .argument = 0,
    };
    struct servo_state state;
    int fd;
    ssize_t count;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return 1;
    }

    if (set_socket_timeout(fd, 500) < 0) {
        fprintf(stderr, "setsockopt timeout: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SERVO_SOCKET_PATH, sizeof(address.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        fprintf(stderr, "connect %s: %s\n", SERVO_SOCKET_PATH,
                strerror(errno));
        close(fd);
        return 1;
    }

    count = send(fd, &request, sizeof(request), MSG_NOSIGNAL);
    if (count != (ssize_t)sizeof(request)) {
        fprintf(stderr, "send request: %s\n",
                count < 0 ? strerror(errno) : "short write");
        close(fd);
        return 1;
    }

    count = recv(fd, &state, sizeof(state), 0);
    close(fd);
    if (count != (ssize_t)sizeof(state)) {
        fprintf(stderr, "recv state: %s\n",
                count < 0 ? strerror(errno) : "short read");
        return 1;
    }

    if (state.version != SERVO_PROTOCOL_VERSION) {
        fprintf(stderr, "protocol mismatch: got %u expected %u\n",
                state.version, SERVO_PROTOCOL_VERSION);
        return 1;
    }

    printf("%-24s %u\n", "protocol", state.version);
    printf("%-24s 0x%08x\n", "flags", state.flags);
    printf("%-24s %s\n", "flag.link_up",
           yes_no(state.flags & SERVO_FLAG_LINK_UP));
    printf("%-24s %s\n", "flag.bus_op",
           yes_no(state.flags & SERVO_FLAG_BUS_OP));
    printf("%-24s %s\n", "flag.enabled",
           yes_no(state.flags & SERVO_FLAG_ENABLED));
    printf("%-24s %s\n", "flag.fault",
           yes_no(state.flags & SERVO_FLAG_FAULT));
    printf("%-24s %u\n", "responding_slaves", state.responding_slaves);
    printf("%-24s 0x%02x (%s)\n", "al_state", state.al_state,
           al_state_name(state.al_state));
    printf("%-24s %s\n", "slave_online", yes_no(state.slave_online));
    printf("%-24s %s\n", "slave_operational",
           yes_no(state.slave_operational));
    printf("%-24s %lu\n", "domain_wc",
           (unsigned long)state.domain_working_counter);
    printf("%-24s %u (%s)\n", "domain_wc_state", state.domain_wc_state,
           wc_state_name(state.domain_wc_state));
    printf("%-24s 0x%04x (%s)\n", "status_word", state.status_word,
           cia402_state_name(state.status_word));
    printf("%-24s 0x%04x\n", "control_word", state.control_word);
    printf("%-24s 0x%04x\n", "error_code", state.error_code);
    printf("%-24s %d\n", "mode_display", state.mode_display);
    printf("%-24s %ld\n", "actual_position",
           (long)state.actual_position);
    printf("%-24s %ld\n", "target_position",
           (long)state.target_position);
    printf("%-24s %ld\n", "following_error",
           (long)state.following_error);
    printf("%-24s %ld\n", "actual_velocity",
           (long)state.actual_velocity);
    printf("%-24s %ld\n", "target_velocity",
           (long)state.target_velocity);
    printf("%-24s %u\n", "jitter_us", state.jitter_us);
    printf("%-24s %u\n", "cycle_hz", state.ethercat_frequency_hz);
    printf("%-24s %u..%u\n", "period_us",
           state.ethercat_period_min_us, state.ethercat_period_max_us);
    printf("%-24s %u\n", "wakeup_latency_max_us",
           state.wakeup_latency_max_us);
    printf("%-24s %u\n", "cycle_exec_max_us", state.cycle_exec_max_us);
    printf("%-24s %lu\n", "deadline_misses",
           (unsigned long)state.deadline_misses);
    return 0;
}
