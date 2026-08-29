#include <Arduino.h>
#include <avr/pgmspace.h>

#include "song_data.h"

// MusicXMLReader v2.0：Arduino Uno R3 双 SN74HC595 测试程序。
// Arduino Uno R3 到两片 SN74HC595 的三根控制线。
const uint8_t DATA_PIN = 11;   // D11 -> U1 pin 14 (SER)
const uint8_t CLOCK_PIN = 13;  // D13 -> U1/U2 pin 11 (SRCLK)
const uint8_t LATCH_PIN = 10;  // D10 -> U1/U2 pin 12 (RCLK)

const uint16_t USED_OUTPUT_MASK = 0x3F3F;
const uint16_t SELF_TEST_DELAY_MS = 300;

void writeOutputs(uint16_t mask)
{
  // 四个未使用输出始终保持 LOW。
  mask &= USED_OUTPUT_MASK;

  digitalWrite(LATCH_PIN, LOW);

  // 先送出的高字节会进入较远的 U2（第 2 品）。
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, highByte(mask));

  // 后送出的低字节留在靠近 Arduino 的 U1（第 1 品）。
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, lowByte(mask));
  digitalWrite(LATCH_PIN, HIGH);
}

void runSelfTest()
{
  for (uint8_t stringNumber = 1; stringNumber <= 6; ++stringNumber)
  {
    writeOutputs(static_cast<uint16_t>(1u << (stringNumber - 1)));
    delay(SELF_TEST_DELAY_MS);
  }

  for (uint8_t stringNumber = 1; stringNumber <= 6; ++stringNumber)
  {
    writeOutputs(static_cast<uint16_t>(1u << (8 + stringNumber - 1)));
    delay(SELF_TEST_DELAY_MS);
  }

  writeOutputs(0x0000);
  delay(500);
}

void playSongOnce()
{
  for (uint16_t index = 0; index < SONG_EVENT_COUNT; ++index)
  {
    SongEvent event;
    memcpy_P(&event, &SONG_EVENTS[index], sizeof(SongEvent));
    writeOutputs(event.mask);
    delay(event.durationMs);
  }
  writeOutputs(0x0000);
}

void setup()
{
  // 先把锁存脚置 LOW，再配置输出，减少上电时的意外闪烁。
  digitalWrite(LATCH_PIN, LOW);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  writeOutputs(0x0000);
  runSelfTest();
  playSongOnce();
}

void loop()
{
  // 按计划只播放一次。播放结束后一直保持全部关闭。
  writeOutputs(0x0000);
  delay(1000);
}
