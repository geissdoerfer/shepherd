#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>


static struct rpmsg_device * rpdev_pru = NULL;

static int (*rpmsg_recvd_callback)(void *, unsigned int) = NULL;
static int (*rpmsg_probe_callback)(void) = NULL;
static int (*rpmsg_removed_callback)(void) = NULL;

static int rpmsg_pru_cb(struct rpmsg_device *rpdev, void *data, int len,
	void *priv, u32 src)
{
	if(rpmsg_recvd_callback)
		return rpmsg_recvd_callback(data, (unsigned int) len);
	else
		return 0;
}

static int rpmsg_pru_probe(struct rpmsg_device *rpdev)
{

	dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
					rpdev->src, rpdev->dst);

	rpdev_pru = rpdev;

	if(rpmsg_probe_callback)
		return rpmsg_probe_callback();
	else
		return 0;

}

int rpmsg_pru_send(void * data, unsigned int len)
{
	int ret;
	if(!rpdev_pru)
		return -1;

	if ((ret = rpmsg_send(rpdev_pru->ept, data, len)))
		dev_err(&rpdev_pru->dev, "rpmsg_send failed: %d\n", ret);
	return ret;

}

static void rpmsg_pru_remove(struct rpmsg_device *rpdev)
{
	if(rpmsg_removed_callback)
		rpmsg_removed_callback();

	dev_info(&rpdev->dev, "rpmsg client driver is removed\n");
}


static struct rpmsg_device_id rpmsg_driver_sample_id_table[] = {
	{ .name	= "rpmsg-shprd" },
	{ },
};

static struct rpmsg_driver rpmsg_pru_client = {
	.drv.name	= KBUILD_MODNAME,
	.id_table	= rpmsg_driver_sample_id_table,
	.probe		= rpmsg_pru_probe,
	.callback	= rpmsg_pru_cb,
	.remove		= rpmsg_pru_remove,
};

int rpmsg_pru_init(
	int (*rpmsg_probe_cb)(void),
	int (*rpmsg_removed_cb)(void),
	int (*rpmsg_recvd_cb)(void *, unsigned int)
)
{
	int ret;

	rpmsg_recvd_callback = rpmsg_recvd_cb;
	rpmsg_probe_callback = rpmsg_probe_cb;
	rpmsg_removed_callback = rpmsg_removed_cb;


	if ((ret = register_rpmsg_driver(&rpmsg_pru_client))) {
		printk(KERN_ERR "shprd: Unable to register rpmsg driver");
		return ret;
	}

	return 0;
}

void rpmsg_pru_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_pru_client);
}

MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_sample_id_table);
