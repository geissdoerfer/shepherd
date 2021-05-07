PRU_NUM = 1
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
SOURCES += iep.c
SOURCES += simple_lock.c

BASE_PATH ?= ./..

include $(BASE_PATH)/lib/Makefile
