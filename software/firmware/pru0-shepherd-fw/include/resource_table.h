#ifndef _RESOURCE_TABLE_H_
#define _RESOURCE_TABLE_H_

#include <rsc_types.h>

struct my_resource_table {
	struct resource_table base;

	uint32_t offset[3]; /* Should match 'num' in actual definition */

	/* rpmsg vdev entry */
	struct fw_rsc_vdev rpmsg_vdev;
	struct fw_rsc_vdev_vring rpmsg_vring0;
	struct fw_rsc_vdev_vring rpmsg_vring1;

	/* intc definition */
	struct fw_rsc_custom pru_ints;
	struct fw_rsc_carveout shared_mem;
};

#endif /* _RESOURCE_TABLE_H_ */
