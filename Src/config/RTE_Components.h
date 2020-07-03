
/*
 * Auto generated Run-Time-Environment Configuration File
 *      *** Do not modify ! ***
 *
 * Project: 'TBook_Rev2b' 
 * Target:  'TBv2b' 
 */

#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H


/*
 * Define the Device Header File: 
 */
#define CMSIS_device_header "stm32f4xx.h"

/*  ARM::CMSIS:RTOS2:Keil RTX5:Source:5.5.1 */
#define RTE_CMSIS_RTOS2                 /* CMSIS-RTOS2 */
        #define RTE_CMSIS_RTOS2_RTX5            /* CMSIS-RTOS2 Keil RTX5 */
        #define RTE_CMSIS_RTOS2_RTX5_SOURCE     /* CMSIS-RTOS2 Keil RTX5 Source */

/*  Keil.ARM Compiler::Compiler:Event Recorder:DAP:1.4.0 */
#define RTE_Compiler_EventRecorder
#define RTE_Compiler_EventRecorder_DAP

/*  Keil.ARM Compiler::Compiler:I/O:File:File System:1.2.0 */
#define RTE_Compiler_IO_File            /* Compiler I/O: File */
#define RTE_Compiler_IO_File_FS         /* Compiler I/O: File (File System) */

/*  Keil.ARM Compiler::Compiler:I/O:STDOUT:User:1.2.0 */
#define RTE_Compiler_IO_STDOUT          /* Compiler I/O: STDOUT */
#define RTE_Compiler_IO_STDOUT_ITM      /* Compiler I/O: STDOUT ITM */
#define RTE_Compiler_IO_STDERR_ITM      /* Compiler I/O: STDERR ITM */
//          #define RTE_Compiler_IO_STDOUT_User     /* Compiler I/O: STDOUT User */

/*  Keil.MDK-Plus::File System:CORE:LFN Debug:6.13.0 */
#define RTE_FileSystem_Core             /* File System Core */
#define RTE_FileSystem_LFN              /* File System with Long Filename support */
#define RTE_FileSystem_Debug            /* File System Debug Version */

/*  Keil.MDK-Plus::File System:Drive:Memory Card:6.13.0 */
#define RTE_FileSystem_Drive_MC_0       /* File System Memory Card Drive 0 */

/*  Keil.MDK-Plus::USB:CORE:Debug:6.13.7 */
#define RTE_USB_Core                    /* USB Core */
#define RTE_USB_Core_Debug              /* USB Core Debug Version */

/*  Keil.MDK-Plus::USB:Device:6.13.7 */
#define RTE_USB_Device_0                /* USB Device 0 */

/*  Keil.MDK-Plus::USB:Device:MSC:6.13.7 */
#define RTE_USB_Device_MSC_0            /* USB Device MSC instance 0 */

/*  Keil::CMSIS Driver:Flash:AT45DB642D:1.3.0 */
#define RTE_Drivers_Flash_AT45DB642D    /* Driver Flash AT45DB642D */

/*  Keil::CMSIS Driver:I2C:2.8 */
#define RTE_Drivers_I2C1                /* Driver I2C1 */
//        #define RTE_Drivers_I2C2                /* Driver I2C2 */
//        #define RTE_Drivers_I2C3                /* Driver I2C3 */

/*  Keil::CMSIS Driver:MCI:2.8 */
#define RTE_Drivers_MCI0                /* Driver MCI0 */

/*  Keil::CMSIS Driver:SPI:2.11 */
#define RTE_Drivers_SPI1                /* Driver SPI1 */

#define RTE_Drivers_SPI2                /* Driver SPI2 */
#define RTE_Drivers_SPI3                /* Driver SPI3 */
//        #define RTE_Drivers_SPI4                /* Driver SPI4 */
//        #define RTE_Drivers_SPI5                /* Driver SPI5 */
//        #define RTE_Drivers_SPI6                /* Driver SPI6 */

/*  Keil::CMSIS Driver:USB Device:Full-speed:2.19 */
#define RTE_Drivers_USBD0               /* Driver USBD0 */

/*  Keil::Device:STM32Cube Framework:Classic:1.7.4 */
#define RTE_DEVICE_FRAMEWORK_CLASSIC

/*  Keil::Device:STM32Cube HAL:Common:1.7.4 */
#define RTE_DEVICE_HAL_COMMON

/*  Keil::Device:STM32Cube HAL:Cortex:1.7.4 */
#define RTE_DEVICE_HAL_CORTEX

/*  Keil::Device:STM32Cube HAL:DMA:1.7.4 */
#define RTE_DEVICE_HAL_DMA

/*  Keil::Device:STM32Cube HAL:GPIO:1.7.4 */
#define RTE_DEVICE_HAL_GPIO

/*  Keil::Device:STM32Cube HAL:I2C:1.7.4 */
#define RTE_DEVICE_HAL_I2C

/*  Keil::Device:STM32Cube HAL:PWR:1.7.4 */
#define RTE_DEVICE_HAL_PWR

/*  Keil::Device:STM32Cube HAL:QSPI:1.7.4 */
#define RTE_DEVICE_HAL_QSPI

/*  Keil::Device:STM32Cube HAL:RCC:1.7.4 */
#define RTE_DEVICE_HAL_RCC

/*  Keil::Device:STM32Cube HAL:SPI:1.7.4 */
#define RTE_DEVICE_HAL_SPI

/*  Keil::Device:Startup:2.6.2 */
#define RTE_DEVICE_STARTUP_STM32F4XX    /* Device Startup for STM32F4 */


#endif /* RTE_COMPONENTS_H */
