# Architecture Notes

## Service Layer

`edt_debug_service` 统一管理全部模块生命周期：

1. `logic_analyzer` 采样线程启动
2. `sump_server` 对外提供 sigrok/PulseView 采样桥接
3. `net_capture` 与 `system_monitor` 后台线程启动
4. `plugin_manager` 按目录动态加载后端插件

## Logic Analyzer Data Path

```text
Reader(simulated/GPIO) -> trigger gate -> ring_buffer -> consumer(decode/sump)
```

- 数据格式：`edt_logic_sample_t`
- 每条样本：`timestamp_ns + 8-channel bitfield`
- ring buffer：单生产者采样线程 + 单消费者协议线程

## Qt Plugin Layer

`debug_workbench` 通过 `QPluginLoader` 加载插件，每个插件实现 `IDebugToolPlugin`：

- `pluginName()`
- `createWidget(parent)`
- `onMessage(heartbeat/status)`

当前插件示例：

- `logic_view_plugin`
- `system_monitor_plugin`
- `capture_tools_plugin`

## Deployment Suggestion

- 板端运行后端（采样 + SUMP + 调试模块）
- PC端运行 PulseView / Qt GUI
- 通过 TCP 协同查看信号与系统状态
