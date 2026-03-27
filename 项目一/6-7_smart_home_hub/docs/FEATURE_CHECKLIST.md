# 项目要求对照清单（对应简历描述）

## 1) MQTT 心跳 + 自动重连 + 云平台远程控制

- 实现文件：`src/mqtt/mqtt_service.c`
- 对应点：
  - `keepAliveInterval = 20s`（默认，可配置）
  - `mqtt_reconnect_sec = 5s` 起步，断线后指数退避重连
  - QoS 区分：传感器上报 `qos_report=0`，控制下发 `qos_control=1`，响应 `qos_response=1`
  - 订阅下行 Topic，执行远程控制

## 2) V4L2 采集显示双线程 + 4缓冲区 + mmap + 查找表转换

- 实现文件：`src/video/video_pipeline.c`
- 对应点：
  - `capture_thread` / `render_thread` 双线程
  - `HUB_V4L2_BUF_COUNT = 4`
  - `V4L2_MEMORY_MMAP`
  - `init_yuv_lut` + `yuyv_to_rgb565` 查表转换
  - 输出 `/tmp/hub_video_stats.json`（fps/captured/drop_pool/drop_queue）用于量化验证

## 3) 固定块内存池 + 引用计数，降低碎片

- 实现文件：`src/core/memory_pool.c`
- 对应点：
  - 预分配固定大小块
  - `atomic_int refs` 管理生命周期
  - 多线程条件变量等待可用块
  - 默认参数按简历要求：`4` 个 `1080p` 级别内存块（`4147200` bytes/block）

## 4) MQTT 回调中解析 JSON-RPC，避免额外端口服务

- 实现文件：`src/mqtt/mqtt_service.c`, `src/rpc/json_rpc_dispatch.c`
- 对应点：
  - MQTT 回调 `paho_msgarrvd`
  - 直接调用 `hub_json_rpc_dispatch`
  - 响应回推 `mqtt_topic_resp`

## 5) LED 字符设备驱动：注册/节点创建/安全检查/并发互斥

- 实现文件：`drivers/led_char_safe/hub_led_drv.c`
- 对应点：
  - `register_chrdev`、`class_create`、`device_create`
  - `copy_from_user/copy_to_user` 返回值检查
  - `DEFINE_MUTEX(hub_led_lock)` 保护并发访问

## 6) 统一设备抽象接口：open/read/write/ioctl

- 实现文件：`include/device_hal.h`, `src/device/device_hal.c`, `tools/hal_demo/hal_demo.c`
- 对应点：
  - `hub_device_open/read/write/ioctl`
  - 上层统一用 `hub_led_set`、`hub_dht11_read`
  - 示例工具 `hal_demo` 直接调用统一 HAL，证明上层无需关心底层硬件差异
  - 屏蔽硬件节点差异

## 7) 本地 Qt 5.9 GUI + 微信小程序双端控制

- 实现文件：`qt_gui/qt59_control_panel/*`、`src/mqtt/mqtt_service.c`
- 对应点：
  - 本地 Qt 控制面板：LED 控制、温湿度读取、媒体播放、视频状态显示
  - Qt 控制面板内置 MQTT 远程控制按钮（通过 `mosquitto_pub` 下发到 `/iot/down`）
  - 微信小程序/云平台：MQTT 下发 JSON 指令控制同一设备
  - 构成“本地 GUI + 远程小程序”双端控制闭环

## 8) 媒体播放功能

- 实现文件：`src/media/media_player.c`、`src/rpc/json_rpc_dispatch.c`
- 对应点：
  - JSON-RPC 新增 `media_play` / `media_stop`
  - 通过 `aplay` 播放本地音频文件
