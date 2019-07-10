#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/remoteproc.h>
#include <asm/io.h>

#include "rpmsg_pru.h"
#include "sync_ctrl.h"
#include "pru_comm.h"
#include "sysfs_interface.h"

static struct rproc *rproc_prus[2];
static phandle pru_phandles[] = { 0xF6, 0xF7 };

/*
 * Handler for incoming RPMSG messages from PRU1. We only expect one type of
 * message, which is the control request, i.e. PRU1 asking us to run the PI
 * controller and send back the resulting clock correction factor. On reception
 * of any other message, we print it to kernel console. This way, PRU1 can
 * send arbitrary messages to user space
 */
int pru_recvd(void *data, unsigned int len)
{
	char *msg;
	struct CtrlRepMsg ctrl_rep;
	msg = (char *)(data);

	switch (msg[0]) {
	case MSG_SYNC_CTRL_REQ:
		/* Run the clock synchronization control loop */
		sync_loop(&ctrl_rep, (struct CtrlReqMsg *)data);
		/* Send the result back as RPMSG */
		rpmsg_pru_send(&ctrl_rep, sizeof(struct CtrlRepMsg));
		break;
	default:
		printk(KERN_INFO "shprd: RPMSG: %s\n", msg);
	}
	return 0;
}

static int __init mod_init(void)
{
	int i;
	int counter;
	int ret = 0;
	printk(KERN_INFO "shprd: module inserted into kernel!!!\n");

	/* Boot the two PRU cores with the corresponding the shepherd firmware */
	for (i = 0; i < 2; i++) {
		/* We'll wait a bit in case remoteproc is not yet up and running */
		counter = 0;
		while ((rproc_prus[i] = rproc_get_by_phandle(
				pru_phandles[i])) == NULL) {
			if (counter++ == 100) {
				printk(KERN_ERR
				       "shprd: Could not get PRU%d by phandle 0x%02X",
				       i, pru_phandles[i]);
				return -ENXIO;
			}
			msleep(100);
		}

		if (rproc_prus[i]->state == RPROC_RUNNING)
			rproc_shutdown(rproc_prus[i]);

		sprintf(rproc_prus[i]->firmware, "am335x-pru%u-shepherd-fw", i);

		if ((ret = rproc_boot(rproc_prus[i]))) {
			printk(KERN_ERR "shprd: Couldn't boot PRU%d", i);
			return ret;
		}
	}
	printk(KERN_INFO "shprd: PRUs started!");

	/* Initialize shared memory and PRU interrupt controller */
	pru_comm_init();

	/* Initialize RPMSG and register the 'received' callback function */
	if ((ret = rpmsg_pru_init(NULL, NULL, pru_recvd)))
		return ret;

	/* Initialize synchronization mechanism between PRU1 and our clock */
	sync_init(pru_comm_get_buffer_period_ns());

	/* Setup the sysfs interface for access from userspace */
	sysfs_interface_init();

	return 0;
}

static void __exit mod_exit(void)
{
	sysfs_interface_exit();
	pru_comm_exit();
	sync_exit();
	rpmsg_pru_exit();

	rproc_shutdown(rproc_prus[0]);
	rproc_put(rproc_prus[0]);
	rproc_shutdown(rproc_prus[1]);
	rproc_put(rproc_prus[1]);

	printk(KERN_INFO "shprd: module exited from kernel!!!\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kai Geissdoerfer");
MODULE_DESCRIPTION("Shepherd time synchronization kernel module");
MODULE_VERSION("0.0.3");
MODULE_ALIAS("rpmsg:rpmsg-shprd");