/*
 * dr16.c - DR16(DBUS) 遥控器解析 + 摇杆->电流映射
 *
 * 硬件接口：DR16 接收机插 A 板 DBUS 口 -> USART1
 *   A 板 DBUS 引脚 = USART1_RX: PB7 / USART1_TX: PB6（见 A 板端口表）
 *   A 板板上已硬件反相，软件不用反相。
 * 串口参数：100000 baud, 8 数据位 + 偶校验(8E1), 1 停止位
 *   STM32F4 上 8E1 需 WordLength=9B(9bit帧) + Parity=Even（校验位占第9位）
 *
 * 协议：每 ~14ms 一帧，18 字节。4 摇杆通道按 11bit 连续位流打包(小端)，
 *       右摇杆 Y = ch1，解析见 DR16_Task()。数据布局与 ARBATOS
 *       ManualInputDbus.h 的官方解码一致。
 *
 * “需要打开什么串口接收中断”：
 *   串口全局中断 USART1_IRQn + 接收事件 RXNE(每字节) + IDLE(一帧结束)。
 *   本驱动在 DR16_Init() 里已通过 NVIC 使能；USART1_IRQHandler 在本文件底部。
 *   若以后用 CubeMX 新建工程：NVIC 勾选 USART1 global interrupt，并在
 *   stm32f4xx_it.c 的 USART1_IRQHandler 里调用同样的收帧逻辑。
 */
#include "dr16.h"
#include "main.h"
#include <string.h>

#define DBUS_FRAME_LEN     18u   /* DR16 一帧固定 18 字节 */
#define DBUS_CH_ERR_ABS    700   /* 有效摇杆值偏离中点 1024 的上限（校验帧用）*/

static UART_HandleTypeDef s_huart1;                 /* 只用来做初始化配置 */

static uint8_t  s_rx_buf[DBUS_FRAME_LEN];           /* 中断里逐字节收 */
static uint8_t  s_rx_idx;                           /* 当前收到第几字节 */
static uint8_t  s_frame[DBUS_FRAME_LEN];            /* 完整的一帧(给主循环解析) */
static volatile uint8_t  s_new_frame;               /* =1 表示有一帧待解析 */
static volatile uint32_t s_last_good_tick;          /* 最近一帧“有效”数据的 tick */

static dr16_rc_t s_rc;

/* 帧校验：4 个摇杆都应在 1024±700 内（收到错误字长/乱码时必超范围）*/
static uint8_t Dr16FrameValid(const dr16_rc_t *rc)
{
    if (rc == NULL) { return 0u; }
    if (rc->ch0 < DR16_CH_MID - DBUS_CH_ERR_ABS || rc->ch0 > DR16_CH_MID + DBUS_CH_ERR_ABS) { return 0u; }
    if (rc->ch1 < DR16_CH_MID - DBUS_CH_ERR_ABS || rc->ch1 > DR16_CH_MID + DBUS_CH_ERR_ABS) { return 0u; }
    if (rc->ch2 < DR16_CH_MID - DBUS_CH_ERR_ABS || rc->ch2 > DR16_CH_MID + DBUS_CH_ERR_ABS) { return 0u; }
    if (rc->ch3 < DR16_CH_MID - DBUS_CH_ERR_ABS || rc->ch3 > DR16_CH_MID + DBUS_CH_ERR_ABS) { return 0u; }
    return 1u;
}

void DR16_Init(void)
{
    GPIO_InitTypeDef g = {0};

    /* 1) 开时钟：USART1 在 APB2；RX 引脚 PB7 在 GPIOB */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 2) PB7 -> USART1_RX(AF7)。A 板 DBUS 座 -> 板上反相器 -> PB7，无需软件反转 */
    g.Pin       = GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &g);

    /* 3) USART1：100000 baud, 8E1 -> 9bit 字长 + 偶校验, 1 停止位 */
    s_huart1.Instance          = USART1;
    s_huart1.Init.BaudRate     = 100000u;
    s_huart1.Init.WordLength   = UART_WORDLENGTH_9B;   /* 8 数据位 + 校验位 = 9bit 帧 */
    s_huart1.Init.StopBits     = UART_STOPBITS_1;
    s_huart1.Init.Parity       = UART_PARITY_EVEN;
    s_huart1.Init.Mode         = UART_MODE_RX;         /* 只收不发 */
    s_huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_huart1) != HAL_OK) { Error_Handler(); }

    /* 4) 打开串口接收中断：RXNE(每字节) + IDLE(空闲=帧尾)，NVIC 用 USART1_IRQn */
    __HAL_UART_ENABLE_IT(&s_huart1, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&s_huart1, UART_IT_IDLE);
    HAL_NVIC_SetPriority(USART1_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    s_rx_idx = 0;
    s_new_frame = 0;
    s_last_good_tick = 0;
    memset(&s_rc, 0, sizeof(s_rc));
}

/*
 * USART1 收帧中断：
 *   RXNE：来一个字节收一个（最多 18）
 *   IDLE：总线空闲 -> 一帧收完 -> 置新帧标志
 */
void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if (sr & USART_SR_RXNE)
    {
        uint8_t b = (uint8_t)(USART1->DR & 0xFFu);
        if (s_rx_idx < DBUS_FRAME_LEN)
        {
            s_rx_buf[s_rx_idx++] = b;
        }
    }

    if (sr & USART_SR_IDLE)
    {
        (void)USART1->DR;                 /* 读 SR 后再读 DR 清除 IDLE 标志 */
        if (s_rx_idx == DBUS_FRAME_LEN)
        {
            memcpy(s_frame, s_rx_buf, DBUS_FRAME_LEN);
            s_new_frame = 1u;
        }
        s_rx_idx = 0;                     /* 下一帧从头收 */
    }
}

/* 主循环调用：有新帧才解析（DR16 官方 11bit 连续打包规则，与 ARBATOS 一致）*/
void DR16_Task(void)
{
    const uint8_t *b;
    if (!s_new_frame) { return; }
    s_new_frame = 0u;

    b = s_frame;
    /* ch0 = 右摇杆 X */
    s_rc.ch0 = (int16_t)((uint16_t)(b[0] | ((uint16_t)b[1] << 8)) & 0x07FFu);
    /* ch1 = 右摇杆 Y：上推≈1684(+660)，下推≈364(-660)，中 1024 */
    s_rc.ch1 = (int16_t)((uint16_t)(((uint16_t)b[1] >> 3) | ((uint16_t)b[2] << 5)) & 0x07FFu);
    /* ch2 = 左摇杆 X */
    s_rc.ch2 = (int16_t)((uint16_t)(((uint16_t)b[2] >> 6) | ((uint16_t)b[3] << 2) | ((uint16_t)b[4] << 10)) & 0x07FFu);
    /* ch3 = 左摇杆 Y */
    s_rc.ch3 = (int16_t)((uint16_t)(((uint16_t)b[4] >> 1) | ((uint16_t)b[5] << 7)) & 0x07FFu);
    /* 开关（2bit，未用，仅解析备用）*/
    s_rc.sw1 = (uint8_t)((b[5] >> 4) & 0x03u);
    s_rc.sw2 = (uint8_t)((b[5] >> 6) & 0x03u);

    /* 校验通过才更新时间戳（有效帧才解锁输出）*/
    if (Dr16FrameValid(&s_rc))
    {
        s_last_good_tick = HAL_GetTick();
    }
}

const dr16_rc_t *DR16_GetRC(void) { return &s_rc; }

uint8_t DR16_IsFresh(void)
{
    return (HAL_GetTick() - s_last_good_tick) < DR16_TIMEOUT_MS;
}

/* 摇杆 Y -> 开环电流：线性映射 ±660 -> ±16384，回中死区给 0 */
int32_t DR16_MapStickToCurrent(int16_t ch1)
{
    int32_t diff = (int32_t)ch1 - DR16_CH_MID;

    if (diff > -DR16_DEADZONE && diff < DR16_DEADZONE) { return 0; }  /* 回中附近=0 */

    /* 上推 diff≈+660 -> 正电流(正转)；下推 diff≈-660 -> 负电流(反转) */
    int32_t cur = diff * DR16_IQ_MAX / DR16_CH_RANGE;
    if (cur >  DR16_IQ_MAX) { cur =  DR16_IQ_MAX; }
    if (cur <  DR16_IQ_MIN) { cur =  DR16_IQ_MIN; }
    return cur;
}

/* 安全出口：没收到有效遥控帧(开机未连/信号丢失/解析异常) -> 0；否则摇杆映射电流 */
int32_t DR16_OutputCurrent(void)
{
    if (!DR16_IsFresh()) { return 0; }
    return DR16_MapStickToCurrent(s_rc.ch1);
}
