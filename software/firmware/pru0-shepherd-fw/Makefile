PRU_NUM = 0
PROJ_NAME = $(CURRENT_DIR)
FW_NAME = shepherd

INCLUDE = --include_path=./include --include_path=../include

SOURCES = $(notdir $(wildcard ./*.c))
SOURCES += printf.c
SOURCES += rpmsg.c
SOURCES += simple_lock.c
SOURCES += virtcap.c

ASM_SOURCES = spi_transfer.asm

BASE_PATH ?= ./..

include $(BASE_PATH)/lib/Makefile
