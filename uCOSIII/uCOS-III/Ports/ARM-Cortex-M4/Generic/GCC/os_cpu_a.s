@********************************************************************************************************
@                                                uC/OS-III
@                                          The Real-Time Kernel
@
@                         (c) Copyright 2009-2013; Micrium, Inc.; Weston, FL
@                    All rights reserved.  Protected by international copyright laws.
@
@                                           ARM Cortex-M4 Port
@
@ File      : os_cpu_a.s
@ Version   : V3.04.04
@ For       : ARMv7 Cortex-M4
@ Mode      : Thumb-2 ISA
@ Toolchain : GCC
@********************************************************************************************************

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

@********************************************************************************************************
@                                          EXTERNAL REFERENCES
@********************************************************************************************************

    .extern OSRunning
    .extern OSPrioCur
    .extern OSPrioHighRdy
    .extern OSTCBCurPtr
    .extern OSTCBHighRdyPtr
    .extern OSIntExit
    .extern OSTaskSwHook
    .extern OS_CPU_ExceptStkBase

@********************************************************************************************************
@                                          PUBLIC FUNCTIONS
@********************************************************************************************************

    .global OSStartHighRdy
    .global OSCtxSw
    .global OSIntCtxSw
    .global OS_CPU_PendSVHandler
    .global OS_CPU_SysTickHandler
    .global OS_CPU_FP_Reg_Push
    .global OS_CPU_FP_Reg_Pop

@********************************************************************************************************
@                                               EQUATES
@********************************************************************************************************

    .equ NVIC_INT_CTRL,   0xE000ED04      @ Interrupt control state register.
    .equ NVIC_SYSPRI14,   0xE000ED22      @ System priority register (priority 14).
    .equ NVIC_PENDSV_PRI, 0xFF            @ PendSV priority value (lowest).
    .equ NVIC_PENDSVSET,  0x10000000      @ Value to trigger PendSV exception.

@********************************************************************************************************
@                                   FLOATING POINT REGISTERS PUSH
@                             void  OS_CPU_FP_Reg_Push (CPU_STK  *stkPtr)
@********************************************************************************************************

    .section .text.OS_CPU_FP_Reg_Push, "ax", %progbits
    .type OS_CPU_FP_Reg_Push, %function
    .align 2
OS_CPU_FP_Reg_Push:
    MRS     R1, PSP                         @ PSP is process stack pointer
    CBZ     R1, OS_CPU_FP_nosave            @ Skip FP register save the first time

    VMRS    R1, FPSCR
    STR     R1, [R0, #-4]!
    VSTMDB  R0!, {S0-S31}
    LDR     R1, =OSTCBCurPtr
    LDR     R2, [R1]
    STR     R0, [R2]
OS_CPU_FP_nosave:
    BX      LR
    .size OS_CPU_FP_Reg_Push, .-OS_CPU_FP_Reg_Push

@********************************************************************************************************
@                                   FLOATING POINT REGISTERS POP
@                             void  OS_CPU_FP_Reg_Pop (CPU_STK  *stkPtr)
@********************************************************************************************************

    .section .text.OS_CPU_FP_Reg_Pop, "ax", %progbits
    .type OS_CPU_FP_Reg_Pop, %function
    .align 2
OS_CPU_FP_Reg_Pop:
    VLDMIA  R0!, {S0-S31}
    LDMIA   R0!, {R1}
    VMSR    FPSCR, R1
    LDR     R1, =OSTCBHighRdyPtr
    LDR     R2, [R1]
    STR     R0, [R2]
    BX      LR
    .size OS_CPU_FP_Reg_Pop, .-OS_CPU_FP_Reg_Pop

@********************************************************************************************************
@                                         START MULTITASKING
@                                      void OSStartHighRdy(void)
@********************************************************************************************************

    .section .text.OSStartHighRdy, "ax", %progbits
    .type OSStartHighRdy, %function
    .align 2
OSStartHighRdy:
    CPSID   I                               @ Prevent interruption during context switch
    LDR     R0, =NVIC_SYSPRI14              @ Set the PendSV exception priority
    LDR     R1, =NVIC_PENDSV_PRI
    STRB    R1, [R0]

    LDR     R0, =OS_CPU_ExceptStkBase       @ Initialize the MSP to the OS_CPU_ExceptStkBase
    LDR     R1, [R0]
    MSR     MSP, R1

    LDR     R0, =OSPrioCur                  @ OSPrioCur   = OSPrioHighRdy;
    LDR     R1, =OSPrioHighRdy
    LDRB    R2, [R1]
    STRB    R2, [R0]

    LDR     R5, =OSTCBCurPtr
    LDR     R1, =OSTCBHighRdyPtr            @ OSTCBCurPtr = OSTCBHighRdyPtr;
    LDR     R2, [R1]
    STR     R2, [R5]

    LDR     R0, [R2]                        @ R0 is new process SP; SP = OSTCBHighRdyPtr->StkPtr;
    MSR     PSP, R0                         @ Load PSP with new process SP

    MRS     R0, CONTROL
    ORR     R0, R0, #2
    MSR     CONTROL, R0
    ISB                                     @ Sync instruction stream

    LDMIA   SP!, {R4-R11}                   @ Restore r4-11 from new process stack
    LDMIA   SP!, {R0-R3}                    @ Restore r0, r3
    LDMIA   SP!, {R12, LR}                  @ Load R12 and LR
    LDMIA   SP!, {R1, R2}                   @ Load PC and discard xPSR
    CPSIE   I
    BX      R1
    .size OSStartHighRdy, .-OSStartHighRdy

@********************************************************************************************************
@                       PERFORM A CONTEXT SWITCH (From task level) - OSCtxSw()
@********************************************************************************************************

    .section .text.OSCtxSw, "ax", %progbits
    .type OSCtxSw, %function
    .align 2
OSCtxSw:
    LDR     R0, =NVIC_INT_CTRL              @ Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR
    .size OSCtxSw, .-OSCtxSw

@********************************************************************************************************
@                   PERFORM A CONTEXT SWITCH (From interrupt level) - OSIntCtxSw()
@********************************************************************************************************

    .section .text.OSIntCtxSw, "ax", %progbits
    .type OSIntCtxSw, %function
    .align 2
OSIntCtxSw:
    LDR     R0, =NVIC_INT_CTRL              @ Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR
    .size OSIntCtxSw, .-OSIntCtxSw

@********************************************************************************************************
@                                       HANDLE PendSV EXCEPTION
@                                   void OS_CPU_PendSVHandler(void)
@********************************************************************************************************

    .section .text.OS_CPU_PendSVHandler, "ax", %progbits
    .type OS_CPU_PendSVHandler, %function
    .align 2
OS_CPU_PendSVHandler:
    CPSID   I                               @ Prevent interruption during context switch
    MRS     R0, PSP                         @ PSP is process stack pointer
    STMDB   R0!, {R4-R11}                   @ Save remaining regs r4-11 on process stack

    LDR     R5, =OSTCBCurPtr                @ OSTCBCurPtr->OSTCBStkPtr = SP;
    LDR     R6, [R5]
    STR     R0, [R6]                        @ R0 is SP of process being switched out

                                            @ At this point, entire context of process has been saved
    MOV     R4, LR                          @ Save LR exc_return value
    BL      OSTaskSwHook                    @ OSTaskSwHook();

    LDR     R0, =OSPrioCur                  @ OSPrioCur   = OSPrioHighRdy;
    LDR     R1, =OSPrioHighRdy
    LDRB    R2, [R1]
    STRB    R2, [R0]

    LDR     R1, =OSTCBHighRdyPtr            @ OSTCBCurPtr = OSTCBHighRdyPtr;
    LDR     R2, [R1]
    STR     R2, [R5]

    ORR     LR, R4, #0x04                   @ Ensure exception return uses process stack
    LDR     R0, [R2]                        @ R0 is new process SP; SP = OSTCBHighRdyPtr->StkPtr;
    LDMIA   R0!, {R4-R11}                   @ Restore r4-11 from new process stack
    MSR     PSP, R0                         @ Load PSP with new process SP
    CPSIE   I
    BX      LR                              @ Exception return will restore remaining context
    .size OS_CPU_PendSVHandler, .-OS_CPU_PendSVHandler

@********************************************************************************************************
@                                       HANDLE SysTick EXCEPTION
@                                   void OS_CPU_SysTickHandler(void)
@ NOTE: This function is implemented in os_cpu_c.c, so we don't define it here
@********************************************************************************************************

    .end
