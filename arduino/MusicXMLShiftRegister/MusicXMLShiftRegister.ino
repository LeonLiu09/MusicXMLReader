#include <Arduino.h>
#include <avr/pgmspace.h>

#include "song_data.h"

// MusicXMLReader v2.2：7x6 点阵动态扫描程序。
// U1（靠近 Arduino）：QA -> 空弦阴极，QB-QG -> 第 1-6 品阴极。
// U2（级联的第二片）：QA-QF -> 第 6-1 弦阳极，现有接线无需调整。
const uint8_t DATA_PIN = 11;   // D11 -> U1 pin 14 (SER)
const uint8_t CLOCK_PIN = 13;  // D13 -> U1/U2 pin 11 (SRCLK)
const uint8_t LATCH_PIN = 10;  // D10 -> U1/U2 pin 12 (RCLK)

const uint8_t FRET_OUTPUT_MASK = 0x7F;    // U1 的空弦及第 1-6 品
const uint8_t STRING_OUTPUT_MASK = 0x3F;  // U2 的 QA-QF

// 每一品保持约 1ms；实际刷新速度还会受到 595 移位时间影响。
const uint16_t ROW_ON_TIME_US = 1000;
const uint16_t ROW_BLANK_TIME_US = 20;
const uint16_t SELF_TEST_LED_MS = 120;

void writeMatrixOutputs(uint8_t fretCathodes, uint8_t stringAnodes)
{
  // U1 的 QH、U2 的 QG/QH 没有连接，固定输出 LOW。
  fretCathodes &= FRET_OUTPUT_MASK;
  stringAnodes &= STRING_OUTPUT_MASK;

  // 乐谱数据的 bit 0-5 仍表示第 1-6 弦，只在输出时镜像低 6 位。
  // QA <-> QF、QB <-> QE、QC <-> QD；QG/QH 保持 LOW。
  const uint8_t mirroredStringAnodes = static_cast<uint8_t>(
      ((stringAnodes & 0x01u) << 5) |
      ((stringAnodes & 0x02u) << 3) |
      ((stringAnodes & 0x04u) << 1) |
      ((stringAnodes & 0x08u) >> 1) |
      ((stringAnodes & 0x10u) >> 3) |
      ((stringAnodes & 0x20u) >> 5));

  digitalWrite(LATCH_PIN, LOW);

  // 先送出的字节最终进入较远的 U2（6 根弦阳极）。
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, mirroredStringAnodes);

  // 后送出的字节留在靠近 Arduino 的 U1（7 个品位阴极）。
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, fretCathodes);
  digitalWrite(LATCH_PIN, HIGH);
}

void blankMatrix()
{
  // 阴极全部 HIGH、阳极全部 LOW，所以所有 LED 都熄灭。
  writeMatrixOutputs(FRET_OUTPUT_MASK, 0x00);
}

void showPatternForDuration(
    const uint8_t stringMasksByFret[MATRIX_FRET_COUNT],
    uint32_t durationMs)
{
  const uint32_t startMs = millis();

  while (static_cast<uint32_t>(millis() - startMs) < durationMs)
  {
    for (uint8_t fretIndex = 0;
         fretIndex < MATRIX_FRET_COUNT;
         ++fretIndex)
    {
      if (static_cast<uint32_t>(millis() - startMs) >= durationMs)
        break;

      // 切换品位前先熄灭，避免上一品的阳极数据造成短暂串光。
      blankMatrix();
      delayMicroseconds(ROW_BLANK_TIME_US);

      const uint8_t stringMask =
          stringMasksByFret[fretIndex] & STRING_OUTPUT_MASK;
      if (stringMask != 0)
      {
        // 只把当前空弦/品位行的阴极拉 LOW，其余行保持 HIGH。
        const uint8_t fretCathodes = static_cast<uint8_t>(
            FRET_OUTPUT_MASK & ~(1u << fretIndex));
        writeMatrixOutputs(fretCathodes, stringMask);
      }

      delayMicroseconds(ROW_ON_TIME_US);
    }
  }

  blankMatrix();
}

void runSelfTest()
{
  // 依次测试空弦、第 1-6 品 x 6 弦，共 42 个小灯。
  for (uint8_t fretIndex = 0;
       fretIndex < MATRIX_FRET_COUNT;
       ++fretIndex)
  {
    for (uint8_t stringIndex = 0;
         stringIndex < MATRIX_STRING_COUNT;
         ++stringIndex)
    {
      uint8_t testPattern[MATRIX_FRET_COUNT] = {};
      testPattern[fretIndex] = static_cast<uint8_t>(1u << stringIndex);
      showPatternForDuration(testPattern, SELF_TEST_LED_MS);
    }
  }

  blankMatrix();
  delay(500);
}

void playSongOnce()
{
  for (uint16_t index = 0; index < SONG_EVENT_COUNT; ++index)
  {
    SongEvent event;
    memcpy_P(&event, &SONG_EVENTS[index], sizeof(SongEvent));
    showPatternForDuration(event.stringMasksByFret, event.durationMs);
  }
  blankMatrix();
}

void setup()
{
  // 先准备好 LOW 电平，再把三根控制线设为输出，减少上电闪烁。
  digitalWrite(DATA_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  blankMatrix();
  runSelfTest();
  playSongOnce();
}

void loop()
{
  // 播放结束后一直保持全部关闭。
  blankMatrix();
  delay(1000);
}
