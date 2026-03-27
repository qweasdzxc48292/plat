# Qt 5.9 本地控制面板使用说明

目录：`qt_gui/qt59_control_panel`

## 功能

- 本地 LED 开/关
- 远程 MQTT LED 控制（通过 `mosquitto_pub` 下发到 Hub）
- 本地 DHT11 温湿度读取
- 本地媒体播放/停止（`aplay`）
- 视频状态显示（读取 `/tmp/hub_video_stats.json`）

## 编译

```bash
cd qt_gui/qt59_control_panel
qmake qt59_control_panel.pro
make -j4
```

## 运行

```bash
./qt59_control_panel
```

## 配置项

- `LED dev`：默认 `/dev/hub_led`
- `DHT11 dev`：默认 `/dev/mydht11`
- `Video stats`：默认 `/tmp/hub_video_stats.json`
- `Media file`：默认 `/root/media/test.wav`
- `MQTT host/port/down topic`：用于发送远程控制消息

## 依赖

- `aplay`（媒体播放）
- `mosquitto_pub`（远程 MQTT 控制按钮）
