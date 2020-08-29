# contains most "magic" for cgt-compiling the pru-code

.DEFAULT_GOAL := all

ifeq ($(MAKECMDGOALS),all)
ifndef PRU_CGT
define CGT_ERROR_BODY

*******************************************************************************
PRU_CGT environment variable is not set. Examples given:
(Desktop Linux) export PRU_CGT=/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
(Windows) set PRU_CGT=C:/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
(ARM Linux*) export PRU_CGT=/usr/share/ti/cgt-pru

*ARM Linux also needs to create a symbolic link to the /usr/bin/ directory in
order to use the same Makefile
(ARM Linux) ln -s /usr/bin/ /usr/share/ti/cgt-pru/bin
*******************************************************************************

endef
$(error $(CGT_ERROR_BODY))
endif
ifndef PRU_SUPPORT
define SUP_ERROR_BODY

*******************************************************************************
PRU_SUPPORT environment variable is not set. Examples given:
(Desktop Linux) export PRU_SUPPORT=/path/to/ti/pru-software-support-package-5.7.0
(Windows) set PRU_SUPPORT=C:/path/to/ti/pru-software-support-package-5.7.0
(ARM Linux) export PRU_SUPPORT=/usr/share/ti/pru-software-support-package-5.7.0
*******************************************************************************

endef
$(error $(SUP_ERROR_BODY))
endif
endif


PRU_NUM ?= 0
REMOTEPROC_NUM = $(shell expr $(PRU_NUM) + 1 )

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR := $(notdir $(patsubst %/,%,$(dir $(MKFILE_PATH))))

LINKER_COMMAND_FILE=$(BASE_PATH)/lib/AM335x_PRU.cmd
LIBS=--library=${PRU_SUPPORT}/lib/rpmsg_lib.lib

INCLUDE += --include_path=${PRU_SUPPORT}/include
INCLUDE += --include_path=${PRU_SUPPORT}/include/am335x
INCLUDE += --include_path=$(BASE_PATH)/lib/include

STACK_SIZE=0x200
HEAP_SIZE=0x100
GEN_DIR=gen

#Common compiler and linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
# PRU-Hardware
CFLAGS = -v3 --endian=little --hardware_mac=on
# Compile Options
CFLAGS += --c99 --float_operations_allowed=none --symdebug:none --plain_char=unsigned --fp_mode=relaxed
CFLAGS += --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) -ppd -ppa
# Compile Optimization
CFLAGS += -O4
# Compile Feedback
CFLAGS += --issue_remarks --display_error_number

# -O2 -> default for shepherd
# -O3 ->
# -O4 -> link time optimization, 30-40% smaller code

#Linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
LFLAGS=--reread_libs --warn_sections --stack_size=$(STACK_SIZE) --heap_size=$(HEAP_SIZE)

TARGET=$(GEN_DIR)/pru$(PRU_NUM)-$(FW_NAME).out
MAP=$(GEN_DIR)/pru$(PRU_NUM)-$(FW_NAME).map

TARGET_PATH := $(folder_path)$(TARGET)
TARGET_SIZE := $(shell stat -L -c %s $(TARGET_PATH))

VPATH = $(BASE_PATH)/lib/src

#Using .object instead of .obj in order to not conflict with the CCS build process
OBJECTS=$(patsubst %,$(GEN_DIR)/%,$(SOURCES:.c=.object))
OBJECTS+=$(patsubst %,$(GEN_DIR)/%,$(ASM_SOURCES:.asm=.object))

.PHONY: all
all: printStart $(TARGET) printEnd

.PHONY: printStart
printStart:
	@echo ''
	@echo '************************************************************'
	@echo 'Building project: $(PROJ_NAME)'

.PHONY: printEnd
printEnd:
	@echo ''
	@echo 'Output files can be found in the "$(GEN_DIR)" directory'
	@echo ''
	@echo 'Finished building project: $(PROJ_NAME), in $(TARGET_PATH) [$(TARGET_SIZE) bytes]'
	@echo '************************************************************'
	@echo ''

# Invokes the linker (-z flag) to make the .out file
$(TARGET): $(OBJECTS) $(LINKER_COMMAND_FILE)
	@echo ''
	@echo 'Building target: $@'
	@echo 'Invoking: PRU Linker'
	$(PRU_CGT)/bin/clpru $(CFLAGS) -z -i$(PRU_CGT)/lib -i$(PRU_CGT)/include $(LFLAGS) -o $(TARGET) $(OBJECTS) -m$(MAP) $(LINKER_COMMAND_FILE) --library=libc.a $(LIBS)
	@echo 'Finished building target: $@ [$$(shell stat -L -c %s $(TARGET_PATH)) bytes]' # TODO: not working yet

# Invokes the compiler on all c files in the directory to create the object files
$(GEN_DIR)/%.object: %.c
	@mkdir -p $(GEN_DIR)
	@echo ''
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	$(PRU_CGT)/bin/clpru --include_path=$(PRU_CGT)/include $(INCLUDE) $(CFLAGS) -D PRU$(PRU_NUM) -fe $@ $<

$(GEN_DIR)/%.object: %.asm
	@mkdir -p $(GEN_DIR)
	@echo ''
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	$(PRU_CGT)/bin/clpru --include_path=$(PRU_CGT)/include $(INCLUDE) $(CFLAGS) -D PRU$(PRU_NUM) -fe $@ $<


.PHONY: clean
clean:
	@echo ''
	@echo '************************************************************'
	@echo 'Cleaning project: $(PROJ_NAME)'
	@echo ''
	@echo 'Removing files in the "$(GEN_DIR)" directory'
	@rm -rf $(GEN_DIR)
	@echo ''
	@echo 'Finished cleaning project: $(PROJ_NAME)'
	@echo '************************************************************'
	@echo ''

.PHONY: install
install:
	@echo ''
	@echo '************************************************************'
	@echo 'Installing project: $(PROJ_NAME)'
	@echo ''
	@echo 'Installing firmware to /lib/firmware'
	install -D $(TARGET) $(DESTDIR)/lib/firmware/am335x-pru$(PRU_NUM)-$(FW_NAME)-fw
	@echo ''
	@echo '************************************************************'
	@echo ''

# Includes the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.object=%.pp)
