#include "./usart/bsp_usart.h"



void Debug_USART_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

	/* ��һ������ʼ��GPIO */
	 RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //�������
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  //����
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; //�ٶ�

  /* ����Tx����Ϊ���ù���  */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9  ;  //������������
  GPIO_Init(GPIOA, &GPIO_InitStructure);
	  /* ����Rx����Ϊ���ù���  */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = DEBUG_USART_RX_PIN;
  GPIO_Init(DEBUG_USART_RX_GPIO_PORT, &GPIO_InitStructure);




/* ���� PXx �� USARTx_Tx*/
  GPIO_PinAFConfig(DEBUG_USART_RX_GPIO_PORT,DEBUG_USART_RX_SOURCE,DEBUG_USART_RX_AF);
/*  ���� PXx �� USARTx__Rx*/
  GPIO_PinAFConfig(DEBUG_USART_TX_GPIO_PORT,DEBUG_USART_TX_SOURCE,DEBUG_USART_TX_AF);



  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART_IRQ;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  USART_ITConfig(DEBUG_USART, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART1, ENABLE);
}


/*****************  ����һ���ַ� **********************/
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch)
{
	uint32_t timeout = 0x100000;

	/* ����һ���ֽ����ݵ�USART */
	USART_SendData(pUSARTx,ch);

	/* �ȴ��������ݼĴ���Ϊ�� */
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET) {
		if (timeout-- == 0) {
			break;
		}
	}
}


/*****************  �����ַ��� **********************/
void Usart_SendString( USART_TypeDef * pUSARTx, char *str)
{
	unsigned int k=0;
  do
  {
      Usart_SendByte( pUSARTx, *(str + k) );
      k++;
  } while(*(str + k)!='\0');

  /* �ȴ�������� */
  while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
  {}
}

/*****************  newlib retarget：让 printf 从 USART1 出 **********************/
/* CMakeLists 用了 --specs=nosys.specs，默认的 _write 是返回 -1 的桩函数，
 * printf 调用后实际什么都不输出。这里覆盖 _write，把 stdout/stderr 引到 USART1。
 *
 * 注意：Worker_Task 现在用整数毫伏输出 ADC 电压，不再依赖 nano newlib
 * 的浮点 printf 支持，避免 ADC 任务拉入较重的 %f 格式化路径。
 *
 * 这里没用 mutex 保护——目前只有 Worker_Task 调 printf，UI_Task 不调，
 * 没有重入风险。GTP_INFO/GTP_ERROR 宏理论上能在触摸 ISR 里调（I2C 读写
 * 失败时），印一段就阻塞一段，不优雅，但量小可以接受。
 */
#include <sys/stat.h>

int _write(int file, char *ptr, int len)
{
	int i;
	(void)file;
	for (i = 0; i < len; i++) {
		Usart_SendByte(USART1, (uint8_t)ptr[i]);
	}
	return len;
}

/* 下面这些桩是 nosys.specs 已经提供过的，但有的链接器会因为我们提供了 _write
 * 而要求把整套也带上。提供弱实现兜底。
 */
__attribute__((weak)) int _read(int file, char *ptr, int len)  { (void)file; (void)ptr; (void)len; return 0; }
__attribute__((weak)) int _close(int file)                     { (void)file; return -1; }
__attribute__((weak)) int _fstat(int file, struct stat *st)    { (void)file; (void)st; return 0; }
__attribute__((weak)) int _isatty(int file)                    { (void)file; return 1; }
__attribute__((weak)) int _lseek(int file, int ptr, int dir)   { (void)file; (void)ptr; (void)dir; return 0; }

//made by









