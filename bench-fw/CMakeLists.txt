cmake_minimum_required(VERSION 3.16)

set(TCH D:/tools/arm-gnu-toolchain/bin)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER ${TCH}/arm-none-eabi-gcc.exe)
set(CMAKE_ASM_COMPILER ${TCH}/arm-none-eabi-gcc.exe)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(bench LANGUAGES C ASM)

set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Os -ffunction-sections -fdata-sections -DSTM32F427xx -DUSE_HAL_DRIVER -Wall -Wno-unused-variable -std=gnu11")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")

include_directories(
  D:/ARBATOS/projects/INFANTRY-A/Core/Inc
  D:/ARBATOS/projects/INFANTRY-A/Drivers/STM32F4xx_HAL_Driver/Inc
  D:/ARBATOS/projects/INFANTRY-A/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
  D:/ARBATOS/projects/INFANTRY-A/Drivers/CMSIS/Device/ST/STM32F4xx/Include
  D:/ARBATOS/projects/INFANTRY-A/Drivers/CMSIS/Include
)

file(GLOB HAL_SRC D:/ARBATOS/projects/INFANTRY-A/Drivers/STM32F4xx_HAL_Driver/Src/*.c)

add_executable(bench.elf
  ${CMAKE_CURRENT_SOURCE_DIR}/bench_main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/dr16.c
  D:/ARBATOS/projects/INFANTRY-A/Core/Src/can.c
  D:/ARBATOS/projects/INFANTRY-A/Core/Src/system_stm32f4xx.c
  ${HAL_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}/startup_gcc.S
)

target_link_options(bench.elf PRIVATE
  -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  -Wl,-Map=${CMAKE_CURRENT_SOURCE_DIR}/build/bench.map
)

add_custom_command(TARGET bench.elf POST_BUILD
  COMMAND ${TCH}/arm-none-eabi-objcopy.exe -O ihex bench.elf bench.hex
  COMMAND ${TCH}/arm-none-eabi-objcopy.exe -O binary bench.elf bench.bin
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "generating bench.hex / bench.bin"
)

