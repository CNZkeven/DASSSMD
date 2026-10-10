/**
  ******************************************************************************
  * @file    gt5xx.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   i2c��������������gt9157оƬ
  ******************************************************************************
  * @attention
  *
  * ʵ��ƽ̨:Ұ��  STM32 F407 ������ 
  * ��̳    :http://www.firebbs.cn
  * �Ա�    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */ 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./touch/gt9xx.h"
#include "./touch/bsp_i2c_touch.h"
#include "./lcd/bsp_nt35510_lcd.h"
#include "./touch/palette.h"
// 4.5����GT5688��������
const uint8_t CTP_CFG_GT5688[] =  {
			0x96,0xE0,0x01,0x56,0x03,0x05,0x35,0x00,0x01,0x00,
			0x00,0x05,0x50,0x3C,0x53,0x11,0x00,0x00,0x22,0x22,
			0x14,0x18,0x1A,0x1D,0x0A,0x04,0x00,0x00,0x00,0x00,
			0x00,0x00,0x53,0x00,0x14,0x00,0x00,0x84,0x00,0x00,
			0x3C,0x19,0x19,0x64,0x1E,0x28,0x88,0x29,0x0A,0x2D,
			0x2F,0x29,0x0C,0x20,0x33,0x60,0x13,0x02,0x24,0x00,
			0x00,0x20,0x3C,0xC0,0x14,0x02,0x00,0x00,0x54,0xAC,
			0x24,0x9C,0x29,0x8C,0x2D,0x80,0x32,0x77,0x37,0x6E,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x50,0x3C,
			0xFF,0xFF,0x07,0x00,0x00,0x00,0x02,0x14,0x14,0x03,
			0x04,0x00,0x21,0x64,0x0A,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x32,0x20,0x50,0x3C,0x3C,0x00,0x00,0x00,0x00,0x00,
			0x0D,0x06,0x0C,0x05,0x0B,0x04,0x0A,0x03,0x09,0x02,
			0xFF,0xFF,0xFF,0xFF,0x00,0x01,0x02,0x03,0x04,0x05,
			0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
			0x10,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x3C,0x00,0x05,0x1E,0x00,0x02,
			0x2A,0x1E,0x19,0x14,0x02,0x00,0x03,0x0A,0x05,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xFF,0xFF,0x86,
			0x22,0x03,0x00,0x00,0x33,0x00,0x0F,0x00,0x00,0x00,
			0x50,0x3C,0x50,0x00,0x00,0x00,0x1A,0x64,0x01

};

//GT9147���ò�����
//��һ���ֽ�Ϊ�汾��(0X60),���뱣֤�µİ汾�Ŵ��ڵ���GT9147�ڲ�
//flashԭ�а汾��,�Ż��������.
const u8 CTP_CFG_GT9147[]=
{
  0x99,0xE0,0x01,0x20,0x03,0x05,0x34,0x00,0x02,
0x08,0x1E,0x08,0x50,0x3C,0x0F,0x05,0x00,0x00,
0xFF,0x67,0x02,0x02,0x00,0x18,0x1A,0x1E,0x14,
0x88,0x28,0x0A,0x55,0x57,0xD3,0x07,0x03,0x00,
0x00,0x42,0x32,0x1D,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x32,0x00,0x00,0x2A,0x4B,0x78,0x94,
0xD5,0x02,0x07,0x00,0x00,0x04,0x88,0x4E,0x00,
0x7E,0x56,0x00,0x76,0x5E,0x00,0x6E,0x68,0x00,
0x67,0x72,0x00,0x67,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0F,0x0F,0x03,0x06,0x10,0x42,0xF8,0x0F,0x14,
0x00,0x00,0x00,0x00,0x1A,0x18,0x16,0x14,0x12,
0x10,0x0E,0x0C,0x0A,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
0x04,0x05,0x06,0x08,0x0A,0x0C,0x1D,0x1E,0x1F,
0x20,0x22,0x24,0x28,0x29,0xFF,0xFF,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0x37,0x01
//	0x99,0XE0,0X01,0X20,0X03,0X05,0X35,0X00,0X02,0X08,
//	0X1E,0X08,0X50,0X3C,0X0F,0X05,0X00,0X00,0XFF,0X67,
//	0X50,0X00,0X00,0X18,0X1A,0X1E,0X14,0X89,0X28,0X0A,
//	0X30,0X2E,0XBB,0X0A,0X03,0X00,0X00,0X02,0X33,0X1D,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X32,0X00,0X00,
//	0X2A,0X1C,0X5A,0X94,0XC5,0X02,0X07,0X00,0X00,0X00,
//	0XB5,0X1F,0X00,0X90,0X28,0X00,0X77,0X32,0X00,0X62,
//	0X3F,0X00,0X52,0X50,0X00,0X52,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X0F,
//	0X0F,0X03,0X06,0X10,0X42,0XF8,0X0F,0X14,0X00,0X00,
//	0X00,0X00,0X1A,0X18,0X16,0X14,0X12,0X10,0X0E,0X0C,
//	0X0A,0X08,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X29,0X28,0X24,0X22,0X20,0X1F,0X1E,0X1D,
//	0X0E,0X0C,0X0A,0X08,0X06,0X05,0X04,0X02,0X00,0XFF,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,
//	0XFF,0XFF,0XFF,0XFF,
};  

const uint8_t CTP_CFG_GT917S[] =  {
  0x84,0x20,0x03,0xE0,0x01,0x05,0x05,0x00,0x00,0x40,
  0x00,0x0F,0x78,0x64,0x53,0x11,0x00,0x00,0x00,0x00,
  0x23,0x17,0x19,0x1D,0x0F,0x04,0x00,0x00,0x00,0x00,
  0x00,0x00,0x04,0x51,0x14,0x00,0x00,0x00,0x00,0x00,
  0x32,0x00,0x00,0x50,0x38,0x28,0x8A,0x20,0x11,0x37,
  0x39,0xA2,0x07,0x38,0x6D,0x28,0x11,0x03,0x24,0x00,
  0x01,0x28,0x50,0xC0,0x94,0x02,0x00,0x00,0x53,0xB8,
  0x2E,0xA2,0x35,0x8F,0x3B,0x80,0x42,0x75,0x49,0x6B,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x4C,0x3C,
  0xFF,0xFF,0x07,0x14,0x14,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x73,
  0x50,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x1F,0x1D,0x1B,0x1A,0x19,0x18,0x17,0x16,0x15,0x09,
  0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0x1C,0x1B,0x1A,0x19,0x18,0x17,0x15,0x14,
  0x13,0x12,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x05,0x00,0x00,0x0F,
  0x00,0x00,0x00,0x80,0x46,0x08,0x96,0x50,0x32,0x0A,
  0x0A,0x64,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x32,0x03,0x0C,0x08,0x23,0x00,0x14,0x23,0x00,0x28,
  0x46,0x30,0x3C,0xD0,0x07,0x50,0x70,0xB0,0x01
};

const uint8_t CTP_CFG_GT911[] =  {
  0x53,0x20,0x03,0xE0,0x01,0x05,0x0D,0x10,0x01,0x18,
  0x28,0x0F,0x50,0x32,0x03,0x05,0x00,0x00,0x00,0x00,
  0x11,0x11,0x05,0x18,0x1A,0x1E,0x14,0x88,0x29,0x0A,
  0x52,0x50,0x40,0x04,0x00,0x00,0x00,0x1A,0x32,0x1C,
  0x00,0x01,0x00,0x0F,0x00,0x2A,0xFF,0x7F,0x00,0x50,
  0x32,0x3C,0x64,0x94,0xD5,0x02,0x07,0x00,0x00,0x04,
  0x9F,0x3F,0x00,0x90,0x46,0x00,0x84,0x4D,0x00,0x79,
  0x55,0x00,0x6D,0x5F,0x00,0x6D,0x00,0x00,0x00,0x00,
  0xF0,0x4A,0x3A,0xFF,0xFF,0x27,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,
  0x12,0x14,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0x22,0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,
  0x16,0x12,0x10,0x0F,0x08,0x06,0x04,0x02,0x00,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0x17,0x01
};

//uint8_t config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
//                = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

TOUCH_IC touchIC = GT917S;			

const TOUCH_PARAM_TypeDef touch_param[2] = 
{
  /* GT917S,4.3���� */
  {
  .max_width = 800,
  .max_height = 480,
  .config_reg_addr = 0x8050,
  },
  
  /* GT911,4.3���� */
  {
  .max_width = 800,
  .max_height = 480,
  .config_reg_addr = 0x8047,
  },
};
						

static int8_t GTP_I2C_Test(void);

#if UPDATE_CONFIG
static void Delay(__IO uint32_t nCount)	 //�򵥵���ʱ����
{
	for(; nCount != 0; nCount--);
}
#endif


/**
  * @brief   ʹ��IIC�������ݴ���
  * @param
  *		@arg i2c_msg:���ݴ���ṹ��
  *		@arg num:���ݴ���ṹ��ĸ���
  * @retval  ������ɵĴ���ṹ��������������������0xff
  */
static int I2C_Transfer( struct i2c_msg *msgs,int num)
{
	int im = 0;
	int ret = 0;

	GTP_DEBUG_FUNC();

	for (im = 0; ret == 0 && im != num; im++)
	{
		if ((msgs[im].flags&I2C_M_RD))																//����flag�ж��Ƕ����ݻ���д����
		{
			ret = I2C_ReadBytes(msgs[im].addr, msgs[im].buf, msgs[im].len);		//IIC��ȡ����
		} else
		{
			ret = I2C_WriteBytes(msgs[im].addr,  msgs[im].buf, msgs[im].len);	//IICд������
		}
	}

	if(ret)
		return -ret;

	return im;   													//������ɵĴ���ṹ����
}

/**
  * @brief   ��IIC�豸�ж�ȡ����
  * @param
  *		@arg client_addr:�豸��ַ
  *		@arg  buf[0~1]: ��ȡ���ݼĴ�������ʼ��ַ
  *		@arg buf[2~len-1]: �洢���������ݵĻ���buffer
  *		@arg len:    GTP_ADDR_LENGTH + read bytes count���Ĵ�����ַ����+��ȡ�������ֽ�����
  * @retval  i2c_msgs����ṹ��ĸ�����2Ϊ�ɹ�������Ϊʧ��
  */
static int32_t GTP_I2C_Read(uint8_t client_addr, uint8_t *buf, int32_t len)
{
    struct i2c_msg msgs[2];
    int32_t ret=-1;
    int32_t retries = 0;

    GTP_DEBUG_FUNC();
    /*һ�������ݵĹ��̿��Է�Ϊ�����������:
     * 1. IIC  д�� Ҫ��ȡ�ļĴ�����ַ
     * 2. IIC  ��ȡ  ����
     * */

    msgs[0].flags = !I2C_M_RD;					//д��
    msgs[0].addr  = client_addr;					//IIC�豸��ַ
    msgs[0].len   = GTP_ADDR_LENGTH;	//�Ĵ�����ַΪ2�ֽ�(��д�����ֽڵ�����)
    msgs[0].buf   = &buf[0];						//buf[0~1]�洢����Ҫ��ȡ�ļĴ�����ַ
    
    msgs[1].flags = I2C_M_RD;					//��ȡ
    msgs[1].addr  = client_addr;					//IIC�豸��ַ
    msgs[1].len   = len - GTP_ADDR_LENGTH;	//Ҫ��ȡ�����ݳ���
    msgs[1].buf   = &buf[GTP_ADDR_LENGTH];	//buf[GTP_ADDR_LENGTH]֮��Ļ������洢����������

    while(retries < 5)
    {
        ret = I2C_Transfer( msgs, 2);					//����IIC���ݴ�����̺�������2���������
        if(ret == 2)break;
        retries++;
    }
    if((retries >= 5))
    {
        GTP_ERROR("I2C Read: 0x%04X, %ld bytes failed, errcode: %ld! Process reset.", (((uint16_t)(buf[0] << 8)) | buf[1]), (long)(len - 2), (long)ret);
    }
    return ret;
}



/**
  * @brief   ��IIC�豸д������
  * @param
  *		@arg client_addr:�豸��ַ
  *		@arg  buf[0~1]: Ҫд������ݼĴ�������ʼ��ַ
  *		@arg buf[2~len-1]: Ҫд�������
  *		@arg len:    GTP_ADDR_LENGTH + write bytes count���Ĵ�����ַ����+д��������ֽ�����
  * @retval  i2c_msgs����ṹ��ĸ�����1Ϊ�ɹ�������Ϊʧ��
  */
static int32_t GTP_I2C_Write(uint8_t client_addr,uint8_t *buf,int32_t len)
{
    struct i2c_msg msg;
    int32_t ret = -1;
    int32_t retries = 0;

    GTP_DEBUG_FUNC();
    /*һ��д���ݵĹ���ֻ��Ҫһ���������:
     * 1. IIC���� д�� ���ݼĴ�����ַ������
     * */
    msg.flags = !I2C_M_RD;			//д��
    msg.addr  = client_addr;			//���豸��ַ
    msg.len   = len;							//����ֱ�ӵ���(�Ĵ�����ַ����+д��������ֽ���)
    msg.buf   = buf;						//ֱ������д�뻺�����е�����(�����˼Ĵ�����ַ)

    while(retries < 5)
    {
        ret = I2C_Transfer(&msg, 1);	//����IIC���ݴ�����̺�����1���������
        if (ret == 1)break;
        retries++;
    }
    if((retries >= 5))
    {

        GTP_ERROR("I2C Write: 0x%04X, %ld bytes failed, errcode: %ld! Process reset.", (((uint16_t)(buf[0] << 8)) | buf[1]), (long)(len - 2), (long)ret);

    }
    return ret;
}



/**
  * @brief   ʹ��IIC��ȡ�ٴ����ݣ������Ƿ�����
  * @param
  *		@arg client:�豸��ַ
  *		@arg  addr: �Ĵ�����ַ
  *		@arg rxbuf: �洢����������
  *		@arg len:    ��ȡ���ֽ���
  * @retval
  * 	@arg FAIL
  * 	@arg SUCCESS
  */
 int32_t GTP_I2C_Read_dbl_check(uint8_t client_addr, uint16_t addr, uint8_t *rxbuf, int len)
{
    uint8_t buf[16] = {0};
    uint8_t confirm_buf[16] = {0};
    uint8_t retry = 0;
    
    GTP_DEBUG_FUNC();

    while (retry++ < 3)
    {
        memset(buf, 0xAA, 16);
        buf[0] = (uint8_t)(addr >> 8);
        buf[1] = (uint8_t)(addr & 0xFF);
        GTP_I2C_Read(client_addr, buf, len + 2);
        
        memset(confirm_buf, 0xAB, 16);
        confirm_buf[0] = (uint8_t)(addr >> 8);
        confirm_buf[1] = (uint8_t)(addr & 0xFF);
        GTP_I2C_Read(client_addr, confirm_buf, len + 2);

      
        if (!memcmp(buf, confirm_buf, len+2))
        {
            memcpy(rxbuf, confirm_buf+2, len);
            return SUCCESS;
        }
    }    
    GTP_ERROR("I2C read 0x%04X, %d bytes, double check failed!", addr, len);
    return FAIL;
}


/**
  * @brief   �ر�GT91xx�ж�
  * @param ��
  * @retval ��
  */
void GTP_IRQ_Disable(void)
{

    GTP_DEBUG_FUNC();

    I2C_GTP_IRQDisable();
}

/**
  * @brief   ʹ��GT91xx�ж�
  * @param ��
  * @retval ��
  */
void GTP_IRQ_Enable(void)
{
    GTP_DEBUG_FUNC();
     
	  I2C_GTP_IRQEnable();    
}


/**
  * @brief   ���ڴ����򱨸津����⵽����
  * @param
  *    @arg     id: ����˳��trackID
  *    @arg     x:  ������ x ����
  *    @arg     y:  ������ y ����
  *    @arg     w:  ������ ��С
  * @retval ��
  */
/*���ڼ�¼��������ʱ(����)����һ�δ���λ�ã�����ֵ��ʾ��һ���޴�������*/
static int16_t pre_x[GTP_MAX_TOUCH] ={-1,-1,-1,-1,-1};
static int16_t pre_y[GTP_MAX_TOUCH] ={-1,-1,-1,-1,-1};

/* Global variables for GTP_GetTouchPoint and event extraction. */
static volatile uint8_t g_touch_flag = 0;
static volatile uint16_t g_touch_x = 0;
static volatile uint16_t g_touch_y = 0;
static uint16_t g_pre_touch = 0;
static uint8_t g_pre_id[GTP_MAX_TOUCH] = {0};
static uint8_t g_touch_ambiguous = 0;

static void GTP_Touch_ResetState(void)
{
    uint8_t i;

    g_pre_touch = 0;
    g_touch_ambiguous = 0;
    for (i = 0; i < GTP_MAX_TOUCH; i++) {
        g_pre_id[i] = 0;
        pre_x[i] = -1;
        pre_y[i] = -1;
    }
    g_touch_flag = 0;
    g_touch_x = 0;
    g_touch_y = 0;
}

static void GTP_Touch_Down(int32_t id, int32_t x, int32_t y, int32_t w,
                           uint8_t was_active, GTP_TouchEvent_t *event)
{
    (void)w;
    if (id < 0 || id >= GTP_MAX_TOUCH) {
        return;
    }

    pre_x[id] = (int16_t)x;
    pre_y[id] = (int16_t)y;

    if (event != NULL && event->type == GTP_EVENT_NONE) {
        event->type = was_active ? GTP_EVENT_MOVE : GTP_EVENT_DOWN;
        event->x = (uint16_t)x;
        event->y = (uint16_t)y;
        g_touch_flag = 1;
        g_touch_x = (uint16_t)x;
        g_touch_y = (uint16_t)y;
    }
}

static void GTP_Touch_Up(int32_t id, GTP_TouchEvent_t *event)
{
    if (id < 0 || id >= GTP_MAX_TOUCH) {
        return;
    }

    pre_x[id] = -1;
    pre_y[id] = -1;
    g_touch_flag = 0;
    g_touch_x = 0;
    g_touch_y = 0;

    if (event != NULL && event->type == GTP_EVENT_NONE) {
        event->type = GTP_EVENT_UP;
        event->x = 0;
        event->y = 0;
    }
}


/**
  * @brief   Get touch point coordinates
  * @param   x: pointer to store x coordinate
  * @param   y: pointer to store y coordinate
  * @retval  1 if touched, 0 if not touched
  */
uint8_t GTP_GetTouchPoint(uint16_t *x, uint16_t *y)
{
    if (g_touch_flag) {
        *x = g_touch_x;
        *y = g_touch_y;
        return 1;
    }
    return 0;
}


/**
  * @brief   ����������������ѯ�����ڴ����жϵ���
  * @param ��
  * @retval ��
  */
static void Goodix_TS_Work_Func(GTP_TouchEvent_t *event)
{
    uint8_t end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
    uint8_t point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {
        GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF
    };
    uint8_t current_id[GTP_MAX_TOUCH] = {0};
    uint8_t touch_num = 0;
    uint8_t finger;
    uint8_t id;
    uint8_t current_ids_valid = 1;
    uint8_t was_active;
    uint8_t found;
    uint8_t i;
    uint8_t j;
    uint8_t *coor_data;
    uint8_t client_addr = GTP_ADDRESS;
    int32_t input_x;
    int32_t input_y;
    int32_t input_w;
    int32_t temp_x;
    int32_t temp_y;
    int32_t ret;

    if (event == NULL) {
        return;
    }
    event->type = GTP_EVENT_NONE;
    event->x = 0;
    event->y = 0;
    event->touch_count = 0;

    GTP_DEBUG_FUNC();

    ret = GTP_I2C_Read(client_addr, point_data, 12);
    if (ret != 2) {
        event->type = GTP_EVENT_ERROR;
        GTP_Touch_ResetState();
        return;
    }

    finger = point_data[GTP_ADDR_LENGTH];
    if (finger == 0x00) {
        /* No fresh coordinate report: preserve the ordinary held-point state. */
        return;
    }

    if ((finger & 0x80) == 0) {
        goto exit_work_func;
    }

    touch_num = finger & 0x0F;
    event->touch_count = touch_num;
    if (touch_num > GTP_MAX_TOUCH) {
        event->type = GTP_EVENT_MULTI;
        g_touch_ambiguous = 1;
        g_touch_flag = 0;
        g_touch_x = 0;
        g_touch_y = 0;
        g_pre_touch = touch_num;
        goto exit_work_func;
    }

    if (touch_num > 1) {
        uint8_t buf[2 + 8 * (GTP_MAX_TOUCH - 1)] = {
            (GTP_READ_COOR_ADDR + 10) >> 8,
            (GTP_READ_COOR_ADDR + 10) & 0xFF
        };

        ret = GTP_I2C_Read(client_addr, buf, 2 + 8 * (touch_num - 1));
        if (ret != 2) {
            event->type = GTP_EVENT_ERROR;
            GTP_Touch_ResetState();
            goto exit_work_func;
        }
        memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
    }

    for (i = 0; i < touch_num; i++) {
        coor_data = &point_data[i * 8 + 3];
        current_id[i] = coor_data[0] & 0x0F;
        if (current_id[i] >= GTP_MAX_TOUCH) {
            current_ids_valid = 0;
        }
    }

    if (touch_num > 1 || !current_ids_valid || g_touch_ambiguous || g_pre_touch > 1) {
        event->type = GTP_EVENT_MULTI;
        g_touch_ambiguous = 1;
        g_touch_flag = 0;
        g_touch_x = 0;
        g_touch_y = 0;
    } else if (touch_num == 1) {
        if (g_pre_touch == 1 && g_pre_id[0] != current_id[0]) {
            /* A single output cannot encode release-old plus down-new. */
            event->type = GTP_EVENT_MULTI;
            g_touch_ambiguous = 1;
            g_touch_flag = 0;
            g_touch_x = 0;
            g_touch_y = 0;
        }
    } else if (g_pre_touch == 1) {
        GTP_Touch_Up(g_pre_id[0], event);
    } else if (g_pre_touch > 1 || g_touch_ambiguous) {
        event->type = GTP_EVENT_MULTI;
        g_touch_ambiguous = 0;
        GTP_Touch_ResetState();
    } else {
        g_touch_flag = 0;
        g_touch_x = 0;
        g_touch_y = 0;
    }

    if (touch_num == 0 && event->type == GTP_EVENT_MULTI) {
        /* A fully lifted ambiguous gesture is a stroke boundary. */
        GTP_Touch_ResetState();
    } else {
        /* Discard state for IDs that disappeared without a representable UP. */
        for (i = 0; i < g_pre_touch && i < GTP_MAX_TOUCH; i++) {
            found = 0;
            for (j = 0; j < touch_num; j++) {
                if (g_pre_id[i] == current_id[j]) {
                    found = 1;
                    break;
                }
            }
            if (!found && g_pre_id[i] < GTP_MAX_TOUCH) {
                pre_x[g_pre_id[i]] = -1;
                pre_y[g_pre_id[i]] = -1;
            }
        }

        for (i = 0; i < touch_num; i++) {
            coor_data = &point_data[i * 8 + 3];
            id = current_id[i];
            input_x = coor_data[1] | (coor_data[2] << 8);
            input_y = coor_data[3] | (coor_data[4] << 8);
            input_w = coor_data[5] | (coor_data[6] << 8);

            temp_x = input_x;
            temp_y = input_y;
            switch (LCD_SCAN_MODE) {
                case 0:
                    input_x = LCD_X_LENGTH - temp_y;
                    input_y = temp_x;
                    break;
                case 1:
                    input_y = LCD_Y_LENGTH - input_y;
                    break;
                case 2:
                    input_x = temp_y;
                    input_y = temp_x;
                    break;
                case 3:
                    break;
                case 4:
                    input_x = LCD_X_LENGTH - temp_y;
                    input_y = LCD_Y_LENGTH - temp_x;
                    break;
                case 5:
                    input_x = LCD_X_LENGTH - input_x;
                    input_y = LCD_Y_LENGTH - input_y;
                    break;
                case 6:
                    input_x = temp_y;
                    input_y = LCD_Y_LENGTH - temp_x;
                    break;
                case 7:
                    input_x = LCD_X_LENGTH - input_x;
                    break;
                default:
                    break;
            }

            if (id < GTP_MAX_TOUCH) {
                was_active = 0;
                for (j = 0; j < g_pre_touch && j < GTP_MAX_TOUCH; j++) {
                    if (g_pre_id[j] == id) {
                        was_active = 1;
                        break;
                    }
                }
                GTP_Touch_Down(id, input_x, input_y, input_w, was_active, event);
            }
        }

        if (touch_num == 0) {
            g_pre_touch = 0;
            for (i = 0; i < GTP_MAX_TOUCH; i++) {
                g_pre_id[i] = 0;
            }
            /* A staggered multi-touch release may arrive as UP rather than
               MULTI; clear the ambiguity latch at the final lift. */
            GTP_Touch_ResetState();
        } else {
            g_pre_touch = touch_num;
            for (i = 0; i < touch_num; i++) {
                g_pre_id[i] = current_id[i];
            }
        }
    }

exit_work_func:
    ret = GTP_I2C_Write(client_addr, end_cmd, 3);
    if (ret != 1) {
        event->type = GTP_EVENT_ERROR;
        event->x = 0;
        event->y = 0;
        event->touch_count = 0;
        GTP_Touch_ResetState();
    }
}
/**
  * @brief   ������оƬ���¸�λ
  * @param ��
  * @retval ��
  */
 int8_t GTP_Reset_Guitar(void)
{
    GTP_DEBUG_FUNC();
#if 1
    I2C_ResetChip();
    return 0;
#else 		//������λ
    int8_t ret = -1;
    int8_t retry = 0;
    uint8_t reset_command[3]={(uint8_t)GTP_REG_COMMAND>>8,(uint8_t)GTP_REG_COMMAND&0xFF,2};

    //д�븴λ����
    while(retry++ < 5)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, reset_command, 3);
        if (ret > 0)
        {
            GTP_INFO("GTP enter sleep!");

            return ret;
        }

    }
    GTP_ERROR("GTP send sleep cmd failed.");
    return ret;
#endif

}



 /**
   * @brief   ����˯��ģʽ
   * @param ��
   * @retval 1Ϊ�ɹ�������Ϊʧ��
   */
//int8_t GTP_Enter_Sleep(void)
//{
//    int8_t ret = -1;
//    int8_t retry = 0;
//    uint8_t reset_comment[3] = {(uint8_t)(GTP_REG_COMMENT >> 8), (uint8_t)GTP_REG_COMMENT&0xFF, 5};//5
//
//    GTP_DEBUG_FUNC();
//
//    while(retry++ < 5)
//    {
//        ret = GTP_I2C_Write(GTP_ADDRESS, reset_comment, 3);
//        if (ret > 0)
//        {
//            GTP_INFO("GTP enter sleep!");
//
//            return ret;
//        }
//
//    }
//    GTP_ERROR("GTP send sleep cmd failed.");
//    return ret;
//}


int8_t GTP_Send_Command(uint8_t command)
{
    int8_t ret = -1;
    int8_t retry = 0;
    uint8_t command_buf[3] = {(uint8_t)(GTP_REG_COMMAND >> 8), (uint8_t)GTP_REG_COMMAND&0xFF, GTP_COMMAND_READSTATUS};

    GTP_DEBUG_FUNC();
    command_buf[2] = command;

    while(retry++ < 5)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, command_buf, 3);
        if (ret > 0)
        {
            GTP_INFO("send command success!");

            return ret;
        }

    }
    GTP_ERROR("send command fail!");
    return ret;
}

/**
  * @brief   ���Ѵ�����
  * @param ��
  * @retval 0Ϊ�ɹ�������Ϊʧ��
  */
int8_t GTP_WakeUp_Sleep(void)
{
    uint8_t retry = 0;
    int8_t ret = -1;

    GTP_DEBUG_FUNC();

    while(retry++ < 10)
    {
        ret = GTP_I2C_Test();
        if (ret > 0)
        {
            GTP_INFO("GTP wakeup sleep.");
            return ret;
        }
        GTP_Reset_Guitar();
    }

    GTP_ERROR("GTP wakeup sleep failed.");
    return ret;
}

static int32_t GTP_Get_Info(void)
{
    uint8_t opr_buf[10] = {0};
    int32_t ret = 0;

    uint16_t abs_x_max = GTP_MAX_WIDTH;
    uint16_t abs_y_max = GTP_MAX_HEIGHT;
    uint8_t int_trigger_type = GTP_INT_TRIGGER;
        
    opr_buf[0] = (uint8_t)((GTP_REG_CONFIG_DATA+1) >> 8);
    opr_buf[1] = (uint8_t)((GTP_REG_CONFIG_DATA+1) & 0xFF);
    
    ret = GTP_I2C_Read(GTP_ADDRESS, opr_buf, 10);
    if (ret < 0)
    {
        return FAIL;
    }
    
    abs_x_max = (opr_buf[3] << 8) + opr_buf[2];
    abs_y_max = (opr_buf[5] << 8) + opr_buf[4];
		GTP_DEBUG("RES");   
		GTP_DEBUG_ARRAY(&opr_buf[0],10);

    opr_buf[0] = (uint8_t)((GTP_REG_CONFIG_DATA+6) >> 8);
    opr_buf[1] = (uint8_t)((GTP_REG_CONFIG_DATA+6) & 0xFF);
    ret = GTP_I2C_Read(GTP_ADDRESS, opr_buf, 3);
    if (ret < 0)
    {
        return FAIL;
    }
    int_trigger_type = opr_buf[2] & 0x03;
    
    GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
            abs_x_max,abs_y_max,int_trigger_type);
    
    return SUCCESS;    
}

/*******************************************************
Function:
    Initialize gtp.
Input:
    ts: goodix private data
Output:
    Executive outcomes.
        0: succeed, otherwise: failed
*******************************************************/
 int32_t GTP_Init_Panel(void)
{
    int32_t ret = -1;

#if UPDATE_CONFIG
    int32_t i = 0;
    uint16_t check_sum = 0;
    int32_t retry = 0;

    const uint8_t* cfg_info;
    uint8_t cfg_info_len  ;
		uint8_t* config;

    uint8_t cfg_num =0 ;		//��Ҫ���õļĴ�������
#endif

    GTP_DEBUG_FUNC();
	
//uint8_t config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
//                = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

		
	
    /* I2C_Touch_Init() 已经在 UI_Task 里调用过了，这里不再重复调用。
     * 重复调用会再次复位触摸芯片，可能导致初始化失败。
     */

    ret = GTP_I2C_Test();
    if (ret < 0)
    {
        GTP_ERROR("I2C communication ERROR!");
				return ret;
    }

		//读取触摸IC的型号
    GTP_Read_Version();
    
#if UPDATE_CONFIG
    
    config = (uint8_t *)malloc (GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);

		config[0] = GTP_REG_CONFIG_DATA >> 8;
		config[1] =  GTP_REG_CONFIG_DATA & 0xff;
		
		//����IC���ͺ�ָ��ͬ������
		if(touchIC == GT5688)
		{
			cfg_info =  CTP_CFG_GT5688; //ָ��Ĵ�������
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT5688);//�������ñ��Ĵ�С
		}
		else if(touchIC == GT917S)
		{
			cfg_info =  CTP_CFG_GT917S; //ָ��Ĵ�������
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT917S);//�������ñ��Ĵ�С
		}
        else if(touchIC == GT911)
		{
			cfg_info =  CTP_CFG_GT911; //ָ��Ĵ�������
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT911);//�������ñ��Ĵ�С
		}
		else
		{//Ĭ������ΪGT917S
			cfg_info =  CTP_CFG_GT917S; //ָ��Ĵ�������
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT917S);//�������ñ��Ĵ�С
		}
		
    memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
    memcpy(&config[GTP_ADDR_LENGTH], cfg_info, cfg_info_len);
		

		cfg_num = cfg_info_len;
		
		GTP_DEBUG("cfg_info_len = %d ",cfg_info_len);
		GTP_DEBUG("cfg_num = %d ",cfg_num);
		GTP_DEBUG_ARRAY(config,6);
		
		/*����LCD��ɨ�跽�����÷ֱ���*/
		config[GTP_ADDR_LENGTH+1] = LCD_X_LENGTH & 0xFF;
		config[GTP_ADDR_LENGTH+2] = LCD_X_LENGTH >> 8;
		config[GTP_ADDR_LENGTH+3] = LCD_Y_LENGTH & 0xFF;
		config[GTP_ADDR_LENGTH+4] = LCD_Y_LENGTH >> 8;
		
		/*����ɨ��ģʽ����X2Y����*/
        if(touchIC == GT917S)
        {
            switch(LCD_SCAN_MODE)
            {
                case 0:case 2:case 4: case 6:
                    config[GTP_ADDR_LENGTH+6] |= (X2Y_LOC);
                    break;
                
                case 1:case 3:case 5: case 7:
                    config[GTP_ADDR_LENGTH+6] &= ~(X2Y_LOC);
                    break;		
            }
        }
        
        //����Ҫд��checksum�Ĵ�����ֵ
        check_sum = 0;

        /* ����check sumУ��ֵ */
        if(touchIC == GT911)
        {
            for (i = GTP_ADDR_LENGTH; i < cfg_num; i++)
            {
                check_sum += (config[i] & 0xFF);
            }
            config[ cfg_num] = (~(check_sum & 0xFF)) + 1; 	//checksum
            config[ cfg_num+1] =  1; 						//refresh ���ø��±�־
        }
        else if(touchIC == GT9157)
        {
            for (i = GTP_ADDR_LENGTH; i < cfg_num+GTP_ADDR_LENGTH; i++)
            {
                check_sum += (config[i] & 0xFF);
            }
            config[ cfg_num+GTP_ADDR_LENGTH] = (~(check_sum & 0xFF)) + 1; 	//checksum
            config[ cfg_num+GTP_ADDR_LENGTH+1] =  1; 						//refresh ���ø��±�־
        }
        else if(touchIC == GT5688 || touchIC == GT917S) 
        {
            for (i = GTP_ADDR_LENGTH; i < (cfg_num+GTP_ADDR_LENGTH -3); i += 2) 
            {
            check_sum += (config[i] << 8) + config[i + 1];
            }

            check_sum = 0 - check_sum;
            GTP_DEBUG("Config checksum: 0x%04X", check_sum);
            //����checksum
            config[(cfg_num+GTP_ADDR_LENGTH -3)] = (check_sum >> 8) & 0xFF;
            config[(cfg_num+GTP_ADDR_LENGTH -2)] = check_sum & 0xFF;
            config[(cfg_num+GTP_ADDR_LENGTH -1)] = 0x01;
        }
        

    //д��������Ϣ
    for (retry = 0; retry < 5; retry++)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, config , cfg_num + GTP_ADDR_LENGTH+2);
        if (ret > 0)
        {
            break;
        }
    }
    Delay(0xfffff);				//�ӳٵȴ�оƬ����
		

		
#if 1	//����д������ݣ�����Ƿ�����д��
    //���������������д����Ƿ���ͬ
	{
    	    uint16_t i;
    	    uint8_t buf[300];
    	     buf[0] = config[0];
    	     buf[1] =config[1];    //�Ĵ�����ַ

    	    GTP_DEBUG_FUNC();

    	    ret = GTP_I2C_Read(GTP_ADDRESS, buf, sizeof(buf));
			   
					GTP_DEBUG("read ");

					GTP_DEBUG_ARRAY(buf,cfg_num);
		
			    GTP_DEBUG("write ");

					GTP_DEBUG_ARRAY(config,cfg_num);

					//���ԱȰ汾��
    	    for(i=1;i<cfg_num+GTP_ADDR_LENGTH-3;i++)
    	    {

    	    	if(config[i] != buf[i])
    	    	{
    	    		GTP_ERROR("Config fail ! i = %d ",i);
							free(config);
    	    		return -1;
    	    	}
    	    }
    	    if(i==cfg_num+GTP_ADDR_LENGTH-3)
	    		GTP_DEBUG("Config success ! i = %d ",i);
	}
#endif
	free(config);
#endif
		
    ret = GTP_Get_Info();
    if (ret != SUCCESS)
    {
        GTP_ERROR("GTP get info failed.");
        return -1;
    }

	 /*ʹ���жϣ��������ܼ�ⴥ������*/
		I2C_GTP_IRQEnable();
		
		

    return 0;
}


/*******************************************************
Function:
    Read chip version.
Input:
    client:  i2c device
    version: buffer to keep ic firmware version
Output:
    read operation return.
        2: succeed, otherwise: failed
*******************************************************/
int32_t GTP_Read_Version(void)
{
    int32_t ret = -1;
    uint8_t buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};    //�Ĵ�����ַ

    GTP_DEBUG_FUNC();

    ret = GTP_I2C_Read(GTP_ADDRESS, buf, sizeof(buf));
    if (ret < 0)
    {
        GTP_ERROR("GTP read version failed");
        return ret;
    }
    if (buf[2] == '5')
    {
        GTP_INFO("IC1 Version: %c%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
				
				//GT5688оƬ
				if(buf[2] == '5' && buf[3] == '6' && buf[4] == '8'&& buf[5] == '8')
					touchIC = GT5688;
    }        
    else if (buf[5] == 0x00)
    {
        GTP_INFO("IC2 Version: %c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[7], buf[6]);
				
				//GT911оƬ
				if(buf[2] == '9' && buf[3] == '1' && buf[4] == '1')
					touchIC = GT911;
    }
    else if (buf[5] == '7')
    {
        GTP_INFO("IC3 Version: %c%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
				
				//GT9147оƬ
				if(buf[2] == '9' && buf[3] == '1' && buf[4] == '4' && buf[5] == '7')
					touchIC = GT9147;
		}
    else
    {
        GTP_INFO("IC3 Version: %c%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
				
				//GT9157оƬ
				if(buf[2] == '9' && buf[3] == '1' && buf[4] == '5' && buf[5] == '7')
					touchIC = GT9157;
		}
    return ret;
}

/*******************************************************
Function:
    I2c test Function.
Input:
    client:i2c client.
Output:
    Executive outcomes.
        2: succeed, otherwise failed.
*******************************************************/
static int8_t GTP_I2C_Test( void)
{
    uint8_t test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
    uint8_t retry = 0;
    int8_t ret = -1;

    GTP_DEBUG_FUNC();
  
    while(retry++ < 5)
    {
        ret = GTP_I2C_Read(GTP_ADDRESS, test, 3);
        if (ret > 0)
        {
            return ret;
        }
        GTP_ERROR("GTP i2c test failed time %d.",retry);
    }
    return ret;
}

//��⵽�����ж�ʱ���ã�
void GTP_TouchProcess(GTP_TouchEvent_t *event)
{
    if (event == NULL) {
        return;
    }
    event->type = GTP_EVENT_NONE;
    event->x = 0;
    event->y = 0;
    event->touch_count = 0;
    Goodix_TS_Work_Func(event);
}






//MODULE_DESCRIPTION("GTP Series Driver");
//MODULE_LICENSE("GPL");
