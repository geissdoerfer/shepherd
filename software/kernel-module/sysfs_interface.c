#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <asm/io.h>

#include "sysfs_interface.h"
#include "pru_comm.h"

int schedule_start(unsigned int start_time_second);

struct kobject *kobj_ref;
struct kobject *kobj_mem_ref;

static ssize_t sysfs_SharedMemhow(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t sysfs_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t sysfs_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t sysfs_mode_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t sysfs_mode_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t sysfs_harvesting_voltage_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

struct kobj_attr_SharedMem {
	struct kobj_attribute attr;
	unsigned int val_offset;
};

struct kobj_attribute attr_state = __ATTR(state, 0660, sysfs_state_show, sysfs_state_store);

struct kobj_attr_SharedMem attr_mem_base_addr = {
	.attr= __ATTR(address, 0660, sysfs_SharedMemhow, NULL),
	.val_offset=offsetof(struct SharedMem, mem_base_addr)
};
struct kobj_attr_SharedMem attr_mem_size = {
	.attr= __ATTR(size, 0660, sysfs_SharedMemhow, NULL),
	.val_offset=offsetof(struct SharedMem, mem_size)
};

struct kobj_attr_SharedMem attr_n_buffers = {
	.attr= __ATTR(n_buffers, 0660, sysfs_SharedMemhow, NULL),
	.val_offset=offsetof(struct SharedMem, n_buffers)
};
struct kobj_attr_SharedMem attr_samples_per_buffer = {
	.attr= __ATTR(samples_per_buffer, 0660, sysfs_SharedMemhow, NULL),
	.val_offset=offsetof(struct SharedMem, samples_per_buffer)
};
struct kobj_attr_SharedMem attr_buffer_period_ns = {
	.attr= __ATTR(buffer_period_ns, 0660, sysfs_SharedMemhow, NULL),
	.val_offset=offsetof(struct SharedMem, buffer_period_ns)
};
struct kobj_attr_SharedMem attr_mode = {
	.attr= __ATTR(mode, 0660, sysfs_mode_show, sysfs_mode_store),
	.val_offset=offsetof(struct SharedMem, shepherd_mode)
};
struct kobj_attr_SharedMem attr_harvesting_voltage = {
	.attr= __ATTR(
		harvesting_voltage, 0660, sysfs_SharedMemhow,
		sysfs_harvesting_voltage_store),
	.val_offset=offsetof(struct SharedMem, harvesting_voltage)
};
static struct attribute *pru_attrs[] = {
	&attr_n_buffers.attr.attr,
	&attr_samples_per_buffer.attr.attr,
	&attr_buffer_period_ns.attr.attr,
	&attr_mode.attr.attr,
	&attr_harvesting_voltage.attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = pru_attrs,
};

static struct attribute *pru_mem_attrs[] = {
	&attr_mem_base_addr.attr.attr,
	&attr_mem_size.attr.attr,
	NULL,
};

static struct attribute_group attr_mem_group = {
	.attrs = pru_mem_attrs,
};

static ssize_t sysfs_SharedMemhow(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kobj_attr_SharedMem * kobj_attr_wrapped;

	kobj_attr_wrapped = container_of(
		attr, struct kobj_attr_SharedMem, attr);
	return sprintf(buf, "%u", readl(
		pru_shared_mem_io + kobj_attr_wrapped->val_offset));
}

static ssize_t sysfs_state_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	switch (pru_comm_get_state()) {
		case STATE_IDLE:
			return sprintf(buf, "idle");
		case STATE_ARMED:
			return sprintf(buf, "armed");
		case STATE_RUNNING:
			return sprintf(buf, "running");
		case STATE_FAULT:
			return sprintf(buf, "fault");
		default:
			return sprintf(buf, "unknown");
	}
}

static ssize_t sysfs_state_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count
)
{
	struct timespec ts_now;
	int tmp;


	if(strncmp(buf, "start", 5) == 0) {
		if((count < 5) || (count > 6))
			return -EINVAL;

		if(pru_comm_get_state() != STATE_IDLE)
			return -EBUSY;

		pru_comm_trigger(HOST_PRU_EVT_START);
		return count;
	}

	else if(strncmp(buf, "stop", 4) == 0) {
		if((count < 4) || (count > 5))
			return -EINVAL;

		pru_comm_cancel_delayed_start();
		pru_comm_trigger(HOST_PRU_EVT_RESET);
		return count;
	}

	else if(sscanf(buf, "%d", &tmp) == 1) {
		/* Timestamp system clock */

		if(pru_comm_get_state() != STATE_IDLE)
			return -EBUSY;

		getnstimeofday (&ts_now);
		if(tmp < ts_now.tv_sec + 1)
			return -EINVAL;
		printk(KERN_INFO "shprd: Setting ts_start to %d", tmp);
		pru_comm_set_state(STATE_ARMED);
		pru_comm_schedule_delayed_start(tmp);
		return count;
	}
	else
		return -EINVAL;

}

static ssize_t sysfs_mode_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kobj_attr_SharedMem * kobj_attr_wrapped;
	unsigned int mode;


	kobj_attr_wrapped = container_of(
		attr, struct kobj_attr_SharedMem, attr);

	mode = readl(
		pru_shared_mem_io + kobj_attr_wrapped->val_offset);

	switch (mode) {
		case MODE_HARVESTING:
			return sprintf(buf, "harvesting");
		case MODE_LOAD:
			return sprintf(buf, "load");
		case MODE_EMULATION:
			return sprintf(buf, "emulation");
		case MODE_DEBUG:
			return sprintf(buf, "debug");
		default:
			return -EINVAL;
	}

}

static ssize_t sysfs_mode_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count
)
{
	struct kobj_attr_SharedMem * kobj_attr_wrapped;
	unsigned int mode;

	if (pru_comm_get_state() != STATE_IDLE)
		return -EBUSY;


	kobj_attr_wrapped = container_of(
		attr, struct kobj_attr_SharedMem, attr);

	if(pru_comm_get_state() != STATE_IDLE)
		return -EBUSY;

	if(strncmp(buf, "harvesting", 10) == 0) {
		if((count < 10) || (count > 11))
			return -EINVAL;

		mode = MODE_HARVESTING;
	}
	else if(strncmp(buf, "load", 4) == 0) {
		if((count < 4) || (count > 5))
		return -EINVAL;

		mode = MODE_LOAD;
	}
	else if(strncmp(buf, "emulation", 9) == 0) {
		if((count < 9) || (count > 10))
			return -EINVAL;

		mode = MODE_EMULATION;
	}
	else if(strncmp(buf, "debug", 5) == 0) {
		if((count < 5) || (count > 6))
		return -EINVAL;

		mode = MODE_DEBUG;
	}
	else
		return -EINVAL;

	writel(
		mode,
		pru_shared_mem_io + kobj_attr_wrapped->val_offset);
	printk(KERN_INFO "shprd: new mode: %d", mode);
	pru_comm_trigger(HOST_PRU_EVT_RESET);
	return count;
}

static ssize_t sysfs_harvesting_voltage_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count
)
{
	unsigned int tmp;
	struct kobj_attr_SharedMem * kobj_attr_wrapped;

	if(pru_comm_get_state() != STATE_IDLE)
		return -EBUSY;

	kobj_attr_wrapped = container_of(
		attr, struct kobj_attr_SharedMem, attr);

	if(sscanf(buf, "%u", &tmp) == 1) {
		printk(KERN_INFO "shprd: Setting harvesting voltage to %u", tmp);
		writel(
			tmp,
			pru_shared_mem_io + kobj_attr_wrapped->val_offset);

		pru_comm_trigger(HOST_PRU_EVT_RESET);
		return count;
	}

	return -EINVAL;

}

int sysfs_interface_init(void)
{
	int retval = 0;

	kobj_ref = kobject_create_and_add("shepherd", NULL);


	if((retval = sysfs_create_file(kobj_ref, &attr_state.attr))){
		printk(KERN_ERR "Cannot create sysfs state attrib\n");
		goto r_sysfs;
	}

	if((retval = sysfs_create_group(kobj_ref, &attr_group))){
		printk(KERN_ERR "shprd: cannot create sysfs attrib group\n");
		goto r_state;
	};

	kobj_mem_ref = kobject_create_and_add("memory", kobj_ref);

	if((retval = sysfs_create_group(kobj_mem_ref, &attr_mem_group))){
		printk(KERN_ERR "shprd: cannot create sysfs memory attrib group\n");
		goto r_group;
	};
	return 0;

r_group:
	sysfs_remove_group(kobj_ref, &attr_group);
r_state:
	sysfs_remove_file(kobj_ref, &attr_state.attr);
r_sysfs:
	kobject_put(kobj_ref);

	return retval;
}


void sysfs_interface_exit(void)
{
	sysfs_remove_group(kobj_ref, &attr_group);
	sysfs_remove_file(kobj_ref, &attr_state.attr);
	kobject_put(kobj_mem_ref);
	kobject_put(kobj_ref);
}
