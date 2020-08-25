#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>

#include "resource_table.h"
#include "rpmsg.h"

extern struct my_resource_table resourceTable;

 // TODO: removed volatile register R30/31, a copied section of gpio.h, why is it needed here?


/* The PRU-ICSS system events used for RPMsg are defined in the Linux device tree
 * PRU0 uses system event 16 (To ARM) and 17 (From ARM)
 * PRU1 uses system event 18 (To ARM) and 19 (From ARM)
 */
#if defined(PRU0)
	#define TO_ARM_HOST 16
	#define FROM_ARM_HOST 17

	#define CHAN_DESC			"Channel 0"
	#define CHAN_PORT			0

#elif defined(PRU1)
	#define TO_ARM_HOST 18
	#define FROM_ARM_HOST 19

	#define CHAN_DESC			"Channel 1"
	#define CHAN_PORT			1

#else
	#error
#endif

/* Apparently this is the RPMSG address of the ARM core */
#define RPMSG_DST 0x0400
#define RPMSG_SRC CHAN_PORT

/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4
struct pru_rpmsg_transport transport;

static uint8_t print_buffer[RPMSG_BUF_SIZE];

void rpmsg_putraw(void * data, unsigned int len)
{
	pru_rpmsg_send(&transport, RPMSG_SRC, RPMSG_DST, data, len);
}

void rpmsg_printf(uint8_t *fmt, ...)
{
	va_list va;
	va_start(va,fmt);
    uint8_t * s = print_buffer;
	tfp_format(&s, putcp, fmt, va);
	putcp(&s, 0);
	rpmsg_putraw(print_buffer, strlen((char*)print_buffer) + 1);
	va_end(va);
}

void rpmsg_init(uint8_t * chan_name)
{
	volatile uint8_t *status;
	/* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
	CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

	/* Make sure the Linux drivers are ready for RPMsg communication */
	status = &resourceTable.rpmsg_vdev.status;
	while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

	/* Initialize the RPMsg transport structure */
	pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0, &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

	/* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
	while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, chan_name, CHAN_DESC, CHAN_PORT) != PRU_RPMSG_SUCCESS);
}

void rpmsg_flush()
{
	uint16_t src, dst, len;
	while(pru_rpmsg_receive(&transport, &src, &dst, print_buffer, &len) == PRU_RPMSG_SUCCESS){};
	CT_INTC.SECR0 |= (1 << FROM_ARM_HOST);
}

int rpmsg_get(uint8_t * s)
{
	uint16_t src, dst, len;
	int ret = pru_rpmsg_receive(&transport, &src, &dst, s, &len);
	if(ret != PRU_RPMSG_SUCCESS)
	{
		return ret;
	}
	return (int) len;
}
