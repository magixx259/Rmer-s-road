#ifndef DR16_H
#define DR16_H
/*
 * dr16.h - DR16(DBUS) 遥控器驱动头文件
 * 配合 bench-fw 使用：右摇杆 Y -> M2006/C610 开环电流
 */
#include <stdint.h>

/* ---- 摇杆通道参数（DR16 手册，11bit 解码后）---- */
#define DR16_CH_MID     1024   /* 摇杆中点 */
#define DR16_CH_MAX     1684   /* 推到一头(上/右) */
#define DR16_CH_MIN     364    /* 推到另一头(下/左) */
#define DR16_CH_RANGE   (DR16_CH_MAX - DR16_CH_MID)   /* 660 */

/* ---- C610 电流指令范围 ---- */
#define DR16_IQ_MAX     16384
#define DR16_IQ_MIN     (-16384)

/* ---- 回中死区：原始值离中点多少以内当作 0（防手抖）---- */
#define DR16_DEADZONE   15

/* ---- 遥控失控保护：超过该时间没收到新帧 -> 强制输出 0 ---- */
#define DR16_TIMEOUT_MS 100

/* 解码后的遥控数据结构（只用到 ch1，其余预留便于以后扩展） */
typedef struct {
    int16_t ch0;   /* 右摇杆 X（未用）*/
    int16_t ch1;   /* 右摇杆 Y：中1024 上1684 下364 —— 本工程用这个 */
    int16_t ch2;   /* 左摇杆 X（未用）*/
    int16_t ch3;   /* 左摇杆 Y（未用）*/
    uint8_t sw1;   /* 开关1（2bit 编码，未用）*/
    uint8_t sw2;   /* 开关2（2bit 编码，未用）*/
} dr16_rc_t;

/* 初始化 USART1/DBUS：100000 baud, 8E1, RX=PB7, 使能 RXNE+IDLE 中断 */
void DR16_Init(void);

/* 主循环轮询：有新帧才解析（无新帧立即返回）*/
void DR16_Task(void);

/* 取最近一次解析结果 */
const dr16_rc_t *DR16_GetRC(void);

/* 摇杆原始值 -> 开环电流指令（-16384 ~ +16384，带死区）*/
int32_t DR16_MapStickToCurrent(int16_t ch1);

/* 安全输出：遥控无新帧/超时 -> 0；否则摇杆映射电流 */
int32_t DR16_OutputCurrent(void);

/* 最近 DR16_TIMEOUT_MS 内是否收到过遥控帧 */
uint8_t DR16_IsFresh(void);

#endif

