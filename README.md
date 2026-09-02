# MusicXMLReader v2.1

MusicXMLReader 把 MusicXML 乐谱转换成 Arduino Uno R3 可播放的引脚事件。
v2.1 支持吉他空弦及第 1～6 品的全部 6 根弦，并使用两片级联的 SN74HC595 动态扫描 7×6 点阵。

## v2.1 功能

- 读取第一声部的完整时间轴、音符、和弦、休止符和速度变化。
- 将空弦、第 1～6 品和第 1～6 弦映射成 42 个点阵位置。
- 生成 Arduino 草图使用的 `song_data.h`。
- 提供 Arduino Uno R3 + 两片 SN74HC595 的测试草图。
- 采用高速逐品动态扫描，而不是把 42 个灯静态直连到输出。
- 上电时依次测试 42 个 LED，随后按乐谱播放一次。
- 使用 CTest 自动检查输出位、示例乐谱、速度变化和冲突品位。
- 已在 Arduino Uno R3、两片 SN74HC595 和 7×6 点阵实物上确认运行正常。

## 输出与接线

- Uno D11：串行数据，连接 U1 的 SER（14 脚）。
- Uno D13：移位时钟，同时连接 U1、U2 的 SRCLK（11 脚）。
- Uno D10：锁存时钟，同时连接 U1、U2 的 RCLK（12 脚）。
- U1 的 QH'（9 脚）连接 U2 的 SER（14 脚），形成级联。
- U1 的 QA 连接空弦阴极，QB-QG 依次连接第 1～6 品阴极。
- U2 的 QA-QF 依次通过 680Ω 电阻连接第 1～6 弦阳极。

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

1. 按 [`Arduino UNO R3、两片 SN74HC595 与 7×6 点阵接线说明`](arduino/MusicXMLShiftRegister/接线说明.md) 连接 Uno、两片 SN74HC595、点阵和 6 个 680Ω 电阻。
2. 用 Arduino IDE 打开 `arduino/MusicXMLShiftRegister/MusicXMLShiftRegister.ino`。
3. 选择 `Arduino Uno` 和对应串口。
4. 先点击“验证”，再点击“上传”。
5. 观察 42 个 LED 依次点亮，然后按乐谱事件动态扫描播放一次并全部熄灭。

> LED 仅用于模拟 MOSFET 开关状态。不要把线圈、电磁铁或其他大电流负载直接接到 SN74HC595。

## 当前限制

- 只转换第一个声部。
- 只映射空弦及第 1～6 品；第 7 品以上暂不点亮。
- 遇到 `<backup>` 多声线时间轴会停止并给出错误。
- 当前示例 MusicXML 路径和 Arduino 输出路径固定在程序中。

版本历史见 [`CHANGELOG.md`](CHANGELOG.md)。
