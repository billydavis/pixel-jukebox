// Unit tests for lib/jukebox_core, the code with no hardware dependencies.
// They run on the board (pio test) and would run unchanged on a PC.

#include <unity.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "gif_pad.h"
#include "html_escape.h"
#include "rotation_rules.h"
#include "spectrum_math.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

void setUp() {}
void tearDown() {}

// --- gif_pad ---------------------------------------------------------------

// A 1x1 GIF: 13-byte header, a 2-colour table, one image, trailer.
static const uint8_t TINY_GIF[] = {
    'G',  'I',  'F',  '8',  '9',  'a',  0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, // header
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,                                           // colour table
    0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,                   // image descriptor
    0x02, 0x02, 0x44, 0x01, 0x00,                                                 // pixel data
    0x3B,                                                                         // trailer
};
static const size_t TINY_HEADER = 13 + 6;

static void test_gif_pad_grows_a_small_gif() {
  uint8_t buffer[sizeof(TINY_GIF) + GIF_PAD_BYTES];
  memcpy(buffer, TINY_GIF, sizeof(TINY_GIF));

  size_t size = gifPadSmall(buffer, sizeof(TINY_GIF));

  TEST_ASSERT_EQUAL_UINT(sizeof(TINY_GIF) + GIF_PAD_BYTES, size);
  TEST_ASSERT_TRUE(size >= GIF_SMALL_BYTES);
}

static void test_gif_pad_keeps_the_header_and_the_image() {
  uint8_t buffer[sizeof(TINY_GIF) + GIF_PAD_BYTES];
  memcpy(buffer, TINY_GIF, sizeof(TINY_GIF));

  gifPadSmall(buffer, sizeof(TINY_GIF));

  TEST_ASSERT_EQUAL_UINT8_ARRAY(TINY_GIF, buffer, TINY_HEADER);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(TINY_GIF + TINY_HEADER, buffer + TINY_HEADER + GIF_PAD_BYTES,
                                sizeof(TINY_GIF) - TINY_HEADER);
}

static void test_gif_pad_inserts_a_wellformed_comment_block() {
  uint8_t buffer[sizeof(TINY_GIF) + GIF_PAD_BYTES];
  memcpy(buffer, TINY_GIF, sizeof(TINY_GIF));

  gifPadSmall(buffer, sizeof(TINY_GIF));

  // Walk the block the way a decoder does: introducer, label, sub-blocks, terminator.
  const uint8_t *p = buffer + TINY_HEADER;
  TEST_ASSERT_EQUAL_HEX8(0x21, p[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFE, p[1]);
  size_t offset = 2;
  int blocks = 0;
  while (p[offset] != 0) {
    offset += 1 + p[offset];
    blocks++;
    TEST_ASSERT_TRUE(offset < GIF_PAD_BYTES);
  }
  TEST_ASSERT_EQUAL_INT(GIF_PAD_BLOCKS, blocks);
  TEST_ASSERT_EQUAL_UINT(GIF_PAD_BYTES, offset + 1);
  TEST_ASSERT_EQUAL_HEX8(0x2C, p[offset + 1]); // the image follows straight after
}

static void test_gif_pad_handles_no_colour_table() {
  uint8_t gif[40 + GIF_PAD_BYTES] = {'G', 'I', 'F', '8', '7', 'a', 1, 0, 1, 0, 0x00, 0, 0, 0x2C};
  size_t size = gifPadSmall(gif, 40);

  TEST_ASSERT_EQUAL_UINT(40 + GIF_PAD_BYTES, size);
  TEST_ASSERT_EQUAL_HEX8(0x21, gif[13]);
  TEST_ASSERT_EQUAL_HEX8(0x2C, gif[13 + GIF_PAD_BYTES]);
}

static void test_gif_pad_leaves_other_data_alone() {
  uint8_t notGif[64 + GIF_PAD_BYTES] = {'<', 'h', 't', 'm', 'l', '>'};
  TEST_ASSERT_EQUAL_UINT(64, gifPadSmall(notGif, 64));
  TEST_ASSERT_EQUAL_HEX8('<', notGif[0]);

  uint8_t tooShort[12 + GIF_PAD_BYTES] = {'G', 'I', 'F', '8', '9', 'a'};
  TEST_ASSERT_EQUAL_UINT(12, gifPadSmall(tooShort, 12));

  // Claims a 256-colour table (768 bytes) that the file is too short to hold.
  uint8_t truncated[100 + GIF_PAD_BYTES] = {'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0x87};
  TEST_ASSERT_EQUAL_UINT(100, gifPadSmall(truncated, 100));
}

static void test_gif_pad_leaves_large_gifs_alone() {
  static uint8_t large[GIF_SMALL_BYTES + GIF_PAD_BYTES] = {'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0x80};
  large[19] = 0x2C;

  TEST_ASSERT_EQUAL_UINT(GIF_SMALL_BYTES, gifPadSmall(large, GIF_SMALL_BYTES));
  TEST_ASSERT_EQUAL_HEX8(0x2C, large[19]);
}

// --- html_escape -----------------------------------------------------------

static void test_html_escape_plain_text_is_unchanged() {
  TEST_ASSERT_EQUAL_STRING("Home Network 2.4", htmlEscape("Home Network 2.4").c_str());
  TEST_ASSERT_EQUAL_STRING("", htmlEscape("").c_str());
  TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9", htmlEscape("Caf\xC3\xA9").c_str()); // UTF-8 passes through
}

static void test_html_escape_each_special_character() {
  TEST_ASSERT_EQUAL_STRING("&amp;", htmlEscape("&").c_str());
  TEST_ASSERT_EQUAL_STRING("&lt;", htmlEscape("<").c_str());
  TEST_ASSERT_EQUAL_STRING("&gt;", htmlEscape(">").c_str());
  TEST_ASSERT_EQUAL_STRING("&quot;", htmlEscape("\"").c_str());
  TEST_ASSERT_EQUAL_STRING("&#39;", htmlEscape("'").c_str());
}

static void test_html_escape_defuses_markup_in_a_network_name() {
  std::string escaped = htmlEscape("<script>alert('x')</script>");
  TEST_ASSERT_EQUAL_STRING("&lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt;", escaped.c_str());

  // A name that tries to break out of a quoted attribute such as data-ssid="...".
  escaped = htmlEscape("\" onclick=\"steal()");
  TEST_ASSERT_TRUE(escaped.find('"') == std::string::npos);
  TEST_ASSERT_TRUE(escaped.find('<') == std::string::npos);
}

static void test_html_escape_does_not_double_escape_by_accident() {
  TEST_ASSERT_EQUAL_STRING("Tom &amp; Jerry", htmlEscape("Tom & Jerry").c_str());
  TEST_ASSERT_EQUAL_STRING("&amp;amp;", htmlEscape("&amp;").c_str()); // literal text stays literal
}

// --- rotation_rules --------------------------------------------------------

static const std::vector<std::string> KNOWN = {"makapix", "clock"};
static const size_t MAX_ENTRIES = 8;
static const int MAX_TURNS = 20;

static bool valid(const std::vector<RotationItem> &items, std::string *error = nullptr) {
  std::string problem;
  bool ok = rotationValid(items, KNOWN, MAX_ENTRIES, MAX_TURNS, problem);
  if (error) {
    *error = problem;
  }
  return ok;
}

static void test_rotation_accepts_the_default() {
  TEST_ASSERT_TRUE(valid({{"makapix", 3}, {"clock", 1}}));
}

static void test_rotation_accepts_repeats_and_the_limits() {
  TEST_ASSERT_TRUE(valid({{"makapix", 1}, {"clock", 1}, {"makapix", 1}, {"clock", 1}}));
  TEST_ASSERT_TRUE(valid({{"clock", MAX_TURNS}}));
  TEST_ASSERT_TRUE(valid(std::vector<RotationItem>(MAX_ENTRIES, {"makapix", 1})));
}

static void test_rotation_rejects_empty_and_oversized() {
  std::string error;
  TEST_ASSERT_FALSE(valid({}, &error));
  TEST_ASSERT_EQUAL_STRING("rotation must have 1 to 8 entries", error.c_str());
  TEST_ASSERT_FALSE(valid(std::vector<RotationItem>(MAX_ENTRIES + 1, {"makapix", 1})));
}

static void test_rotation_rejects_unknown_slots() {
  std::string error;
  TEST_ASSERT_FALSE(valid({{"makapix", 1}, {"fire", 1}}, &error));
  TEST_ASSERT_EQUAL_STRING("unknown slot: fire", error.c_str());
  TEST_ASSERT_FALSE(valid({{"", 1}}));
  TEST_ASSERT_FALSE(valid({{"Makapix", 1}})); // names are case-sensitive
}

static void test_rotation_rejects_turns_out_of_range() {
  std::string error;
  TEST_ASSERT_FALSE(valid({{"clock", 0}}, &error));
  TEST_ASSERT_EQUAL_STRING("turns must be 1 to 20", error.c_str());
  TEST_ASSERT_FALSE(valid({{"clock", -1}}));
  TEST_ASSERT_FALSE(valid({{"clock", MAX_TURNS + 1}}));
}

// --- spectrum_math ---------------------------------------------------------

static const float SAMPLE_RATE = 16000.0f;
static float samples[spectrum::FFT_SIZE];

static void fillSine(float hz, float amplitude) {
  for (int i = 0; i < spectrum::FFT_SIZE; i++) {
    samples[i] = amplitude * sinf(6.28318530718f * hz * i / SAMPLE_RATE);
  }
}

static int strongestBand() {
  int best = 0;
  for (int band = 1; band < spectrum::BANDS; band++) {
    if (spectrum::bandMagnitude(band) > spectrum::bandMagnitude(best)) {
      best = band;
    }
  }
  return best;
}

static int bandContaining(float hz) {
  for (int band = 0; band < spectrum::BANDS; band++) {
    if (hz >= spectrum::bandEdgeHz(band) && hz < spectrum::bandEdgeHz(band + 1)) {
      return band;
    }
  }
  return -1;
}

static void test_spectrum_band_edges_span_the_range_in_order() {
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, spectrum::bandEdgeHz(0));
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 7000.0f, spectrum::bandEdgeHz(spectrum::BANDS));
  for (int band = 0; band < spectrum::BANDS; band++) {
    TEST_ASSERT_TRUE(spectrum::bandEdgeHz(band + 1) > spectrum::bandEdgeHz(band));
  }
  // Evenly spaced by pitch: every band covers the same ratio.
  float ratio = spectrum::bandEdgeHz(1) / spectrum::bandEdgeHz(0);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, ratio, spectrum::bandEdgeHz(20) / spectrum::bandEdgeHz(19));
}

static void test_spectrum_a_tone_lands_in_its_band() {
  const float tones[] = {100.0f, 440.0f, 1000.0f, 3000.0f, 6000.0f};
  for (float hz : tones) {
    fillSine(hz, 1000.0f);
    spectrum::analyse(samples);
    TEST_ASSERT_EQUAL_INT(bandContaining(hz), strongestBand());
  }
}

static void test_spectrum_reports_the_tones_amplitude() {
  fillSine(1000.0f, 1000.0f);
  spectrum::analyse(samples);
  TEST_ASSERT_FLOAT_WITHIN(150.0f, 1000.0f, spectrum::bandMagnitude(strongestBand()));

  fillSine(1000.0f, 250.0f);
  spectrum::analyse(samples);
  TEST_ASSERT_FLOAT_WITHIN(40.0f, 250.0f, spectrum::bandMagnitude(strongestBand()));
}

static void test_spectrum_silence_is_silent() {
  memset(samples, 0, sizeof(samples));
  spectrum::analyse(samples);
  for (int band = 0; band < spectrum::BANDS; band++) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, spectrum::bandMagnitude(band));
  }
}

static void test_spectrum_a_tone_stays_out_of_distant_bands() {
  fillSine(1000.0f, 1000.0f);
  spectrum::analyse(samples);
  TEST_ASSERT_TRUE(spectrum::bandMagnitude(bandContaining(100.0f)) < 10.0f);
  TEST_ASSERT_TRUE(spectrum::bandMagnitude(bandContaining(6000.0f)) < 10.0f);
}

static void test_spectrum_scale_maps_and_clamps() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, spectrum::scale(60.0f, 60.0f, 24.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, spectrum::scale(48.0f, 60.0f, 24.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, spectrum::scale(36.0f, 60.0f, 24.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, spectrum::scale(-100.0f, 60.0f, 24.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, spectrum::scale(90.0f, 60.0f, 24.0f));
}

static void test_spectrum_decibels() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, spectrum::toDb(1000.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, spectrum::toDb(100.0f));
  TEST_ASSERT_TRUE(std::isfinite(spectrum::toDb(0.0f))); // silence must not give -infinity
}

// ---------------------------------------------------------------------------

static int runTests() {
  UNITY_BEGIN();
  RUN_TEST(test_gif_pad_grows_a_small_gif);
  RUN_TEST(test_gif_pad_keeps_the_header_and_the_image);
  RUN_TEST(test_gif_pad_inserts_a_wellformed_comment_block);
  RUN_TEST(test_gif_pad_handles_no_colour_table);
  RUN_TEST(test_gif_pad_leaves_other_data_alone);
  RUN_TEST(test_gif_pad_leaves_large_gifs_alone);

  RUN_TEST(test_html_escape_plain_text_is_unchanged);
  RUN_TEST(test_html_escape_each_special_character);
  RUN_TEST(test_html_escape_defuses_markup_in_a_network_name);
  RUN_TEST(test_html_escape_does_not_double_escape_by_accident);

  RUN_TEST(test_rotation_accepts_the_default);
  RUN_TEST(test_rotation_accepts_repeats_and_the_limits);
  RUN_TEST(test_rotation_rejects_empty_and_oversized);
  RUN_TEST(test_rotation_rejects_unknown_slots);
  RUN_TEST(test_rotation_rejects_turns_out_of_range);

  spectrum::begin(SAMPLE_RATE, 60.0f, 7000.0f);
  RUN_TEST(test_spectrum_band_edges_span_the_range_in_order);
  RUN_TEST(test_spectrum_a_tone_lands_in_its_band);
  RUN_TEST(test_spectrum_reports_the_tones_amplitude);
  RUN_TEST(test_spectrum_silence_is_silent);
  RUN_TEST(test_spectrum_a_tone_stays_out_of_distant_bands);
  RUN_TEST(test_spectrum_scale_maps_and_clamps);
  RUN_TEST(test_spectrum_decibels);
  return UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000); // give the test runner time to open the serial port
  runTests();
}
void loop() {}
#else
int main() { return runTests(); }
#endif
