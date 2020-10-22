PRU_NUM = 0
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))
folder_path := $(dir $(mkfile_path))
PROJ_NAME = $(current_dir)
FW_NAME = shepherd

INCLUDE = --include_path=./include --include_path=../include

SOURCES = $(notdir $(wildcard ./*.c))
# also add external files (./lib/src/)
SOURCES += printf.c
SOURCES += rpmsg.c
SOURCES += simple_lock.c
# TODO: test
SOURCES += iep.c

ASM_SOURCES = spi_transfer_pru.asm

BASE_PATH ?= ./..

include $(BASE_PATH)/lib/Makefile
