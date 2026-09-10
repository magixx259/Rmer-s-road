/*
 * bench_main.c v5 - DR16 遥控 + M2006 开环电流控制
 *
 * 基于之前能正常转的 bench-fw 工程修改；旧版本已备份到 archive/
 *  - prev-root-v4-allid-scan/        之前根目录的 v4 全 ID 扫描版
 *  - prev-cycle-1s-2s-4s-2s-2s/      你最后用的“停1s/缓升2s/转4s/缓降2s/停2s”循环版
 *
 * 硬件：
 *   - A 板(STM32F427IIHx)
 *   - C610 电调 + M2006，电机 CAN ID = 1，挂在 CAN1 (PD0/PD1)
 *   - DR16 接收机(DT7 遥控)插 A 板 DBUS 口 -> USART1
 *
 * 控制方式（纯开环，无速度/位置 PID）：
 *   右摇杆 Y(ch1)：中 1024，上推 1684，下推 364
 *   -> 线性映射为 C610 电流指令 -16384 ~ +16384
 *   -> 摇杆回中(死区±15)=0；上推=正电流/正转；下推=负电流/反转
 *   -> 遥控开机未连/信号丢失>100ms：电流强制 0（失控保护）
 */
#include "main.h"
#include "can.h"
#include "dr16.h"
#include <string.h>
#include <stdint.h>

#define BENCH_LED_TOGGLE_MS   250u

static CAN_TxHeaderTypeDef s_tx200;
static uint8_t s_tx_data[8];
static volatile uint32_t s_rx_count;
static volatile uint32_t s_rx_id;

void SysTick_Handler(void) { HAL_IncTick(); }
void CAN1_TX_IRQHandler(void)  { HAL_CAN_IRQHandler(&hcan1); }
void CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan1); }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx;
    uint8_t d[8];
    if (hcan != &hcan1) { return; }
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, d) == HAL_OK)
    {
        s_rx_count++;
        s_rx_id = rx.StdId;
    }
}

static void CanFilterAcceptAll(void)
{
    CAN_FilterTypeDef f;
    f.FilterIdHigh = 0; f.FilterIdLow = 0;
    f.FilterMaskIdHigh = 0; f.FilterMaskIdLow = 0;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterBank = 0; f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterActivation = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan1, &f);
}

static void CanSendMotor1Current(int32_t raw)
{
    uint32_t mb;
    int16_t v = (int16_t)raw;

    s_tx_data[0] = (uint8_t)(((uint16_t)v) >> 8);
    s_tx_data[1] = (uint8_t)(v & 0xFFu);
    s_tx_data[2] = 0; s_tx_data[3] = 0;
    s_tx_data[4] = 0; s_tx_data[5] = 0;
    s_tx_data[6] = 0; s_tx_data[7] = 0;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) >= 1u)
    {
        (void)HAL_CAN_AddTxMessage(&hcan1, &s_tx200, s_tx_data, &mb);
    }
}

static void LedInit(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Pin = GPIO_PIN_11; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 6;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void) { __disable_irq(); while (1u) { } }
void RobotFaultEarlyInit(void) { }
void RobotFaultDefaultHandler(void) { __disable_irq(); for (;;) { } }

int main(void)
{
    uint32_t now, last_led = 0;
    HAL_Init();
    SystemClock_Config();
    LedInit();

    MX_CAN1_Init();
    CanFilterAcceptAll();
    if (HAL_CAN_Start(&hcan1) != HAL_OK) { Error_Handler(); }
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) { Error_Handler(); }

    DR16_Init();

    memset(s_tx_data, 0, sizeof(s_tx_data));
    s_tx200.IDE = CAN_ID_STD;
    s_tx200.RTR = CAN_RTR_DATA;
    s_tx200.StdId = 0x200u;
    s_tx200.DLC = 8u;
    s_tx200.TransmitGlobalTime = DISABLE;

    while (1u)
    {
        now = HAL_GetTick();
        DR16_Task();
        CanSendMotor1Current(DR16_OutputCurrent());
        if (now - last_led >= BENCH_LED_TOGGLE_MS)
        {
            last_led = now;
            HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_11);
        }
        HAL_Delay(1u);
    }
}
