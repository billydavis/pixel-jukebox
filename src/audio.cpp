#include "audio.h"

#include <Wire.h>
#include <driver/i2s.h>

#include "pins.h"

static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int SAMPLE_RATE = 16000;
static const uint8_t ES8311_ADDRESS = 0x18;
static const int16_t TONE_AMPLITUDE = 12000; // of 32767
static const int FADE_SAMPLES = SAMPLE_RATE / 100; // 10 ms, avoids clicks
static const int CHUNK_FRAMES = 256;

static bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDRESS, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

// Register sequence from Espressif's es8311 driver (esp-bsp), fixed to a
// 16 kHz, 16-bit I2S slave clocked from the MCLK pin at 256 x the sample rate.
static bool setupCodec() {
  if (!writeRegister(0x00, 0x1F)) { // reset
    return false;
  }
  delay(20);
  writeRegister(0x00, 0x00);
  writeRegister(0x00, 0x80); // power on

  writeRegister(0x01, 0x3F);                          // all clocks on, MCLK from pin
  writeRegister(0x06, readRegister(0x06) & ~0x20);    // BCLK not inverted
  writeRegister(0x02, readRegister(0x02) & 0x07);     // pre-divider 1, multiplier 1
  writeRegister(0x03, 0x10);                          // ADC oversampling
  writeRegister(0x04, 0x10);                          // DAC oversampling
  writeRegister(0x05, 0x00);                          // ADC and DAC dividers 1
  writeRegister(0x06, (readRegister(0x06) & 0xE0) | 0x03); // BCLK divider 4
  writeRegister(0x07, readRegister(0x07) & 0xC0);     // LRCK divider high byte
  writeRegister(0x08, 0xFF);                          // LRCK divider low byte

  writeRegister(0x00, readRegister(0x00) & 0xBF); // I2S slave
  writeRegister(0x09, 0x0C);                      // 16-bit in
  writeRegister(0x0A, 0x0C);                      // 16-bit out

  writeRegister(0x0D, 0x01); // analog power up
  writeRegister(0x0E, 0x02); // PGA and ADC modulator on
  writeRegister(0x12, 0x00); // DAC power up
  writeRegister(0x13, 0x10); // output drive on
  writeRegister(0x1C, 0x6A); // ADC equaliser bypass
  return writeRegister(0x37, 0x08); // DAC equaliser bypass
}

static bool setupI2s() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = 8;
  config.dma_buf_len = CHUNK_FRAMES;
  config.tx_desc_auto_clear = true; // silence instead of repeats on underrun
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(I2S_PORT, &config, 0, nullptr) != ESP_OK) {
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_MCLK_PIN;
  pins.bck_io_num = I2S_BCLK_PIN;
  pins.ws_io_num = I2S_WS_PIN;
  pins.data_out_num = I2S_DOUT_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  return i2s_set_pin(I2S_PORT, &pins) == ESP_OK;
}

bool audioBegin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  // The codec needs MCLK running before it is configured.
  if (!setupI2s() || !setupCodec()) {
    return false;
  }
  audioSetVolume(60);

  pinMode(SPEAKER_AMP_PIN, OUTPUT);
  digitalWrite(SPEAKER_AMP_PIN, HIGH);
  return true;
}

void audioSetVolume(int percent) {
  percent = constrain(percent, 0, 100);
  writeRegister(0x32, percent == 0 ? 0 : percent * 256 / 100 - 1);
}

void audioTone(float frequencyHz, uint32_t durationMs) {
  static int16_t frames[CHUNK_FRAMES * 2]; // stereo; the codec plays one channel
  const int total = SAMPLE_RATE * durationMs / 1000;
  const float step = 2.0f * PI * frequencyHz / SAMPLE_RATE;

  int sample = 0;
  while (sample < total) {
    int count = min(CHUNK_FRAMES, total - sample);
    for (int i = 0; i < count; i++, sample++) {
      float gain = 1.0f;
      if (sample < FADE_SAMPLES) {
        gain = (float)sample / FADE_SAMPLES;
      } else if (total - sample < FADE_SAMPLES) {
        gain = (float)(total - sample) / FADE_SAMPLES;
      }
      int16_t value = (int16_t)(sinf(step * sample) * TONE_AMPLITUDE * gain);
      frames[i * 2] = value;
      frames[i * 2 + 1] = value;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, frames, count * 2 * sizeof(int16_t), &written, portMAX_DELAY);
  }
}

String audioScanI2c() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  String found;
  for (uint8_t address = 0x08; address < 0x78; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      char text[6];
      snprintf(text, sizeof(text), "%02X ", address);
      found += text;
    }
  }
  found.trim();
  return found;
}
