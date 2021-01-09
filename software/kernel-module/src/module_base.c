#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/remoteproc.h>
#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "rpmsg_pru.h"
#include "sync_ctrl.h"
#include "pru_comm.h"
#include "sysfs_interface.h"

#define MODULE_NAME "shepherd"
MODULE_SOFTDEP("pre: pruss");
MODULE_SOFTDEP("pre: remoteproc");
MODULE_SOFTDEP("pre: rpmsg_pru");

static const struct of_device_id shepherd_dt_ids[] = {
	{
		.compatible = "tud,shepherd",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, shepherd_dt_ids);

struct shepherd_platform_data {
	struct rproc *rproc_prus[2];
};

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
		printk(KERN_INFO "shprd: PRU-RPMSG = %s\n", msg);
        reset_prev_timestamp(); /* not correct place, but there is no sync-restart-detection yet */
	}
	return 0;
}


/*
 * get the two prus from the pruss-device-tree-node and save the pointers for common use.
 * the pruss-device-tree-node must have a shepherd entry with a pointer to the prusses. 
 */

static struct shepherd_platform_data *
get_shepherd_platform_data(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node, *pruss_dn = NULL;
	struct device_node *child;
	struct rproc *tmp_rproc;
	struct shepherd_platform_data *pdata;

	/*allocate mem for platform data*/
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return NULL;
	}

	if (!of_match_device(shepherd_dt_ids, &pdev->dev)) {
		pr_err("of_match_device failed\n");
		devm_kfree(&pdev->dev, pdata);
		return NULL;
	}

	pruss_dn = of_parse_phandle(np, "prusses", 0);
	if (!pruss_dn) {
		dev_err(&pdev->dev, "Unable to parse device node: prusses\n");
		devm_kfree(&pdev->dev, pdata);
		return NULL;
	}

	for_each_child_of_node (pruss_dn, child) {
		if (strncmp(child->name, "pru", 3) == 0) {
			tmp_rproc =
				rproc_get_by_phandle((phandle)child->phandle);

			if (tmp_rproc == NULL) {
				of_node_put(pruss_dn);
				dev_err(&pdev->dev,
					"Unable to parse device node: %s \n",
					child->name);
				devm_kfree(&pdev->dev, pdata);
				return NULL;
			}

			if (strncmp(tmp_rproc->name, "4a334000.pru", 12) == 0) {
				printk(KERN_INFO
				       "shprd: Found PRU0 at phandle 0x%02X",
				       child->phandle);

				pdata->rproc_prus[0] = tmp_rproc;
			}

			else if (strncmp(tmp_rproc->name, "4a338000.pru", 12) ==
				 0) {
				printk(KERN_INFO
				       "shprd: Found PRU1 at phandle 0x%02X",
				       child->phandle);

				pdata->rproc_prus[1] = tmp_rproc;
			}
		}
	}

	of_node_put(pruss_dn);
	return pdata;
}

static int shepherd_drv_probe(struct platform_device *pdev)
{
	struct shepherd_platform_data *pdata;
	int i;
	int ret = 0;

	printk(KERN_INFO "shprd: found shepherd device!!!\n");

	pdata = get_shepherd_platform_data(pdev);

	if (pdata == NULL) {
		/*pru device are not ready yet so kernel should retry the probe function later again*/
		return -EPROBE_DEFER;
	}

	/* Boot the two PRU cores with the corresponding the shepherd firmware */
	for (i = 0; i < 2; i++) {
		if (pdata->rproc_prus[i]->state == RPROC_RUNNING)
			rproc_shutdown(pdata->rproc_prus[i]);

		sprintf(pdata->rproc_prus[i]->firmware,
			"am335x-pru%u-shepherd-fw", i);

		if ((ret = rproc_boot(pdata->rproc_prus[i]))) {
			printk(KERN_ERR "shprd: Couldn't boot PRU%d", i);
			return ret;
		}
	}
	printk(KERN_INFO "shprd: PRUs started!");

	/* Allow some time for the PRUs to initialize. This is critical! */
	msleep(500);
	/* Initialize shared memory and PRU interrupt controller */
	pru_comm_init();

	/* Initialize RPMSG and register the 'received' callback function */
	if ((ret = rpmsg_pru_init(NULL, NULL, pru_recvd))) {
		return ret;
	} /* TODO: remove parts of subsystem if new system is in place */


	/* Initialize synchronization mechanism between PRU1 and our clock */
	sync_init(pru_comm_get_buffer_period_ns());

	/* Setup the sysfs interface for access from userspace */
	sysfs_interface_init();

	return 0;
}

static int shepherd_drv_remove(struct platform_device *pdev)
{
	struct shepherd_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	sysfs_interface_exit();
	pru_comm_exit();
	sync_exit();
	rpmsg_pru_exit();

	if (pdata != NULL) {
		rproc_shutdown(pdata->rproc_prus[0]);
		rproc_put(pdata->rproc_prus[0]);
		rproc_shutdown(pdata->rproc_prus[1]);
		rproc_put(pdata->rproc_prus[1]);
		devm_kfree(&pdev->dev, pdev->dev.platform_data);
		pdev->dev.platform_data = NULL;
	}

	platform_set_drvdata(pdev, NULL);
	printk(KERN_INFO "shprd: module exited from kernel!!!\n");
	return 0;
}

static struct platform_driver shepherd_driver = {
	.probe = shepherd_drv_probe,
	.remove = shepherd_drv_remove,
	.driver =
		{
			.name = MODULE_NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(shepherd_dt_ids),
		},
};
/**************/

module_platform_driver(shepherd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kai Geissdoerfer");
MODULE_DESCRIPTION("Shepherd time synchronization kernel module");
MODULE_VERSION("0.2.6");
MODULE_ALIAS("rpmsg:rpmsg-shprd");
