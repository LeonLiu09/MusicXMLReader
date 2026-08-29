# MusicXMLReader v2.0

MusicXMLReader 把 MusicXML 乐谱转换成 Arduino Uno R3 可播放的引脚事件。
v2.0 先支持吉他前两品的全部 6 根弦，并使用两片级联的 SN74HC595 扩展输出。

## v2.0 功能

- 读取第一声部的完整时间轴、音符、和弦、休止符和速度变化。
- 将第 1 品和第 2 品的 6 根弦映射成 12 路输出。
- 生成 Arduino 草图使用的 `song_data.h`。
- 提供 Arduino Uno R3 + 两片 SN74HC595 的测试草图。
- 上电时依次测试 12 路 LED，随后按乐谱播放一次。
- 使用 CTest 自动检查输出位、示例乐谱、速度变化和冲突品位。

## 输出与接线

- Uno D11：串行数据，连接 U1 的 SER（14 脚）。
- Uno D13：移位时钟，同时连接 U1、U2 的 SRCLK（11 脚）。
- Uno D10：锁存时钟，同时连接 U1、U2 的 RCLK（12 脚）。
- U1 的 QH'（9 脚）连接 U2 的 SER（14 脚），形成级联。
- U1 控制第 1 品，U2 控制第 2 品；每片 QA-QF 对应第 1-6 弦。

完整接线和安全提示见
[`arduino/MusicXMLShiftRegister/接线说明.md`](arduino/MusicXMLShiftRegister/接线说明.md)。

## 电脑端生成数据

在项目根目录配置、构建并运行：

```powershell
cmake -S . -B .cmake-build -G Ninja
cmake --build .cmake-build
.\.cmake-build\MusicXMLReader.exe
```

程序读取 `musicdoc/Canon in D major.musicxml`，并更新
`arduino/MusicXMLShiftRegister/song_data.h`。

## 自动测试

```powershell
ctest --test-dir .cmake-build --output-on-failure
```

## Arduino 测试

1. 按接线说明连接 Uno、两片 SN74HC595、12 个 LED 和各自的 680Ω 电阻。
2. 用 Arduino IDE 打开 `arduino/MusicXMLShiftRegister/MusicXMLShiftRegister.ino`。
3. 选择 `Arduino Uno` 和对应串口。
4. 先点击“验证”，再点击“上传”。
5. 观察 12 路 LED 依次点亮，然后按乐谱事件播放一次并全部熄灭。

> LED 仅用于模拟 MOSFET 开关状态。不要把线圈、电磁铁或其他大电流负载直接接到 SN74HC595。

## 当前限制

- 只转换第一个声部。
- 只映射第 1 品和第 2 品。
- 遇到 `<backup>` 多声线时间轴会停止并给出错误。
- 当前示例 MusicXML 路径和 Arduino 输出路径固定在程序中。

版本历史见 [`CHANGELOG.md`](CHANGELOG.md)。
