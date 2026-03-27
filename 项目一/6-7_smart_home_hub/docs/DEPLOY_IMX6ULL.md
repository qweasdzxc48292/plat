# IMX6ULL 上板部署步骤

## 1. 编译用户态中枢程序

```bash
cd 6-7_smart_home_hub
make USE_PAHO=1 CC=arm-buildroot-linux-gnueabihf-gcc \
  PAHO_DIR=../6-4-3_MQTT设备程序编写/mqtt_device/paho.mqtt.c
```

## 2. 编译 LED 驱动

```bash
cd drivers/led_char_safe
make KDIR=/path/to/imx6ull_kernel_build
```

产物：`hub_led_drv.ko`

## 3. 拷贝到开发板

```bash
scp smart_home_hub root@<board_ip>:/root/hub/
scp config/hub_config.json root@<board_ip>:/root/hub/
scp scripts/run_hub.sh root@<board_ip>:/root/hub/
scp drivers/led_char_safe/hub_led_drv.ko root@<board_ip>:/root/hub/
```

## 4. 板端加载驱动并运行

```bash
cd /root/hub
insmod hub_led_drv.ko led_gpio=131
chmod +x run_hub.sh
./run_hub.sh ./hub_config.json
```

## 5. 快速验收

1. MQTT 下发：
```json
{"method":"control","params":{"LED1":1},"id":"wx-1"}
```
看 LED 是否点亮。

2. 读取温湿度：
```json
{"method":"dht11_read","params":[0],"id":"sensor-1"}
```
看 `mqtt_topic_resp` 是否返回数组结果。

3. 视频链路：
- 观察串口日志是否周期打印 `video fps=...`。
- 或读取 `/tmp/hub_video_stats.json`。

4. 媒体播放：
```json
{"method":"media_play","params":["/root/media/test.wav"],"id":"media-1"}
```
再发：
```json
{"method":"media_stop","params":[],"id":"media-2"}
```

5. HAL 统一接口示例：
```bash
cd tools/hal_demo
make
./hal_demo ../../config/hub_config.json led on
./hal_demo ../../config/hub_config.json dht
```

## 6. Qt 5.9 本地 GUI（可选）

```bash
cd qt_gui/qt59_control_panel
qmake qt59_control_panel.pro
make
./qt59_control_panel
```

或在工程根目录：

```bash
sh scripts/build_qt_gui.sh
```
