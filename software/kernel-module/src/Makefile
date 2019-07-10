TARGET = shepherd
KVERSION := $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build

obj-m += $(TARGET).o
$(TARGET)-objs += sync_ctrl.o
$(TARGET)-objs += pru_comm.o
$(TARGET)-objs += sysfs_interface.o
$(TARGET)-objs += rpmsg_pru.o
$(TARGET)-objs += module_base.o

ccflags-y=-I$(src)/../common

all: build

build:
	make -C $(KDIR) M=$(CURDIR) modules

clean:
	make -C $(KDIR) M=$(CURDIR) clean

install: all
	@echo 'Installing kernel module'
	@sudo cp $(TARGET).ko /lib/modules/$(KVERSION)/extra/
