# ZLab MQTT Message Guard

这是第四个个人化嵌入式项目：为 [OpenMQTTGateway](https://github.com/1technophile/OpenMQTTGateway) 增加一层轻量消息保护，在 BLE、433 MHz、LoRa、红外等数据进入 MQTT 队列前完成去重、按设备限流和异常设备临时隔离。

这个项目不是网页，也不依赖云服务。核心代码兼容 C++11，不使用动态容器，适合 ESP32 等资源受限设备；同时可直接在电脑上运行测试。

## 为什么做这个功能

真实网关经常同时接收大量无线广播。故障传感器、重复广播或恶意流量可能塞满队列，影响其他正常设备。原项目已有队列容量保护，本模块进一步在入队前区分设备并主动过滤异常流量。

```text
BLE / RF / LoRa / IR
        │
        ▼
  JSON 消息解码
        │
        ▼
┌─────────────────────────┐
│ ZLab Message Guard      │
│ 指纹去重 → 令牌桶限流     │
│          → 临时隔离      │
└─────────────────────────┘
        │ Allow
        ▼
 OpenMQTTGateway JSON Queue → MQTT
```

## 个人新增内容

- 16 个设备的固定容量状态表，内存占用可预测；
- FNV-1a 设备键和载荷指纹；
- 可配置的重复消息时间窗；
- 每设备令牌桶，避免单个设备独占队列；
- 连续超限后自动隔离，冷却期结束自动恢复；
- 最久未使用设备淘汰策略；
- 正确处理 Arduino `millis()` 约 49.7 天回绕；
- `allow / duplicate / rate_limited / quarantined / invalid` 五类决策；
- 接收、放行、去重、限流、隔离、无效和淘汰统计；
- 主机端单元测试和网关流量模拟；
- 独立库与 OpenMQTTGateway Fork 功能分支双重交付。

## 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/gateway_sim
```

模拟输出中，第一条数据放行，短时间内完全相同的第二条数据会被识别为重复，数值变化后的第三条数据继续放行。

## 默认策略

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| 重复窗口 | 3000 ms | 相同设备、相同载荷在窗口内只放行一次 |
| 补充速度 | 1 token/s | 按设备恢复发送额度 |
| 突发额度 | 5 tokens | 允许正常设备短时连续上报 |
| 隔离阈值 | 3 次 | 连续超限后进入隔离 |
| 隔离时间 | 30 s | 冷却后自动恢复 |

## 与上游项目的关系

`MessageGuard` 是独立编写的 MIT 许可模块。自动构建直接检出官方 OpenMQTTGateway `development`，再运行本仓库的集成脚本，不依赖个人 Fork。集成过程保留上游 GPL-3.0 许可和作者信息，没有把上游项目重新声称为个人原创。项目经历中可以准确描述为：

> 基于 OpenMQTTGateway 二次开发消息可靠性保护层，实现固定内存的按设备去重、令牌桶限流、异常隔离与自动化测试，并完成 ESP32 网关入队链路集成。

## 后续硬件验证

自动测试覆盖算法和边界条件。接入真实 ESP32 网关后，还应使用 BLE 广播器或 433 MHz 传感器进行高频流量压测，确认过滤统计和 MQTT 到达率符合预期。
