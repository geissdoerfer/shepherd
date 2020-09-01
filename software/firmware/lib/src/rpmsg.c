#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>

#include "resource_table.h"
#include "rpmsg.h"

/*
 TODO: virtqueue is an example of unoptimized code:
 - lots of struct* in struct* in propably far/slow registers
 	- important far-reads get repeated in 3 lower fn
 - instead of moves, the data (8 bytes) gets copied twice
 -> results in 2000 us sends() and 550 - 4000 us receive() (depends on msg)

 pru0/send_message() -> rpmsg_putraw() -> pru_rpmsg_send() 		|
 pru0/handle_rpmsg() -> rpmsg_get() -> pru_rpmsg_receive() 		| both fn are similar
 	- pru_vq_get_avail_buf()
 	- memcpy()
 	- pru_vq_add_used_buf()
 	- pru_vq_kick()
 todo: optimize for our 2x uint32 transfer, on receive fail early if nothing is there
 */


extern struct my_resource_table resourceTable;

#ifdef __GNUC__
#include <pru/io.h>
#else
volatile register uint32_t __R31;
volatile register uint32_t __R30;
#endif


/* The PRU-ICSS system events used for RPMsg are defined in the Linux device tree
 * PRU0 uses system event 16 (To ARM) and 17 (From ARM)
 * PRU1 uses system event 18 (To ARM) and 19 (From ARM)
 */
#if defined(PRU0)
	#define TO_ARM_HOST     16U
	#define FROM_ARM_HOST   17U

	#define CHAN_DESC		(uint8_t*)"Channel 0"
	#define CHAN_PORT		0U

#elif defined(PRU1)
	#define TO_ARM_HOST     18U
	#define FROM_ARM_HOST   19U

	#define CHAN_DESC		(uint8_t*)"Channel 1"
	#define CHAN_PORT		1u
#else
	#error
#endif

/* Apparently this is the RPMSG address of the ARM core */
#define RPMSG_DST   0x0400u
#define RPMSG_SRC   CHAN_PORT

/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4U
static struct pru_rpmsg_transport transport;

static uint8_t print_buffer[RPMSG_BUF_SIZE];

void rpmsg_putraw(const void *const data, const uint32_t len)
{
	pru_rpmsg_send(&transport, RPMSG_SRC, RPMSG_DST, data, len);
}

void rpmsg_printf(const uint8_t * fmt, ...)  // TODO: fmt should be const char *const, but underlying fn don't allow atm
{
	va_list va;
	va_start(va,fmt);
    uint8_t * dst_ptr = print_buffer;
    tfp_format(&dst_ptr, put_copy, fmt, va);
    put_copy(&dst_ptr, 0U);
	rpmsg_putraw(print_buffer, strlen((char*)print_buffer) + 1U);
	va_end(va);
}

void rpmsg_init(const uint8_t *const chan_name)
{
	/* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
	CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

	/* Make sure the Linux drivers are ready for RPMsg communication */
    volatile uint8_t *const status = &resourceTable.rpmsg_vdev.status;
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
	CT_INTC.SECR0 |= (1U << FROM_ARM_HOST);
}

uint32_t rpmsg_get(uint8_t *const buffer)
{
	uint16_t src, dst, len;
	const int32_t ret = pru_rpmsg_receive(&transport, &src, &dst, buffer, &len);
	if(ret != PRU_RPMSG_SUCCESS)
	{
		return 0U; // NOTE: this was returning (int ret), but its expensive and not useful
	}
	return len;
}
