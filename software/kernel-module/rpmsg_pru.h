#ifndef __RPMSG_PRU_
#define __RPMSG_PRU_

/**
 * Initialize the rpmsg communication with PRU0
 * 
 * We use RPMSG to exchange the control data between this Linux kernel module
 * and the PRU firmware running on PRU0. This function initializes the Linux
 * driver and registers the callbacks that we use to process events.
 * 
 * @param rpmsg_probe_cb called when the RPMSG channel is established
 * @param rpmsg_removed_cb called when RPMSG channel is closed
 * @param prmsg_recvd_cb called whenever an RPMSG is received
 */
int rpmsg_pru_init(
    int (*rpmsg_probe_cb)(void),
    int (*rpmsg_removed_cb)(void),
	int (*rpmsg_recvd_cb)(void *, unsigned int)
);

/**
 * Uninitializes rpmsg driver
 * 
 * @see rpmsg_pru_init()
 */
void rpmsg_pru_exit(void);

/**
 * Sends an RPMSG to PRU1
 * 
 * @param data pointer to the data that should be sent
 * @param len lenght of data to send in bytes
 */
int rpmsg_pru_send(void * data, unsigned int len);

#endif /* __RPMSG_PRU_ */
