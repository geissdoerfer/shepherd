# NOTE: for GCC-PRU, NOT TO BUILD TARGET (yet) -> currently it (just) shows cLion all the sources
# - needs modified support package (at fixed location): https://github.com/dinuxbg/pru-software-support-package
# TODO: several restructures, but linker-problems remain during compilation
cmake_minimum_required(VERSION 3.10)
message("-- CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    # provide custom toolchain
    message("   Detected Linux: provide custom toolchain")
    set(CROSS_COMPILE "pru-")
    set(TOOLCHAIN_ROOT "/usr/bin")
    set(OS_SUFFIX "")
    set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}gcc${OS_SUFFIX}")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}g++${OS_SUFFIX}")
    set(CMAKE_AR           "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}ar${OS_SUFFIX}")
    set(CMAKE_LINKER       "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}ld${OS_SUFFIX}")
    set(CMAKE_NM           "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}nm${OS_SUFFIX}")
    set(CMAKE_OBJCOPY      "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}objcopy${OS_SUFFIX}")
    set(CMAKE_OBJDUMP      "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}objdump${OS_SUFFIX}")
    set(CMAKE_STRIP        "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}strip${OS_SUFFIX}")
    set(CMAKE_RANLIB       "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}ranlib${OS_SUFFIX}")
    set(CMAKE_SIZE         "${TOOLCHAIN_ROOT}/${CROSS_COMPILE}size${OS_SUFFIX}")
else()
    message("   Detected NON-Linux: provide some mockup for editor")
    add_definitions(-D__PRU__)    #can be replaced by add_compile_definitions(var=value) when cmake gets updated ~3.17
    add_definitions(-D__TI_LLVM__)
    add_definitions(-D__delay_cycles=(uint32_t))  #some harmless definition
    include_directories(
            ${CMAKE_CURRENT_LIST_DIR}/_cgt233/include
            ${CMAKE_CURRENT_LIST_DIR}/_cgt233/lib
            ${CMAKE_CURRENT_LIST_DIR}/_cgt233/lib/src
    )
endif()

# TODO: add all includes, cflags and other config needed

# set the project name

project(PRU0 C)
project(PRU1 C)
#set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD 99)

# TODO: the following 40 lines are hideous, but allow fast testing
set(SRC_PRU0
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/ringbuffer.c
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/spi_transfer_pru.asm
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/sampling.c
        )
set(HDR_PRU0
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include/hw_config.h
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include/resource_table.h
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include/resource_table_def.h
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include/ringbuffer.h
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include/sampling.h
        )

set(HDR_SHARED
        ${CMAKE_CURRENT_LIST_DIR}/include/commons.h
        ${CMAKE_CURRENT_LIST_DIR}/include/shepherd_config.h
        )

set(SRC_LIB
        ${CMAKE_CURRENT_LIST_DIR}/lib/src/iep.c
        ${CMAKE_CURRENT_LIST_DIR}/lib/src/printf.c
        ${CMAKE_CURRENT_LIST_DIR}/lib/src/rpmsg.c
        ${CMAKE_CURRENT_LIST_DIR}/lib/src/simple_lock.c
        )
set(HDR_LIB
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/gpio.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/iep.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/stdint_fast.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/intc.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/printf.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/rpmsg.h
        ${CMAKE_CURRENT_LIST_DIR}/lib/include/simple_lock.h
        )

set(SRC_SUP
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/lib/src/rpmsg_lib/pru_rpmsg.c
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/lib/src/rpmsg_lib/pru_virtqueue.c
        )
set(HDR_SUP
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/pru_rpmsg.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/pru_types.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/pru_virtqueue.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_cfg.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_ctrl.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_ecap.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_iep.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_intc.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/pru_uart.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/sys_mailbox.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/sys_mcspi.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/sys_pwmss.h
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x/sys_tscAdcSs.h
        )

# add the executable
add_executable(PRU0
        ${CMAKE_CURRENT_SOURCE_DIR}/pru0-shepherd-fw/main.c
        lib/include/sys_gpio.h)

target_sources(PRU0
        PUBLIC
            ${HDR_SUP}
            ${HDR_LIB}
            ${HDR_SHARED}
            ${SRC_SUP}
            ${SRC_LIB}
        #PRIVATE
            ${HDR_PRU0}
            ${SRC_PRU0}
        )



target_include_directories(PRU0
        PUBLIC
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pru0-shepherd-fw>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pru0-shepherd-fw/include>
        #PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/pru0-shepherd-fw/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
            ${CMAKE_CURRENT_SOURCE_DIR}/_pssp57g/include
            ${CMAKE_CURRENT_SOURCE_DIR}/_pssp57g/include/am335x
        )

set(TEMPEPE
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/ringbuffer.c
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/spi_transfer_pru.asm
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/sampling.c

        ${SOURCE}
        ${HEADERS}
        )

add_executable(PRU1
        ${HEADERS}
        ${CMAKE_CURRENT_LIST_DIR}/pru1-shepherd-fw/main.c
        ${CMAKE_CURRENT_LIST_DIR}/pru1-shepherd-fw/include/resource_table.h
        ${SOURCE}
        ${HEADERS}
        lib/include/sys_gpio.h)

target_compile_definitions(PRU0 PRIVATE PRU0)
target_compile_definitions(PRU1 PRIVATE PRU1)

# Common flags
add_compile_options(-g -Os)
add_compile_options(-Wall -Wextra)
add_compile_options(-v)
# Add remoteproc headers
add_compile_options(-isystem include)
# Define this to squeeze code size by removing atexit, exit, constructors and destructors from CRT.
#add_compile_options(-minrt)

target_compile_options(PRU0 PUBLIC -v -g -Os -Wall -Wextra -isystem include -mmcu=am335x.pru0)
#target_compile_options(PRU1 PRIVATE -mmcu=am335x.pru1)
target_compile_options(PRU1 PUBLIC -v -g -Os -Wall -Wextra -isystem include -mmcu=am335x.pru1)

# shared includes
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/lib/src/rpmsg_lib
        ${CMAKE_CURRENT_LIST_DIR}/lib/src
        ${CMAKE_CURRENT_LIST_DIR}/lib/include
        ${CMAKE_CURRENT_LIST_DIR}/include
        )

target_include_directories(PRU0 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include)
target_include_directories(PRU1 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pru1-shepherd-fw/include)
