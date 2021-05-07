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
LIBS= --library=${PRU_SUPPORT}/lib/rpmsg_lib.lib --library=libc.a
# NOTE: libc.a will be replaced during compile-time by fitting run-time-system (RTS) -> rtspruv3.lib

INCLUDE += --include_path=${PRU_SUPPORT}/include
INCLUDE += --include_path=${PRU_SUPPORT}/include/am335x
INCLUDE += --include_path=$(BASE_PATH)/lib/include

STACK_SIZE 	= 0x200 # 512 byte
HEAP_SIZE 	= 0x100 # 256 byte
GEN_DIR		= gen

### Common compiler and linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
## PRU-Hardware
CFLAGS = -v3 #                              silicon version
CFLAGS += --endian=little #                 for data and code, both (big, little) is possible
CFLAGS += --hardware_mac=on #               use Multiply and Accumulate Unit (no division though)

# Compile Options
CFLAGS += --c99 #                           C-Standard
CFLAGS += --float_operations_allowed=none # usage of FP results in compile-error
CFLAGS += --fp_mode=relaxed
CFLAGS += --symdebug:none #                 debug symbols -> remove
CFLAGS += --plain_char=unsigned    #        explicit define underlying datatype for char
CFLAGS += --obj_directory=$(GEN_DIR)
CFLAGS += --pp_directory=$(GEN_DIR)
CFLAGS += --asm_directory=$(GEN_DIR)
CFLAGS += -ppd #                            perform preprocessing only [but]
CFLAGS += -ppa #                            continue preprocessing with compilation
CFLAGS += --keep_asm #						does what it says - have a look

# float - NOTE: use fp_reassoc=on and fp_mode=relaxed, both reduce the precision a bit, but improve performance (even using int if safe)
# 				enable with --float_operations_allowed=32

# Compile Optimization
CFLAGS += --opt_level=4
# -O2 -> old default for shepherd
# -O3 -> lvl2 & file-optimization
# -O4 -> lvl3 & link time optimization, 30-40% smaller code
# TODO: with optimization <4 and no debug symbols the firmware does not work (60/54 kB, against 101/92 kB)?!?

# Speed Optimization (short -mf)
CFLAGS += --opt_for_speed=5
# 2, 1, 0 	gradually optimize for size, (lower number means higher risk of performance impact)
# 3, 4, 5 	gradually optimize for performance (higher num means high risk impacting size)
# default is 4 (without value), 1 (not specifying command)

# Compile Feedback
CFLAGS += --issue_remarks #                 non-serious warnings
CFLAGS += --display_error_number #          supplies diagnostic identifier
#CFLAGS += --check_misra=all #     	   		diagnostic for MISRA-C:2004 rules -> compiler goes berserk
#CFLAGS += --misra_advisory=warning #    	diagnostic for MISRA-C:2004 rules

# Linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
LFLAGS = --reread_libs #                    reread until references are resolved
LFLAGS += --warn_sections #                 warn when undefined output section is created
LFLAGS += --stack_size=$(STACK_SIZE) #      default is 256 byte
LFLAGS += --heap_size=$(HEAP_SIZE)

# Init-Model -> you MUST SPECIFY ONE one of both (otherwise program-entry-point missing -> undefined behaviour)
#     ram: var-init at load-time, enhanced performance, reduced boot time, save memory used by the initialization tables
#     rom: var-init at run-time, default method of auto-initialization
LFLAGS += --ram_model

TARGET=$(GEN_DIR)/pru$(PRU_NUM)-$(FW_NAME).out
MAP=$(GEN_DIR)/pru$(PRU_NUM)-$(FW_NAME).map

TARGET_PATH := $(folder_path)$(TARGET)

VPATH = $(BASE_PATH)/lib/src

# Using .object instead of .obj in order to not conflict with the CCS build process
OBJECTS=$(patsubst %,$(GEN_DIR)/%,$(SOURCES:.c=.object))
OBJECTS+=$(patsubst %,$(GEN_DIR)/%,$(ASM_SOURCES:.asm=.object))

.PHONY: all
all: printStart $(TARGET) printEnd

.PHONY: printStart
printStart:
	@echo ''
	@echo ''
	@echo '************************************************************'
	@echo 'Building project: $(PROJ_NAME)'

.PHONY: printEnd
printEnd:
	@echo ''
	@echo 'Output files can be found in the "$(GEN_DIR)" directory'
	@echo 'Resulting firmware is $(TARGET_PATH), size in bytes:'
	@stat -L -c %s $(TARGET_PATH)
	@echo ''
	@echo 'Finished building project: $(PROJ_NAME), '
	@echo '************************************************************'
	@echo ''

# Invokes the linker (-z flag) to make the .out file
$(TARGET): $(OBJECTS) $(LINKER_COMMAND_FILE)
	@echo ''
	@echo 'Building target: $@'
	@echo 'Invoking: PRU Linker'
	$(PRU_CGT)/bin/clpru $(CFLAGS) -z -i$(PRU_CGT)/lib -i$(PRU_CGT)/include $(LFLAGS) -o $(TARGET) $(OBJECTS) -m$(MAP) $(LINKER_COMMAND_FILE) $(LIBS)
	@echo 'Finished building target'

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
	@echo ''

.PHONY: install
install: all
	@echo ''
	@echo ''
	@echo '************************************************************'
	@echo 'Installing project: $(PROJ_NAME)'
	@echo ''
	@echo 'Installing firmware to /lib/firmware'
	install -D $(TARGET) $(DESTDIR)/lib/firmware/am335x-pru$(PRU_NUM)-$(FW_NAME)-fw
	@echo ''
	@echo '************************************************************'
	@echo ''
	@echo ''

# Includes the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.object=%.pp)
