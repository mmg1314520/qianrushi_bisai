# 智慧农业 Qt 面板

这个目录下的 Qt 工程已经按 `spi_touch` 里的 LVGL 界面思路做了桌面版：

- 环境数据页：时间、定位、天气、温度、湿度、光照、土壤湿度、CO2、作物分析建议
- 设备控制页：水泵、风扇、蜂鸣器、天窗、诱虫灯、控制建议
- 支持自动/手动模式切换
- 支持串口接收下位机数据
- 支持向下位机发送控制命令
- 无设备时可开启模拟数据

## 目录

- `Main.qml`：主界面
- `appcontroller.*`：界面状态、串口通信、模拟数据
- `controlmodel.*`：设备控制模型

## 串口上行数据

Qt 端支持按行接收 JSON，每行一条。最简单的格式：

```json
{
  "temperature": 26,
  "humidity": 62,
  "light": 48,
  "soil": 35,
  "co2": 780,
  "location": "上海浦东温室 1 区",
  "weather": "多云",
  "auto": true,
  "controls": {
    "pump": 0,
    "fan": 1,
    "buzzer": 0,
    "window": 1,
    "pestLamp": 1
  },
  "cropAdvice": "土壤略干，建议补水。",
  "controlAdvice": "建议开启风扇并保持半开天窗。"
}
```

也支持这种带类型包装的格式：

```json
{
  "type": "telemetry",
  "data": {
    "temperature": 28,
    "humidity": 60,
    "light": 55,
    "soil": 41,
    "co2": 860
  }
}
```

## Qt 下发控制命令

Qt 端会按行发送 JSON：

```json
{"type":"mode","auto":false}
{"type":"control","device":"fan","state":1}
{"type":"control","device":"pump","state":0}
{"type":"control","device":"window","state":2}
```

字段说明：

- `device` 可取：`pump`、`fan`、`buzzer`、`window`、`pestLamp`
- `state`
- 二值设备：`0=关`，`1=开`
- `buzzer`：`0=关`，`1=蝗虫`，`2=菜青虫`
- `window`：`0=关闭`，`1=半开`，`2=全开`

## 已验证

- 项目已在当前机器完成 CMake 配置
- 已成功编译生成 `appone.exe`
- 已做过一次启动检查，程序可正常拉起
