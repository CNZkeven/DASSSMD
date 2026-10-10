cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy CACHE FILEPATH "objcopy tool")
set(CMAKE_SIZE         arm-none-eabi-size    CACHE FILEPATH "size tool")
set(CMAKE_C_COMPILER_WORKS   TRUE)
set(CMAKE_ASM_COMPILER_WORKS TRUE)

project(zhsx C ASM)

set(CMAKE_C_FLAGS   "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Wall -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -x assembler-with-cpp")

set(CMAKE_C_FLAGS_DEBUG   "-Og -g -gdwarf-2")
set(CMAKE_C_FLAGS_RELEASE "-Os")

set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407ZGTx_FLASH.ld")
set(CMAKE_EXE_LINKER_FLAGS
    "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs"
)

add_compile_definitions(
    STM32F40_41xxx
    USE_STDPERIPH_DRIVER
    HSE_VALUE=25000000
)

set(CMSIS_DIR          "${CMAKE_SOURCE_DIR}/Libraries/CMSIS")
set(CMSIS_DEVICE_DIR   "${CMSIS_DIR}/Device/ST/STM32F4xx")
set(PERIPH_DIR         "${CMAKE_SOURCE_DIR}/Libraries/STM32F4xx_StdPeriph_Driver")
set(UCOS_DIR           "${CMAKE_SOURCE_DIR}/uCOSIII")

include_directories(
    ${CMAKE_SOURCE_DIR}/User
    ${CMAKE_SOURCE_DIR}/User/usart
    ${CMAKE_SOURCE_DIR}/User/lcd
    ${CMAKE_SOURCE_DIR}/User/touch
    ${CMAKE_SOURCE_DIR}/User/font
    ${CMAKE_SOURCE_DIR}/User/ui
    ${CMAKE_SOURCE_DIR}/User/task
    ${CMAKE_SOURCE_DIR}/User/motor
    ${CMAKE_SOURCE_DIR}/User/key
    ${CMAKE_SOURCE_DIR}/User/plotter
    ${CMSIS_DIR}/Include
    ${CMSIS_DEVICE_DIR}/Include
    ${PERIPH_DIR}/inc
    ${UCOS_DIR}/uCOS-III/Source
    ${UCOS_DIR}/uCOS-III/Ports/ARM-Cortex-M4/Generic/GCC
    ${UCOS_DIR}/uCOS-III/Ports/ARM-Cortex-M4/Generic/RealView
    ${UCOS_DIR}/uC-CPU
    ${UCOS_DIR}/uC-CPU/ARM-Cortex-M4/RealView
    ${UCOS_DIR}/uC-LIB
    ${UCOS_DIR}/os_cfg
)

set(USER_SOURCES
    ${CMAKE_SOURCE_DIR}/User/main.c
    ${CMAKE_SOURCE_DIR}/User/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/User/usart/bsp_usart.c
    ${CMAKE_SOURCE_DIR}/User/lcd/bsp_nt35510_lcd.c
    ${CMAKE_SOURCE_DIR}/User/touch/bsp_i2c_touch.c
    ${CMAKE_SOURCE_DIR}/User/touch/gt9xx.c
    ${CMAKE_SOURCE_DIR}/User/touch/palette.c
    ${CMAKE_SOURCE_DIR}/User/font/fonts.c
    # fonts.c 是野火的中文字库文件，GBKCODE_FLASH=1 时会 include 这个驱动。
    # 虽然电机界面只用英文字库、从来不会真的去调它，但少了它 fonts.c 编不过。
    ${CMAKE_SOURCE_DIR}/User/flash/bsp_spi_flash.c
    ${CMAKE_SOURCE_DIR}/User/ui/ui_task.c
    ${CMAKE_SOURCE_DIR}/User/ui/ui_draw.c
    ${CMAKE_SOURCE_DIR}/User/task/motor_task.c
    ${CMAKE_SOURCE_DIR}/User/motor/bsp_motor.c
    ${CMAKE_SOURCE_DIR}/User/key/bsp_key.c
    ${CMAKE_SOURCE_DIR}/User/plotter/plotter_path.c
    ${CMAKE_SOURCE_DIR}/User/plotter/plotter_demo.c
)

set(CMSIS_SOURCES
    ${CMSIS_DEVICE_DIR}/Source/Templates/system_stm32f4xx.c
    ${CMSIS_DEVICE_DIR}/Source/Templates/gcc/startup_stm32f40xx.s
)

set(PERIPH_SOURCES
    ${PERIPH_DIR}/src/misc.c
    ${PERIPH_DIR}/src/stm32f4xx_gpio.c
    ${PERIPH_DIR}/src/stm32f4xx_rcc.c
    ${PERIPH_DIR}/src/stm32f4xx_syscfg.c
    ${PERIPH_DIR}/src/stm32f4xx_exti.c
    ${PERIPH_DIR}/src/stm32f4xx_usart.c
    ${PERIPH_DIR}/src/stm32f4xx_tim.c
    ${PERIPH_DIR}/src/stm32f4xx_pwr.c
    ${PERIPH_DIR}/src/stm32f4xx_flash.c
    ${PERIPH_DIR}/src/stm32f4xx_fsmc.c
    ${PERIPH_DIR}/src/stm32f4xx_i2c.c
    ${PERIPH_DIR}/src/stm32f4xx_spi.c
    ${PERIPH_DIR}/src/stm32f4xx_dma.c
    ${PERIPH_DIR}/src/stm32f4xx_adc.c
)

set(UCOS_SOURCES
    ${UCOS_DIR}/uCOS-III/Source/os_core.c
    ${UCOS_DIR}/uCOS-III/Source/os_flag.c
    ${UCOS_DIR}/uCOS-III/Source/os_mem.c
    ${UCOS_DIR}/uCOS-III/Source/os_msg.c
    ${UCOS_DIR}/uCOS-III/Source/os_mutex.c
    ${UCOS_DIR}/uCOS-III/Source/os_prio.c
    ${UCOS_DIR}/uCOS-III/Source/os_q.c
    ${UCOS_DIR}/uCOS-III/Source/os_sem.c
    ${UCOS_DIR}/uCOS-III/Source/os_stat.c
    ${UCOS_DIR}/uCOS-III/Source/os_task.c
    ${UCOS_DIR}/uCOS-III/Source/os_tick.c
    ${UCOS_DIR}/uCOS-III/Source/os_time.c
    ${UCOS_DIR}/uCOS-III/Source/os_tmr.c
    ${UCOS_DIR}/uCOS-III/Source/os_var.c
    ${UCOS_DIR}/uCOS-III/Source/os_cfg_app.c
    ${UCOS_DIR}/uCOS-III/Ports/ARM-Cortex-M4/Generic/RealView/os_cpu_c.c
    ${UCOS_DIR}/uCOS-III/Ports/ARM-Cortex-M4/Generic/GCC/os_cpu_a.s
    ${UCOS_DIR}/uC-CPU/cpu_core.c
    ${UCOS_DIR}/uC-CPU/ARM-Cortex-M4/GCC/cpu_a.s
    ${UCOS_DIR}/uC-LIB/lib_mem.c
    ${UCOS_DIR}/uC-LIB/lib_str.c
    ${UCOS_DIR}/uC-LIB/lib_math.c
    ${UCOS_DIR}/uC-LIB/lib_ascii.c
    ${UCOS_DIR}/uC-LIB/Ports/ARM-Cortex-M4/GCC/lib_mem_a.s
)

add_executable(${PROJECT_NAME}.elf
    ${USER_SOURCES}
    ${CMSIS_SOURCES}
    ${PERIPH_SOURCES}
    ${UCOS_SOURCES}
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)
