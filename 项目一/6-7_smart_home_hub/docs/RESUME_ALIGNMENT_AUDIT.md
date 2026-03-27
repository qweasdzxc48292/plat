# 简历要求对齐审计（代码级）

> 审计范围：`项目一/6-7_smart_home_hub`
> 审计结论：**代码实现层面已基本对齐简历描述**；“30fps无丢帧”与“微信小程序端联调”需上板实测确认。

## 1. MQTT 心跳 + 断线重连 + 云端远程控制

- `src/mqtt/mqtt_service.c`
  - `keepAliveInterval`：第 199 行
  - 断线重连退避：第 249 行、第 298 行
  - MQTT 回调：第 125 行
  - QoS 区分：第 105 行、第 147 行、第 214-215 行
- `config/hub_config.json`
  - `mqtt_keepalive_sec: 20`
  - `mqtt_reconnect_sec: 5`
  - `mqtt_qos_report/control/response`

## 2. V4L2 采集-显示双线程 + 4 缓冲 + mmap + 查表转换

- `src/video/video_pipeline.c`
  - 4 缓冲：第 22 行
  - mmap：第 260 行、第 270 行、第 347 行
  - 双线程：第 326 行（capture）、第 399 行（render）
  - 查表转换：第 78 行（LUT）、第 89 行（YUYV->RGB565）

## 3. 固定大小内存池 + 引用计数

- `include/memory_pool.h`
  - 原子引用计数 `atomic_int refs`：第 19 行
- `src/core/memory_pool.c`
  - 初始化/预分配：第 13 行
  - 获取：第 78 行
  - 释放：第 128 行
- `config/hub_config.json`
  - `pool_block_count=4`
  - `pool_block_size=4147200`（1080p 级别）

## 4. MQTT 回调中解析 JSON-RPC，避免多端口服务

- `src/mqtt/mqtt_service.c`
  - 回调 `paho_msgarrvd`：第 125 行
- `src/main.c`
  - MQTT 消息统一分发到 `hub_json_rpc_dispatch`：第 33 行、第 41 行
- `src/rpc/json_rpc_dispatch.c`
  - `led_control/control/dht11_read` 方法分发：第 184 行、第 197 行、第 211 行

## 5. LED 字符设备驱动（注册/节点/安全检查/互斥）

- `drivers/led_char_safe/hub_led_drv.c`
  - 互斥锁：第 25 行
  - `copy_from_user/copy_to_user`：第 58/90/101 行
  - `register_chrdev`：第 132 行
  - `class_create`：第 139 行
  - `device_create`：第 147 行

## 6. 统一设备抽象接口 + 跨设备兼容

- `include/device_hal.h`
  - `hub_device_open/read/write/ioctl`：第 34/37/38/39 行
- `src/device/device_hal.c`
  - 抽象实现：第 104 行起
  - 节点回退兼容（如 `/dev/hub_led` 与 `/dev/100ask_led`）：第 40-41 行
- `tools/hal_demo/hal_demo.c`
  - 上层统一接口示例（无需关心底层细节）

## 7. 设备控制 + 视频监控 + 媒体播放一体化

- 设备控制：`src/rpc/json_rpc_dispatch.c` + `src/device/device_hal.c`
- 视频监控：`src/video/video_pipeline.c`
- 媒体播放：`src/media/media_player.c`（`aplay`）+ RPC `media_play/media_stop`

## 8. 本地 Qt GUI + 远程控制双端

- 本地 GUI：`qt_gui/qt59_control_panel/*`
- Qt 内远程 MQTT 控制按钮：`mainwindow.cpp`（`sendMqttLedControl`）
- 远程协议入口：`/iot/down` 及 `control` 方法（与小程序下发模型兼容）

## 9. 性能与稳定性证据位

- `src/video/video_pipeline.c` 输出 `/tmp/hub_video_stats.json`
  - 指标：`fps/captured/drop_pool/drop_queue`
  - 用于上板验证“30fps与丢帧优化结果”

## 仍需实机验证项（非代码缺失）

1. IMX6ULL 实机 640x480@30fps 连续运行测试（观察 `drop_pool/drop_queue`）
2. 与真实微信小程序/云平台 Topic 与鉴权参数联调
3. Qt 与 Hub 同机部署的依赖可用性（`aplay`、`mosquitto_pub`）

