/**
  ******************************************************************************
  * @file    FMC_SDRAM/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    11-November-2013
  * @brief   Main Interrupt Service Routines.
  *         This file provides template for all exceptions handler and
  *         peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "os.h"
#include "./touch/bsp_i2c_touch.h"
#include "./usart/bsp_usart.h"
#include "./motor/bsp_motor.h"




/** @addtogroup STM32F429I_DISCOVERY_Examples
  * @{
  */

/** @addtogroup FMC_SDRAM
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/* 各种 fault 处理：以前是静默死循环，发生时看不见现象。
 * 这里直接拍 PF6 / PF7 / PF8（LED1/2/3）让现象可见：
 *   - LED1 (PF6) 亮 = HardFault
 *   - LED2 (PF7) 亮 = MemManage
 *   - LED3 (PF8) 亮 = BusFault
 *   - LED1+LED3 同时亮 = UsageFault
 * LED 是低电平点亮，所以这里 BSRRH（ResetBits）写 1 即点亮。
 * 直接操作寄存器，避免再依赖外设库 / GPIO 时钟是否就绪。
 */
static void Fault_Indicate(uint16_t pinmask)
{
    /* 进死循环之前必须先切断两轴的 STEP 脉冲。
       下面这个 while(1) 只是空转，但 TIM2 是硬件 PWM —— 不管 CPU 在干什么，
       只要 CCR 还非零，驱动器就会一直收到脉冲，机构会一直往一个方向走到底。
       这里只写 TIM2/GPIO 寄存器，不调用任何 uCOS 服务，在 fault 上下文里是安全的。
       Motor_Init() 还没跑过时 Motor_StopAll() 会直接返回。 */
    Motor_StopAll();

    /* 确保 GPIOF 时钟已开启（RCC_AHB1ENR bit 5）。即使之前没开，这里补一次。*/
    RCC->AHB1ENR |= (1U << 5);

    /* 把对应引脚配成推挽输出（每两位为一个 pin 的 MODER：01=输出） */
    if (pinmask & GPIO_Pin_6) { GPIOF->MODER &= ~(3U << (6*2)); GPIOF->MODER |= (1U << (6*2)); }
    if (pinmask & GPIO_Pin_7) { GPIOF->MODER &= ~(3U << (7*2)); GPIOF->MODER |= (1U << (7*2)); }
    if (pinmask & GPIO_Pin_8) { GPIOF->MODER &= ~(3U << (8*2)); GPIOF->MODER |= (1U << (8*2)); }

    /* 拉低 = 点亮（板子 LED 低有效）*/
    GPIOF->BSRRH = pinmask;

    while (1) { ; }
}

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  Fault_Indicate(GPIO_Pin_6);
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  Fault_Indicate(GPIO_Pin_7);
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  Fault_Indicate(GPIO_Pin_8);
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  Fault_Indicate(GPIO_Pin_6 | GPIO_Pin_8);
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{}

/* SVC_Handler, PendSV_Handler, SysTick_Handler 由 uCOS-III 提供。
 * 启动文件 startup_stm32f40xx.s 中使用 OS_CPU_PendSVHandler
 * 和 OS_CPU_SysTickHandler，不需要在这里重复定义。
 */

void DEBUG_USART_IRQHandler(void)
{
  /* 串口在本工程里只是调试输出/交互通道，不服务于任何业务逻辑。
   * 收到什么就回显什么，方便直接用串口终端敲字确认板子活着。
   *
   * 即便当前 NVIC 没开 USART1 IRQ，留这个壳子也得满足 uCOS 的契约：
   * 凡是用户级 ISR 必须配对 OSIntEnter/OSIntExit，否则将来谁打开了
   * 接收中断，就会重演 error.md Fix #1 的内核被打崩问题。
   */
  uint8_t ucTemp;

  OSIntEnter();

  if(USART_GetITStatus(DEBUG_USART,USART_IT_RXNE)!=RESET)
  {
      ucTemp = USART_ReceiveData( DEBUG_USART );
      USART_SendData(DEBUG_USART, ucTemp);
  }

  OSIntExit();
}

/* GTP 触摸中断标志。ISR 里只置位 + 清 EXTI，不做任何 I2C 操作。
 * 真正的 I2C 读取和坐标处理由 UI_Task 在任务上下文里完成。
 * 原来在 ISR 里调 GTP_TouchProcess()（含 bit-bang I2C + 5 次重试），
 * 一次中断就要花几毫秒，期间整个系统被阻塞——UI 画不了、SysTick 被延迟、
 * 如果 I2C 总线卡住就死在里面，表现就是白屏→黑屏，啥 LED 都不亮。
 */
volatile uint8_t g_gtp_irq_flag = 0;

void GTP_IRQHandler(void)
{
  OSIntEnter();

  if (EXTI_GetITStatus(GTP_INT_EXTI_LINE) != RESET) {
    g_gtp_irq_flag = 1;
    EXTI_ClearITPendingBit(GTP_INT_EXTI_LINE);
  }

  OSIntExit();
}

/* TIM2 更新中断：1kHz 公共时间基准。
 * STEP 波形由 TIM2 CH2/CH3 的硬件 PWM 输出；点动模式下本中断统计步数，
 * 作画模式下 Motor_UpdateISR 还会执行一个有界的 DDA 调度步并维护 CCR preload。
 * 因此播放依赖每个更新事件，不能把这个中断误当成“仅显示计数”。
 *
 * 按本工程约定由 TIM2_IRQHandler 用 OSIntEnter()/OSIntExit() 包裹。
 * ISR 调度保持有界，不调用阻塞 API、LCD、串口或 I2C。
 */
void TIM2_IRQHandler(void)
{
  OSIntEnter();

  Motor_UpdateISR();

  OSIntExit();
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f429_439xx.s).                         */
/******************************************************************************/

/**
  * @}
  */ 

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
