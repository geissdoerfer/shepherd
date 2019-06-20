PRU_NUM = 1
PROJ_NAME = $(CURRENT_DIR)
FW_NAME = shepherd

INCLUDE = --include_path=./include --include_path=../include

SOURCES = $(notdir $(wildcard ./*.c))
SOURCES += printf.c
SOURCES += rpmsg.c
SOURCES += iep.c
SOURCES += simple_lock.c

BASE_PATH ?= ./..

include $(BASE_PATH)/lib/Makefile
