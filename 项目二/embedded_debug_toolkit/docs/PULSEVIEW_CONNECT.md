# PulseView Connection Guide (SUMP)

## Backend

1. 在开发板编译并启动：

```bash
make
./embedded_debug_toolkit --iface eth0 --duration 0
```

2. 查看监听端口（默认 9527）：

```bash
netstat -anp | grep 9527
```

## PulseView Steps

1. 打开 PulseView
2. 添加设备，选择 SUMP/OLS 兼容驱动
3. 连接目标地址：`<board-ip>:9527`
4. 配置采样参数：
   - Channels: 8
   - Sample rate: <= 100MHz
   - Trigger: edge/level
5. 点击采样，随后添加协议解码：
   - UART（ch0）
   - I2C（SCL ch1, SDA ch2）
   - SPI（CLK ch3, MOSI ch4, MISO ch5, CS ch6）

## Troubleshooting

- 无法连接：
  - 检查防火墙和端口开放
  - 确认后端未使用 `--no-sump`
- 抓不到包：
  - 确认触发条件过严（改成 level/high 先验证）
- 波形异常：
  - 降低采样率（例如 5MHz 先验证）
  - 确认地线共地
