# NOTE: for GCC-PRU, NOT TO BUILD TARGET (yet) -> it just shows cLion all the sources
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
    message("   Detected NON-Linux: provide some kind of mockup")
    add_definitions(-D__PRU__)    #can be replaced by add_compile_definitions(var=value) when cmake gets updated ~3.17
    add_definitions(-D__TI_LLVM__)
    add_definitions(-D__delay_cycles=uint32_t)  #some harmless definition
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

# add the executable
add_executable(PRU0
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/main.c
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/ringbuffer.c
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/sampling.c
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/spi_transfer.asm
        ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/virtcap.c
        )

add_executable(PRU1
        ${CMAKE_CURRENT_LIST_DIR}/pru1-shepherd-fw/main.c
        )

target_compile_definitions(PRU0 PRIVATE PRU0)
target_compile_definitions(PRU1 PRIVATE PRU1)

# Common flags
add_compile_options(-g -Os)
add_compile_options(-Wall -Wextra)
# Add remoteproc headers
add_compile_options(-isystem include)
# Define this to squeeze code size by removing atexit, exit, constructors and destructors from CRT.
add_compile_options(-minrt)

target_compile_options(PRU0 PRIVATE -mmcu=am335x.pru0)
target_compile_options(PRU1 PRIVATE -mmcu=am335x.pru1)

# shared includes
include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/am335x
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/include/
        ${CMAKE_CURRENT_LIST_DIR}/_pssp57g/lib/src/rpmsg_lib
        ${CMAKE_CURRENT_LIST_DIR}/lib/include
        ${CMAKE_CURRENT_LIST_DIR}/lib/src
        ${CMAKE_CURRENT_LIST_DIR}/include
        )

target_include_directories(PRU0 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pru0-shepherd-fw/include)
target_include_directories(PRU1 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pru1-shepherd-fw/include)
