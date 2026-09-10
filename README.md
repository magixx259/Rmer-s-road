# M2006 Bench Firmware

用于 RoboMaster 开发板 A 型（STM32F427）与 C610 + M2006 的最小测试固件。仓库包含两个已验证版本：无需遥控的自动循环测试，以及 DR16 遥控开环电流控制。

> 本项目是台架测试程序，不是整车工程。电机上电前务必固定机构、保证人手和线缆远离转动部件。

## 硬件配置

- 主控：RoboMaster 开发板 A 型（STM32F427）
- 电机与电调：DJI M2006 + C610
- CAN：CAN1，A 板 `PD0 = CAN_RX`、`PD1 = CAN_TX`
- 已验证电机 ID：`1`
- 电流指令：标准帧 `0x200` 的第 1 路（字节 0–1）
- 电机反馈：标准帧 `0x201`
- 遥控版 DBUS：USART1，A 板 `PB7 = RX`、`PB6 = TX`

## 固件版本

| 文件 | 行为 | 是否需要遥控 |
| --- | --- | --- |
| `bench1/bench1.hex` | 停 1 秒 → 缓升 2 秒 → 转 4 秒 → 缓降 2 秒 → 停 2 秒，循环执行 | 否 |
| `bench2/bench2.hex` | 右摇杆 Y 轴控制 M2006 开环电流 | 是 |

`bench2/m2006-bench-record.md` 记录了已验证的电机 ID、CAN 接口和测试结果。

根目录的 `bench_main.c` 与 `dr16.c/.h` 对应遥控控制版本的源码；`build/bench.hex` 是该版本的已编译固件。

## 遥控版使用方式

1. 将 DR16 接收机接到 A 板的 DBUS 专用接口，确认 DT7 与 DR16 已对频（DR16 绿灯常亮）。
2. 上推右摇杆：正电流 / 正转。
3. 下推右摇杆：负电流 / 反转。
4. 摇杆回中，或遥控信号超过 100 ms 未收到：输出电流为 0。

代码中的“正转”是电流方向定义；电机实际顺时针或逆时针取决于安装方向。

## 构建

该工程依赖本机的 ARM GNU Toolchain、CMake、Ninja，以及 `D:/ARBATOS/projects/INFANTRY-A` 的 STM32 HAL 头文件和源文件。

```bat
cd /d D:\bench-fw
build.cmd
```

构建成功后生成：

```text
build/bench.elf
build/bench.bin
build/bench.hex
```

## 烧录

需要 ST-Link 通过 SWD 连接 A 板。

```bat
cd /d D:\bench-fw
flash.cmd
```

`flash.cmd` 使用 OpenOCD 烧录 `build/bench.hex`，并执行写后校验和复位。若要烧录归档版本，可将对应 `.hex` 文件传给 `flash_any.cmd`。

```bat
flash_any.cmd D:\bench-fw\bench1\bench1.hex
flash_any.cmd D:\bench-fw\bench2\bench2.hex
```

## 安全事项

- 接线、插拔 CAN、电调或电机前先断电。
- 首次测试使用较低电流，并固定电机或将底盘架空。
- 确保 CAN 总线共地、终端电阻配置正确，且电机 ID 不冲突。
- 遥控链路未建立时，不要通过修改失联保护来强行输出电流。

## 下一步

完成单电机验证后，可继续做四轮电机 ID/转向记录、麦轮底盘逆运动学解算、速度闭环 PID，以及 MPU6500 姿态解算。
