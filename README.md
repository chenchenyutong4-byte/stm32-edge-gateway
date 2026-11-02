# STM32 边缘网关 · Modbus 采集 / MQTT 上云 / 离线缓存 / LVGL 监控界面

基于 **STM32F411CEU6 + FreeRTOS + LVGL 8.3** 的物联网边缘数据采集网关。

通过 RS-485 / Modbus-RTU 轮询温湿度与光照传感器，经 ESP8266 用 MQTT 上报 JSON 数据；
网络异常时把数据写入板载 SPI Flash 缓存，恢复后自动补传；本地 320×240 屏提供实时数据卡片
与系统运行日志页面。

---

## 功能特性

| 模块 | 说明 |
|---|---|
| Modbus-RTU 主站 | 轮询 2 台从机（`0x01` 温湿度、`0x02` 光照），800 ms 一轮，CRC16 查表法校验，连续 5 次失败剔除该设备 |
| MQTT 上报 | ESP8266 透传模式下手工组包（CONNECT / SUBSCRIBE / PUBLISH / PINGREQ），JSON 载荷 |
| 离线缓存 | 断网时数据写入 W25Q64 后 4 MB 环形区，每条 128 B；重连后每批补传 5 条，读完即标记，不重复发送 |
| 运行日志 | W25Q64 前 4 MB 分区环形写入，带时间戳、等级、魔数校验；LVGL 日志页从写指针倒序读取，新日志置顶 |
| 心跳自愈 | 每 10 s 发一次 PINGREQ，连续 3 次失败判定离线并自动重连 |
| LVGL UI | 温度 / 光照 / 湿度三卡片 + 连接状态 + 运行时间 + 日志页；10 s 无操作自动息屏，触摸或数据变化唤醒 |
| 看门狗 | IWDG 溢出 20 s，独立任务 3 s 喂狗 |

## 硬件平台

- **MCU**：STM32F411CEU6（UFQFPN48），SYSCLK **100 MHz**（HSE 25 MHz，PLL M=12 / N=96 / P=2 / Q=4）
- **屏幕**：320×240 SPI 液晶，SPI1 与触摸屏共用总线
- **存储**：W25Q64（8 MB，SPI2）
- **无线**：ESP8266（AT 指令 + 透传模式，USART1）
- **现场总线**：RS-485 半双工（USART2 + DE 控制）

### 引脚分配

| 外设 | 引脚 | 说明 |
|---|---|---|
| USART1 TX / RX | PA9 / PA10 | ESP8266，115200 |
| USART2 TX / RX | PA2 / PA3 | Modbus-RTU |
| RS485 DE | PB0 | 发送时拉高、接收时拉低 |
| SPI1 SCK / MISO / MOSI | PA5 / PA6 / PA7 | LCD + 触摸共用 |
| LCD CS / RS / RST | PB6 / PB7 / PA11 | — |
| LCD 背光 | PA8 | 息屏控制 |
| 触摸 CS | PB10 | — |
| SPI2 SCK / MISO / MOSI | PB13 / PB14 / PB15 | W25Q64 |
| Flash CS | PB12 | — |
| 状态灯 | PB3 / PB4 / PA15 | 在线 / 补传 / 离线指示 |

## 软件架构

### FreeRTOS 任务（`code/Taskhandel.c`）

| 任务 | 周期 | 职责 |
|---|---|---|
| `Modbus()` | 800 ms | `Modbus_Poll_AllDev()` 采集 → `Modbus_Upload_Json()` 写队列 |
| `MQTT()` | 队列驱动 | 在线：先补传缓存（≤5 条）再发实时数据；离线：写 Flash 缓存 |
| `AT()` | 10 s | 心跳 PINGREQ / 判断离线 / 调用 `RE_MQTT_Init()` 重连 |
| `UI()` | 5 ms | LVGL 刷新、数据与状态刷新、息屏管理、运行时间更新 |
| `Dog_task()` | 3 s | 喂独立看门狗 |

### 数据流

```
Modbus 从机 ──RS485──▶ Modbus()  ──▶ data_queue(长度1，覆盖式)
                                        │
                                        ├──▶ MQTT()  ──在线──▶ ESP8266 ──▶ MQTT Broker
                                        │            └──离线──▶ W25Q64 补传缓存区
                                        └──▶ UI()    ──▶ LVGL 卡片 / 状态栏
```

### Flash 分区（W25Q64，8 MB）

| 区域 | 地址范围 | 用途 |
|---|---|---|
| 日志区 | `0x000000` ~ `0x3FFFFF` | `Log_Obj` 环形写入（魔数 `0x55AA` 校验） |
| 补传缓存区 | `0x400000` ~ `0x7FFFFF` | `CacheEntry_t` 环形写入，128 B/条，4 KB 扇区擦除 |

## 目录结构

```
.
├── Core/                      # CubeMX 生成的初始化代码（main / 外设 Msp / 中断）
├── Drivers/                   # STM32F4 HAL 驱动 + CMSIS
├── Middlewares/Third_Party/   # FreeRTOS 10.3.1 + CMSIS-RTOS2 封装
├── code/                      # 板级驱动与应用任务
│   ├── lcd.c / lcd.h          # SPI 液晶驱动
│   ├── touch.c / touch.h      # 触摸屏读取与坐标映射
│   └── Taskhandel.c/.h        # Modbus / MQTT / AT / UI / Dog 五个任务
├── ESP8266/                   # ESP8266 通信层
│   ├── command.c/.h           # 环形缓冲区 + DMA 接收
│   ├── ESP_at.c/.h            # AT 指令收发与应答匹配
│   ├── pal.c/.h               # 联网、MQTT 报文组包、数据上报
│   └── libemqtt.c/.h          # 第三方 MQTT 参考实现（未参与编译）
├── model/                     # Modbus-RTU 主站协议实现（CRC16 / 03 功能码 / 轮询）
├── SPI_Flash/                 # W25Q64 底层读写 + 分区封装（日志 / 断网缓存）
├── Lvgl/
│   ├── lv_conf.h, lvgl.h      # LVGL 8.3.11 配置
│   ├── src/                   # LVGL 源码（未入库 demos 目录）
│   ├── examples/porting/      # lv_port_disp / lv_port_indev 显示与触摸移植
│   └── LVGL_myGUI/            # 业务界面：主监控页、日志页、中文小字库
├── MDK-ARM/                   # Keil 工程（F4project.uvprojx）与启动文件
├── F4project.ioc              # STM32CubeMX 工程配置
└── .mxproject
```

## 编译与烧录

1. 安装 **Keil MDK-ARM 5**，并装好 `Keil.STM32F4xx_DFP` 器件包（工程使用 **ARM Compiler 5.06 update 7**，即 AC5）。
2. 打开 `MDK-ARM/F4project.uvprojx`。
3. `Project → Build`（F7）编译，`Flash → Download`（F8）烧录。
4. 用 ST-Link 通过 SWD（PA13 / PA14）连接，Debug 配置里选择 ST-Link Debugger。

> 如果你要改外设配置，用 STM32CubeMX 打开 `F4project.ioc`；重新生成代码时注意保留 `Core/` 中
> `USER CODE BEGIN/END` 段内的手写代码。

### 首次运行必须修改的配置

联网信息目前是占位符，烧录前请改成你自己的：

```c
/* ESP8266/pal.h */
#define WIFI_SSID   "YOUR_WIFI_SSID"
#define WIFI_PASS   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER "mqtt.example.com"
#define MQTT_PORT   1883
#define MQTT_UID    "00000000000000000000000000000000"

/* ESP8266/pal.c：AT 指令中的字面量，需与上面保持一致 */
ESP_SendCmd("AT+CWJAP=\"YOUR_WIFI_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n", "WIFI GOT IP", 5000);
ESP_SendCmd("AT+CIPSTART=\"TCP\",\"mqtt.example.com\",1883\r\n", "CONNECT", 2000);
/* connect_packet[] 中的 32 字节 Client ID 同样替换成你的设备凭据 */
```

> MQTT 服务端需支持 MQTT 3.1（`MQIsdp`）；若用公共云平台（如巴法云、EMQX、私有 Mosquitto），
> 把 `AT+CIPSTART` 的主机/端口和 32 字节 Client ID 换成你的即可。

## 第三方组件与许可

| 组件 | 版本 | 许可 |
|---|---|---|
| STM32F4 HAL Driver | CubeMX 生成 | BSD-3-Clause |
| FreeRTOS | 10.3.1 | MIT |
| CMSIS-RTOS2 | — | Apache-2.0 |
| LVGL | 8.3.11 | MIT |
| libemqtt | — | LGPL（仅作参考实现保留，未参与编译） |
