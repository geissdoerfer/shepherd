#ifndef __KERNEL_PRU_COMM_H_
#define __KERNEL_PRU_COMM_H_

#include <stdint.h>

#define MSG_ID_CTRL_REQ 0xF0
#define MSG_ID_CTRL_REP 0xF1

struct msg_ctrl_req_s {
    char identifier;
    uint32_t ticks_iep;
    uint32_t old_period;
} __attribute__((packed));

struct msg_ctrl_rep_s {
    char identifier;
    int32_t clock_corr;
    uint64_t next_timestamp_ns;
} __attribute__((packed));

#endif /* __KERNEL_PRU_COMM_H_ */
