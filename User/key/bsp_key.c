#include "bsp_key.h"

/*
*********************************************************************************************************
*                                            引脚映射
*********************************************************************************************************
*/
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} s_key_hw[KEY_ID_COUNT] = {
    /* KEY1 */ { GPIOA, GPIO_Pin_0  },
    /* KEY2 */ { GPIOC, GPIO_Pin_13 },
};

#define KEY_DEBOUNCE_SAMPLES  (KEY_DEBOUNCE_MS / KEY_SCAN_PERIOD_MS)   /* 3 */

/*
*********************************************************************************************************
*                                            内部状态
*********************************************************************************************************
*/
static uint8_t s_stable[KEY_ID_COUNT];    /* 消抖后的电平，1 = 按住 */
static uint8_t s_counter[KEY_ID_COUNT];   /* 与当前稳定值不同的连续采样数 */

/* 上电时如果按键已经被按住，先要求松开一次才允许点动。
   否则复位瞬间就自己跑起来，机构有可能直接顶到限位。 */
static uint8_t s_armed[KEY_ID_COUNT];

static uint8_t Key_RawPressed(KeyId_t key);

/*
*********************************************************************************************************
*                                            初始化
*********************************************************************************************************
*/
void Key_Init(void) {
    GPIO_InitTypeDef gpio;
    uint32_t i;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_PuPd  = GPIO_PuPd_DOWN;   /* 按下 = 高电平 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    gpio.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOC, &gpio);

    for (i = 0u; i < (uint32_t)KEY_ID_COUNT; i++) {
        s_stable[i]  = Key_RawPressed((KeyId_t)i);
        s_counter[i] = 0u;
        /* 上电即被按住的键保持"未武装"，等到松开一次才允许动作 */
        s_armed[i]   = (uint8_t)(s_stable[i] == 0u);
    }
}

/*
*********************************************************************************************************
*                                            原始电平
*********************************************************************************************************
*/
static uint8_t Key_RawPressed(KeyId_t key) {
    if (key >= KEY_ID_COUNT) {
        return 0u;
    }
    return (GPIO_ReadInputDataBit(s_key_hw[key].port, s_key_hw[key].pin) != Bit_RESET) ? 1u : 0u;
}

/*
*********************************************************************************************************
*                                        周期扫描（10ms 一次）
*********************************************************************************************************
*/
void Key_Scan(void) {
    uint32_t i;

    for (i = 0u; i < (uint32_t)KEY_ID_COUNT; i++) {
        uint8_t raw = Key_RawPressed((KeyId_t)i);

        if (raw == s_stable[i]) {
            s_counter[i] = 0u;
            continue;
        }

        if (s_counter[i] < KEY_DEBOUNCE_SAMPLES) {
            s_counter[i]++;
        }

        if (s_counter[i] >= KEY_DEBOUNCE_SAMPLES) {
            s_stable[i]  = raw;
            s_counter[i] = 0u;
        }
    }

    /* 任何一个键回到松开状态，就把它"武装"起来 */
    for (i = 0u; i < (uint32_t)KEY_ID_COUNT; i++) {
        if (s_stable[i] == 0u) {
            s_armed[i] = 1u;
        }
    }
}

/*
*********************************************************************************************************
*                                            状态查询
*********************************************************************************************************
*/
uint8_t Key_IsHeld(KeyId_t key) {
    if (key >= KEY_ID_COUNT) {
        return 0u;
    }
    return (uint8_t)((s_stable[key] != 0u) && (s_armed[key] != 0u));
}

uint8_t Key_IsAnyHeld(void) {
    return (uint8_t)(Key_IsHeld(KEY_ID_FORWARD) || Key_IsHeld(KEY_ID_REVERSE));
}

const char *Key_Name(KeyId_t key) {
    switch (key) {
        case KEY_ID_FORWARD: return "KEY1";
        case KEY_ID_REVERSE: return "KEY2";
        default:             return "?";
    }
}
