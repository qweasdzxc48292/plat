# Feature Checklist

## 1. 8通道逻辑分析仪 + 100MHz + 无锁环形缓冲区 + 触发

- [x] 8通道样本位图（`uint8_t channels`）
- [x] 最大采样率限制 `100000000Hz`
- [x] SPSC 无锁环形缓冲区（atomic 读写指针）
- [x] 触发模式：
  - [x] 上升沿
  - [x] 下降沿
  - [x] 高电平
  - [x] 低电平

对应实现：

- `src/logic/logic_analyzer.c`
- `src/core/ring_buffer.c`

## 2. sigrok(PulseView)对接 + I2C/SPI/UART

- [x] SUMP 命令处理（ID、META、ARM、参数配置）
- [x] 采样数据流输出给上位机
- [x] UART 解码器
- [x] I2C 解码器
- [x] SPI 解码器

对应实现：

- `src/protocol/sump_server.c`
- `src/protocol/protocol_decoder.c`

## 3. 多功能调试工具集 + 时间戳

- [x] V4L2 摄像头设备探测与格式枚举
- [x] 网络抓包统计（ARP/IP/TCP/UDP/ICMP）
- [x] 系统资源监控（CPU/内存/网口）
- [x] 统一时间戳（CLOCK_MONOTONIC ns）

对应实现：

- `src/tools/v4l2_debugger.c`
- `src/tools/net_capture.c`
- `src/tools/system_monitor.c`
- `src/core/edt_time.c`

## 4. Qt统一调试界面 + 插件动态加载

- [x] Qt 主界面 `debug_workbench`
- [x] `QPluginLoader` 动态加载插件
- [x] 逻辑分析插件示例
- [x] 系统监控插件示例
- [x] 采集工具插件示例

对应实现：

- `qt_gui/debug_workbench/*`
- `qt_gui/plugins/*`
