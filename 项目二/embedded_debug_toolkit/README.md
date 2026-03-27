# Embedded Debug Toolkit (Project 2)

在 Linux 环境下实现的嵌入式开发调试工具箱，基于逻辑分析仪开源方案扩展，提供硬件信号采集、sigrok/PulseView 对接、V4L2/抓包/系统监控、Qt 插件化界面。

## Resume Alignment

- 8 通道逻辑分析仪，最大采样率配置为 `100MHz`，采样数据进入 `SPSC 无锁环形缓冲区`，支持边沿/电平触发
- SUMP(sigrok 兼容)服务端，支持 PulseView 对接波形显示与协议分析
- 集成 V4L2 摄像头探测、网络协议抓包统计、系统资源监控，统一单调时钟时间戳
- Qt 5.9 统一界面，插件动态加载（逻辑分析/系统监控/采集工具插件示例）

## Key Architecture

```text
Logic Sampler (8ch, trigger, 100MHz cap)
        |
        +--> Lock-free RingBuffer (SPSC)
        |          |
        |          +--> Protocol Decoders (UART/I2C/SPI)
        |
        +--> SUMP Server (sigrok/PulseView bridge)

Toolkit Service
        +--> V4L2 Probe
        +--> Network Capture Stats
        +--> System Monitor (/proc)
        +--> Backend Plugin Manager (dlopen)

Qt Debug Workbench
        +--> QPluginLoader dynamic plugins
```

## Directory

```text
embedded_debug_toolkit
├── include
├── src
│   ├── core            # time + lock-free ring buffer
│   ├── logic           # 8ch sampler + trigger
│   ├── protocol        # UART/I2C/SPI + SUMP bridge
│   ├── tools           # v4l2 + net capture + system monitor
│   ├── plugins         # backend plugin manager
│   └── service         # unified debug service
├── plugins_src         # sample backend plugin source
├── plugins             # runtime backend plugins(.so)
├── qt_gui
│   ├── debug_workbench
│   └── plugins
├── tests
├── scripts
└── config
```

## Build Backend

```bash
make
```

Build sample backend plugin only:

```bash
make plugins
```

Run unit tests:

```bash
make test
```

## Run Backend Demo

```bash
./embedded_debug_toolkit --iface eth0 --duration 20
```

或：

```bash
sh scripts/run_demo.sh
```

Command options:

- `--sample-rate <Hz>`: 采样率（上限 100000000）
- `--no-sump`: 关闭 SUMP 服务
- `--no-netcap`: 关闭抓包模块
- `--iface <ifname>`: 网络接口
- `--duration <sec>`: 演示运行时长

## PulseView / sigrok Integration

1. 启动后端，确认监听 `tcp://<board-ip>:9527`
2. 打开 PulseView，选择 SUMP/OLS 兼容设备（TCP 方式）
3. 设置采样率、通道数（8ch）、触发模式（边沿/电平）
4. 采样后可直接在 PulseView 使用 I2C/SPI/UART 协议解码器分析

## Build Qt GUI

```bash
sh scripts/build_qt_gui.sh
```

构建后主程序与插件位于：

- `qt_gui/build/app/debug_workbench`
- `qt_gui/build/app/plugins/*.so`

## Notes

- 网络抓包使用 `AF_PACKET` 原始套接字，通常需要 root 权限
- V4L2 模块默认探测 `/dev/video0`
- 非 Linux 环境下，V4L2/抓包模块会返回 `-ENOSYS`
- `config/toolkit_config.json` 给出建议部署配置，可用于二次扩展配置加载器
