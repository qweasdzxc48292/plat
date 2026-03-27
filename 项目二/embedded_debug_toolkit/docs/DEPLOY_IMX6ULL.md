# IMX6ULL Deployment

## 1. Cross Compile

```bash
make \
  CC=arm-buildroot-linux-gnueabihf-gcc \
  CFLAGS="-O2 -Wall -Wextra -std=c11 -pthread -D_GNU_SOURCE -Iinclude"
```

## 2. Transfer Files

将以下文件拷贝到开发板：

- `embedded_debug_toolkit`
- `plugins/libsample_backend_plugin.so`
- `config/toolkit_config.json`

## 3. Runtime Check

```bash
chmod +x embedded_debug_toolkit
./embedded_debug_toolkit --iface eth0 --duration 0
```

如果需要抓包模块，建议 root 运行：

```bash
sudo ./embedded_debug_toolkit --iface eth0 --duration 0
```

## 4. PulseView

- 在 PC 端使用 PulseView 连接 `board-ip:9527`
- 可配置触发并进行 I2C/SPI/UART 解码
