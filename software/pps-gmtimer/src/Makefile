TARGET = pps-gmtimer
KVERSION := $(shell uname -r)
KDIR ?= /lib/modules/$(KVERSION)/build

obj-m += $(TARGET).o

all: build

build:
	make -C $(KDIR) M=$(CURDIR) modules

clean:
	make -C $(KDIR) M=$(CURDIR) clean

install: all
	@echo 'Installing kernel module'
	@sudo cp $(TARGET).ko /lib/modules/$(KVERSION)/extra/
