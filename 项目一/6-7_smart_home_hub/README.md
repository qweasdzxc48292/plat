# 6-7 Smart Home Hub

基于 `嵌入式Linux应用开发` 的统一中枢工程，融合并重构了开源项目的关键能力：

- MQTT 远程控制（参考 `使用MQTT实现智能家居`）
- JSON-RPC 命令分发（改为 MQTT 回调内直调，去掉额外端口服务）
- V4L2 双线程视频流水线（参考 `嵌入式Linux相机`）
- 4 缓冲区轮转 + `mmap` 零拷贝采集
- 固定块内存池 + 引用计数（本工程新增）
- 统一设备抽象层 `open/read/write/ioctl`
- 本地 Qt 5.9 控制面板（LED/温湿度/媒体控制/视频状态）
- 本地 Qt 5.9 控制面板（本地控制 + MQTT 远程控制按钮）
- 媒体播放服务（`aplay`）

## 目录结构

```text
6-7_smart_home_hub
├── include
│   ├── hub_config.h
│   ├── device_hal.h
│   ├── memory_pool.h
│   ├── video_pipeline.h
│   ├── mqtt_service.h
│   └── json_rpc_dispatch.h
├── src
│   ├── main.c
│   ├── core
│   ├── device
│   ├── mqtt
│   ├── rpc
│   ├── utils
│   └── video
├── config
│   └── hub_config.json
├── scripts
│   └── run_hub.sh
├── qt_gui
│   └── qt59_control_panel
└── Makefile
```

## 核心架构

```text
MQTT Broker / 微信小程序
        |
   mqtt_service
        | (JSON-RPC request)
json_rpc_dispatch
        |
    device_hal (LED/DHT11/Camera unified interface)

V4L2 Camera --> capture thread --> memory_pool --> render thread --> frame callback
```

## 功能点对应关系

1. MQTT 心跳、断线重连
2. MQTT 回调里解析 JSON-RPC 并执行设备控制
3. V4L2 采集线程 + 渲染线程解耦
4. YUYV -> RGB565 查找表转换
5. 内存池固定块 + 引用计数管理
6. 统一硬件抽象：`hub_device_open/read/write/ioctl`
7. 本地 Qt 5.9 GUI + 微信小程序远程双端控制（本地/远程）

## 构建

默认不依赖 Paho（方便先本地编译框架）：

```bash
make
```

构建安全版 LED 字符设备驱动（内核模块）：

```bash
cd drivers/led_char_safe
make
```

启用 Paho MQTT（上板运行建议）：

```bash
make USE_PAHO=1 \
  CC=arm-buildroot-linux-gnueabihf-gcc \
  PAHO_DIR=../6-4-3_MQTT设备程序编写/mqtt_device/paho.mqtt.c
```

构建 Qt 5.9 GUI：

```bash
sh scripts/build_qt_gui.sh
```

构建 HAL 示例工具（演示上层通过统一接口操作硬件）：

```bash
cd tools/hal_demo
make
```

## 运行

```bash
./smart_home_hub config/hub_config.json
```

或：

```bash
sh scripts/run_hub.sh
```

一键演示（Hub + Qt GUI，若 GUI 已编译）：

```bash
sh scripts/start_full_demo.sh
```

## MQTT 控制消息示例

1. 兼容 JSON-RPC 风格：

```json
{"method":"led_control","params":[1],"id":"2"}
```

2. 兼容微信属性下发风格：

```json
{"method":"control","params":{"LED1":0},"id":"wx-1"}
```

3. 读取温湿度：

```json
{"method":"dht11_read","params":[0],"id":"3"}
```

4. 播放媒体：

```json
{"method":"media_play","params":["/root/media/test.wav"],"id":"4"}
```

5. 停止媒体：

```json
{"method":"media_stop","params":[],"id":"5"}
```

响应默认发布到 `mqtt_topic_resp`。

## 注意事项

- 设备节点默认：
  - LED: `/dev/hub_led`
  - DHT11: `/dev/mydht11`
  - Camera: `/dev/video0`
- 视频状态文件默认：`/tmp/hub_video_stats.json`（供 Qt GUI 显示）
- 若驱动节点名称不同，请修改 `config/hub_config.json`。
- 未接入真实摄像头时，视频模块会自动降级为合成帧输出，不阻塞主流程。

## 文档

- 功能对照：`docs/FEATURE_CHECKLIST.md`
- 上板步骤：`docs/DEPLOY_IMX6ULL.md`
- Qt GUI 使用：`docs/QT_GUI_USAGE.md`
- HAL 示例：`tools/hal_demo/hal_demo.c`
