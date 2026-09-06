# bench_main.c 逐行讲解（共 155 行，用于学习）

> 配套：D:\bench-fw\bench_main.c（当前 1.5A / 停1s-升2s-转4s-降2s-停2s 循环）
> 它干的事一句话：让 A 板每 1ms 通过 CAN1 给 C610（ID=1）发一次电流，按时间曲线"缓加速→转→缓停→停"循环，同时 LED 心跳、接收反馈计数。

## 第 1~5 行：文件头注释
```
1  /*
2   * bench_main.c v4 - ALL-ID scan + record feedback ID
3   * Sends the same current command to motor slots 1..8 (0x200 + 0x1FF),
4   * and records the StdId of received feedback so we can learn the real CAN ID.
5   */
```
- `/* ... */` 是注释，编译器忽略，写给人类看。
- 内容说明：这版为了排查，把电流同时发给 1~8 号电机槽位，并记录反馈 ID（后来确认你的电机是 ID=1）。

## 第 6~9 行：包含头文件
```
6  #include "main.h"
7  #include "can.h"
8  #include <string.h>
9  #include <stdint.h>
```
- `#include "can.h"`：把 `can.c` 里声明的 `hcan1`、`MX_CAN1_Init()` 等"告诉"本文件。`can.c` 是 CubeMX 生成的 CAN 初始化（PD0/PD1、1Mbps）。
- `<string.h>`：用 `memset`。`<stdint.h>`：用 `uint8_t/int32_t` 等明确宽度类型。

## 第 11~18 行：可调参数（改这里 = 改行为！）
```
11  #define BENCH_MOTOR_RAW        1500   // 当前值：满量程10000≈10A => 1500≈1.5A
12  #define BENCH_RAMP_UP_MS       2000u
13  #define BENCH_HOLD_MS          4000u
14  #define BENCH_RAMP_DOWN_MS     2000u
15  #define BENCH_START_DELAY_MS   1000u
16  #define BENCH_OFF_MS           2000u
17  #define BENCH_PERIOD_MS        (START+RAMP+HOLD+DOWN+OFF)  // 自动求和=11s
18  #define BENCH_LED_TOGGLE_MS    250u
```
- `#define 名字 值`：宏，编译时把名字替换成值。加 `u` 表示 unsigned（无符号）。
- `BENCH_MOTOR_RAW 1500` = 要发的电流原始值（≈1.5A）。**电流越大转越快**。
- 时间宏 = 转动曲线：停1s→缓升2s→满转4s→缓降2s→停2s。

## 第 20~25 行：全局变量
```
20  static CAN_TxHeaderTypeDef s_tx200;
21  static CAN_TxHeaderTypeDef s_tx1ff;
22  static uint8_t s_tx_data[8];
24  static volatile uint32_t s_rx_count;
25  static volatile uint32_t s_rx_id;
```
- `static`：只在本文件可见、一直存在。
- `CAN_TxHeaderTypeDef`：CAN 帧的"头"（ID、长度等），`s_tx_data[8]` 是 8 字节数据。
- `volatile`：告诉编译器"这变量会被中断改，别优化缓存"（中断里 `s_rx_count++`）。

## 第 27~29 行：中断处理函数（"硬件到点自动调用的函数"）
```
27  void SysTick_Handler(void) { HAL_IncTick(); }
28  void CAN1_TX_IRQHandler(void)  { HAL_CAN_IRQHandler(&hcan1); }
29  void CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan1); }
```
- `SysTick_Handler`：每 1ms 硬件中断一次，让 HAL 的毫秒计数 +1 → `HAL_GetTick()/HAL_Delay()` 才能用。
- CAN 收发中断：把事件交给 HAL 库统一处理。

## 第 31~41 行：收到 CAN 帧时的回调
```
31  void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
```
- 只要 CAN1 收到一帧，HAL 就会自动调用这个函数。
- 第 35 行：只处理 CAN1；第 36 行 `HAL_CAN_GetRxMessage` 把帧读出来；
- 第 38~39 行：`s_rx_count++`（收到几帧）、记下 `s_rx_id = rx.StdId`（哪条 ID 回的，用于确认电机 ID）。

## 第 43~53 行：CAN 过滤器=全部放行
- `FilterId/Mask 全 0` = 不过滤，任何 ID 都收（方便看到 0x201 反馈确认 ID=1）。正常工程应只放行需要的 ID。

## 第 55~76 行：★组帧并发送（电机控制核心）
```
56  static void FillData(int32_t raw)     // raw=电流原始值
58      uint8_t hi = (uint8_t)((raw >> 8) & 0xFFu);  // 高 8 位
59      uint8_t lo = (uint8_t)(raw & 0xFFu);          // 低 8 位
60      for (int i = 0; i < 4; i++)       // 4 个槽位
62          s_tx_data[i*2]   = hi;        // 槽 i 高字节放前
63          s_tx_data[i*2+1] = lo;        // 槽 i 低字节放后
```
- `>>8 & 0xFF`：取 16 位数的"高字节"；`& 0xFF` 取"低字节"。**大端 = 高字节在前**，这是当时转不起来的大坑。
- `for 4 次`：同一电流填满 4 个电机槽位（排查用；现在已知 ID=1 可只填第 0 槽）。

```
67  static void CanSendAllSlots(int32_t raw)
71      if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) >= 2)  // 有空邮箱才发
73          HAL_CAN_AddTxMessage(&hcan1, &s_tx200, ...);   // 发 0x200（电机1-4）
74          HAL_CAN_AddTxMessage(&hcan1, &s_tx1ff, ...);   // 发 0x1FF（电机5-8）
```
- RM 协议：`0x200` 帧管电机 1~4，`0x1FF` 管 5~8；每 2 字节一个电机电流。

## 第 78~85 行：LED 初始化（PE11）
- `LedInit()`：把 GPIOE 的 PIN11 设成推挽输出。后面用它闪烁表示"程序活着"。

## 第 87~108 行：系统时钟 168MHz
- HSE=12MHz 晶振 → PLL(M6 N168 P2) → SYSCLK=168MHz → APB1=42MHz。
- CAN 用 APB1：Prescaler=3、BS1=10、BS2=3 → `42MHz/(3×14)=1MHz`，正好是 C610 的 CAN 波特率。
- 出错就调 `Error_Handler()`（第 110 行：关中断死循环，方便调试器看出错）。

## 第 110~112 行：错误处理 + ARBATOS 启动占位
- `RobotFaultEarlyInit / RobotFaultDefaultHandler`：我们用的启动文件（ARBATOS 生成）会调用这俩名字，给空实现让它能链接。

## 第 114~155 行：main 主程序
```
116  uint32_t start, last_led = 0;
117  HAL_Init();          // 初始化 HAL
118  SystemClock_Config();// 168MHz
119  LedInit();           // PE11
120  MX_CAN1_Init();      // CAN1 1Mbps (can.c)
121  CanFilterAcceptAll();// 收所有帧
122-123 HAL_CAN_Start + 开接收中断通知
125-127 配置两帧头：StdId=0x200/0x1FF, 标准帧, 8字节
129  start = HAL_GetTick();   // 记启动时刻
130  while (1u) {              // 死循环=程序永远在这跑
132    uint32_t now = HAL_GetTick();   // 当前毫秒
133    uint32_t t = (now - start) % BENCH_PERIOD_MS;  // 周期内时间(0~11s循环)
134    int32_t cur = 0;
136    if (t < BENCH_START_DELAY_MS) { cur = 0; }                 // 停 1s
137-141 else if t 在[1,3):  cur = 3000*dt/2000 → 线性缓升          // 缓升 2s
142    else if t 在[3,7):  cur = BENCH_MOTOR_RAW(1500)            // 满转 4s
143-147 else if t 在[7,9): cur = 1500*(2000-dt)/2000 → 线性缓降    // 缓降 2s
148    else { cur = 0; }                                          // 停 2s
150    CanSendAllSlots(cur);   // 每 1ms 发一次当前电流
152    if (now-last_led >= 250) { 翻转 PE11 }                    // LED 心跳
153    HAL_Delay(1u);          // 等 1ms → 下一圈
  }
```
- `t = (now-start) % 11s`：把时间"折叠"进一个周期 → 循环自动重复。
- 曲线算法：`cur = 目标值 * 已过时间 / 总时间` → 从 0 线性爬到目标（这就是"缓升"）。

## 验收小测（对着文档能答=过关）
1. 3000 为什么≈3A？   → 满量程 10000 ≈ 10A
2. `data[0]=(raw>>8)` 在干嘛？ → 取高字节（大端）
3. `% BENCH_PERIOD_MS` 什么作用？ → 循环重复
4. 为什么要每 1ms 发一次？ → C610 靠连续指令帧维持输出
5. `volatile` 用在哪、为什么？ → 中断会改的变量，防编译器优化
