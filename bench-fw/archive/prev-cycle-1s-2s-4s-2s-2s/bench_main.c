/*
 * bench_main.c v4 - ALL-ID scan + record feedback ID
 * Sends the same current command to motor slots 1..8 (0x200 + 0x1FF),
 * and records the StdId of received feedback so we can learn the real CAN ID.
 */
#include "main.h"
#include "can.h"
#include <string.h>
#include <stdint.h>

#define BENCH_MOTOR_RAW        1500
#define BENCH_RAMP_UP_MS       2000u
#define BENCH_HOLD_MS          4000u
#define BENCH_RAMP_DOWN_MS     2000u
#define BENCH_START_DELAY_MS   1000u
#define BENCH_OFF_MS           2000u
#define BENCH_PERIOD_MS        (BENCH_START_DELAY_MS + BENCH_RAMP_UP_MS + BENCH_HOLD_MS + BENCH_RAMP_DOWN_MS + BENCH_OFF_MS)
#define BENCH_LED_TOGGLE_MS    250u

static CAN_TxHeaderTypeDef s_tx200;
static CAN_TxHeaderTypeDef s_tx1ff;
static uint8_t s_tx_data[8];

static volatile uint32_t s_rx_count;
static volatile uint32_t s_rx_id;      /* StdId of last feedback frame */

void SysTick_Handler(void) { HAL_IncTick(); }
void CAN1_TX_IRQHandler(void)  { HAL_CAN_IRQHandler(&hcan1); }
void CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan1); }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx;
    uint8_t d[8];
    if (hcan != &hcan1) return;
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, d) == HAL_OK)
    {
        s_rx_count++;
        s_rx_id = rx.StdId;
    }
}

static void CanFilterAcceptAll(void)
{
    CAN_FilterTypeDef f;
    f.FilterIdHigh=0; f.FilterIdLow=0; f.FilterMaskIdHigh=0; f.FilterMaskIdLow=0;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterBank = 0; f.FilterMode = CAN_FILTERMODE_IDMASK;
    f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterActivation = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan1, &f);
}

/* current big-endian in all 4 slots of one frame */
static void FillData(int32_t raw)
{
    uint8_t hi = (uint8_t)((raw >> 8) & 0xFFu);
    uint8_t lo = (uint8_t)(raw & 0xFFu);
    for (int i = 0; i < 4; i++)
    {
        s_tx_data[i*2]   = hi;
        s_tx_data[i*2+1] = lo;
    }
}

static void CanSendAllSlots(int32_t raw)
{
    uint32_t mb;
    FillData(raw);
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) >= 2)
    {
        (void)HAL_CAN_AddTxMessage(&hcan1, &s_tx200, s_tx_data, &mb);
        (void)HAL_CAN_AddTxMessage(&hcan1, &s_tx1ff, s_tx_data, &mb);
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
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void) { __disable_irq(); while (1u) { } }
void RobotFaultEarlyInit(void) { }
void RobotFaultDefaultHandler(void) { __disable_irq(); for (;;) { } }

int main(void)
{
    uint32_t start, last_led = 0;
    HAL_Init();
    SystemClock_Config();
    LedInit();
    MX_CAN1_Init();
    CanFilterAcceptAll();
    if (HAL_CAN_Start(&hcan1) != HAL_OK) { Error_Handler(); }
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) { Error_Handler(); }

    memset(s_tx_data, 0, sizeof(s_tx_data));
    s_tx200.IDE = CAN_ID_STD;  s_tx200.RTR = CAN_RTR_DATA; s_tx200.StdId = 0x200u; s_tx200.DLC = 8u; s_tx200.TransmitGlobalTime = DISABLE;
    s_tx1ff.IDE = CAN_ID_STD;  s_tx1ff.RTR = CAN_RTR_DATA; s_tx1ff.StdId = 0x1FFu; s_tx1ff.DLC = 8u; s_tx1ff.TransmitGlobalTime = DISABLE;

    start = HAL_GetTick();
    while (1u)
    {
        uint32_t now = HAL_GetTick();
        uint32_t t = (now - start) % BENCH_PERIOD_MS;
        int32_t cur = 0;

        if (t < BENCH_START_DELAY_MS) { cur = 0; }
        else if (t < BENCH_START_DELAY_MS + BENCH_RAMP_UP_MS)
        {
            uint32_t dt = t - BENCH_START_DELAY_MS;
            cur = (int32_t)((uint32_t)BENCH_MOTOR_RAW * dt / BENCH_RAMP_UP_MS);
        }
        else if (t < BENCH_START_DELAY_MS + BENCH_RAMP_UP_MS + BENCH_HOLD_MS) { cur = BENCH_MOTOR_RAW; }
        else if (t < BENCH_START_DELAY_MS + BENCH_RAMP_UP_MS + BENCH_HOLD_MS + BENCH_RAMP_DOWN_MS)
        {
            uint32_t dt = t - BENCH_START_DELAY_MS - BENCH_RAMP_UP_MS - BENCH_HOLD_MS;
            cur = (int32_t)((uint32_t)BENCH_MOTOR_RAW * (BENCH_RAMP_DOWN_MS - dt) / BENCH_RAMP_DOWN_MS);
        }
        else { cur = 0; }

        CanSendAllSlots(cur);

        if (now - last_led >= BENCH_LED_TOGGLE_MS) { last_led = now; HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_11); }
        HAL_Delay(1u);
    }
}


