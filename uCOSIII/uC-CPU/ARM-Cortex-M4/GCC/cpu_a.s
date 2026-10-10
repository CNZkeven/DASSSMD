@********************************************************************************************************
@                                                uC/CPU
@                                    CPU CONFIGURATION & PORT LAYER
@
@                          (c) Copyright 2004-2013; Micrium, Inc.; Weston, FL
@
@               All rights reserved.  Protected by international copyright laws.
@
@               uC/CPU is provided in source form to registered licensees ONLY.  It is
@               illegal to distribute this source code to any third party unless you receive
@               written permission by an authorized Micrium representative.  Knowledge of
@               the source code may NOT be used to develop a similar product.
@
@               Please help us continue to provide the Embedded community with the finest
@               software available.  Your honesty is greatly appreciated.
@
@               You can find our product's user manual, API reference, release notes and
@               more information at https://doc.micrium.com.
@               You can contact us at www.micrium.com
@********************************************************************************************************

@********************************************************************************************************
@                                            CPU PORT FILE
@
@                                            ARM-Cortex-M4
@                                            GCC Toolchain
@
@ Filename      : cpu_a.s
@ Version       : V1.30.01.00
@********************************************************************************************************

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

@********************************************************************************************************
@                                           PUBLIC FUNCTIONS
@********************************************************************************************************

    .global CPU_IntDis
    .global CPU_IntEn

    .global CPU_SR_Save
    .global CPU_SR_Restore

    .global CPU_WaitForInt
    .global CPU_WaitForExcept

    .global CPU_CntLeadZeros
    .global CPU_CntTrailZeros
    .global CPU_RevBits

@********************************************************************************************************
@                                    DISABLE and ENABLE INTERRUPTS
@********************************************************************************************************

    .section .text.CPU_IntDis, "ax", %progbits
    .type CPU_IntDis, %function
    .align 2
CPU_IntDis:
    CPSID   I
    BX      LR
    .size CPU_IntDis, .-CPU_IntDis

    .section .text.CPU_IntEn, "ax", %progbits
    .type CPU_IntEn, %function
    .align 2
CPU_IntEn:
    CPSIE   I
    BX      LR
    .size CPU_IntEn, .-CPU_IntEn

@********************************************************************************************************
@                                      CRITICAL SECTION FUNCTIONS
@********************************************************************************************************

    .section .text.CPU_SR_Save, "ax", %progbits
    .type CPU_SR_Save, %function
    .align 2
CPU_SR_Save:
    MRS     R0, PRIMASK                     @ Set prio int mask to mask all (except faults)
    CPSID   I
    BX      LR
    .size CPU_SR_Save, .-CPU_SR_Save

    .section .text.CPU_SR_Restore, "ax", %progbits
    .type CPU_SR_Restore, %function
    .align 2
CPU_SR_Restore:
    MSR     PRIMASK, R0
    BX      LR
    .size CPU_SR_Restore, .-CPU_SR_Restore

@********************************************************************************************************
@                                         WAIT FOR INTERRUPT
@********************************************************************************************************

    .section .text.CPU_WaitForInt, "ax", %progbits
    .type CPU_WaitForInt, %function
    .align 2
CPU_WaitForInt:
    WFI                                     @ Wait for interrupt
    BX      LR
    .size CPU_WaitForInt, .-CPU_WaitForInt

@********************************************************************************************************
@                                         WAIT FOR EXCEPTION
@********************************************************************************************************

    .section .text.CPU_WaitForExcept, "ax", %progbits
    .type CPU_WaitForExcept, %function
    .align 2
CPU_WaitForExcept:
    WFE                                     @ Wait for exception
    BX      LR
    .size CPU_WaitForExcept, .-CPU_WaitForExcept

@********************************************************************************************************
@                                         CPU_CntLeadZeros()
@********************************************************************************************************

    .section .text.CPU_CntLeadZeros, "ax", %progbits
    .type CPU_CntLeadZeros, %function
    .align 2
CPU_CntLeadZeros:
    CLZ     R0, R0                          @ Count leading zeros
    BX      LR
    .size CPU_CntLeadZeros, .-CPU_CntLeadZeros

@********************************************************************************************************
@                                         CPU_CntTrailZeros()
@********************************************************************************************************

    .section .text.CPU_CntTrailZeros, "ax", %progbits
    .type CPU_CntTrailZeros, %function
    .align 2
CPU_CntTrailZeros:
    RBIT    R0, R0                          @ Reverse bits
    CLZ     R0, R0                          @ Count trailing zeros
    BX      LR
    .size CPU_CntTrailZeros, .-CPU_CntTrailZeros

@********************************************************************************************************
@                                            CPU_RevBits()
@********************************************************************************************************

    .section .text.CPU_RevBits, "ax", %progbits
    .type CPU_RevBits, %function
    .align 2
CPU_RevBits:
    RBIT    R0, R0                          @ Reverse bits
    BX      LR
    .size CPU_RevBits, .-CPU_RevBits

    .end
