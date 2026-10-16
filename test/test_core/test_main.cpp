// Unit tests for lib/jukebox_core, the code with no hardware dependencies.
// They run on the board (pio test) and would run unchanged on a PC.

#include <unity.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "cache_index.h"
#include "gif_pad.h"
#include "html_escape.h"
#include "likes_book.h"
#include "rotation_rules.h"
#include "spectrum_math.h"
#include "tap_detector.h"
#include "transitions.h"
#include "view_queue.h"

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

// --- transitions -----------------------------------------------------------

static uint16_t imageA[transitions::PIXELS];
static uint16_t imageB[transitions::PIXELS];
static uint16_t rendered[transitions::PIXELS];
static uint16_t renderedAgain[transitions::PIXELS];
static const uint32_t SEEDS[] = {0, 1, 2, 3, 12345, 0xFFFFFFFFu};

// Two different images in which every pixel is distinct, so a pixel in the
// wrong place shows up as a mismatch.
static void fillImages() {
  for (int i = 0; i < transitions::PIXELS; i++) {
    imageA[i] = (uint16_t)(i * 7 + 1);
    imageB[i] = (uint16_t)(0xFFFF - i * 5);
  }
}

static void test_transitions_list_is_complete_and_findable() {
  TEST_ASSERT_EQUAL_INT(13, transitions::count());
  for (int i = 0; i < transitions::count(); i++) {
    TEST_ASSERT_EQUAL_INT(i, transitions::find(transitions::info(i).name)); // names are unique
    TEST_ASSERT_TRUE(strlen(transitions::info(i).label) > 0);
  }
  TEST_ASSERT_EQUAL_INT(-1, transitions::find("nope"));
  TEST_ASSERT_EQUAL_INT(-1, transitions::find("random")); // a setting, not a transition
}

static void test_transitions_start_on_the_old_image() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    for (uint32_t seed : SEEDS) {
      transitions::render(i, 0.0f, seed, imageA, imageB, rendered);
      TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(imageA, rendered, transitions::PIXELS, transitions::info(i).name);
    }
  }
}

static void test_transitions_end_on_the_new_image() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    for (uint32_t seed : SEEDS) {
      transitions::render(i, 1.0f, seed, imageA, imageB, rendered);
      TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(imageB, rendered, transitions::PIXELS, transitions::info(i).name);
    }
  }
}

static void test_transitions_change_something_part_way() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    transitions::render(i, 0.5f, 7, imageA, imageB, rendered);
    TEST_ASSERT_TRUE_MESSAGE(memcmp(rendered, imageA, sizeof(rendered)) != 0, transitions::info(i).name);
  }
}

static void test_transitions_repeat_exactly_for_one_seed() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    transitions::render(i, 0.37f, 99, imageA, imageB, rendered);
    transitions::render(i, 0.37f, 99, imageA, imageB, renderedAgain);
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(rendered, renderedAgain, transitions::PIXELS, transitions::info(i).name);
  }
}

static void test_transitions_tolerate_progress_out_of_range() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    transitions::render(i, -0.5f, 3, imageA, imageB, rendered);
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(imageA, rendered, transitions::PIXELS, transitions::info(i).name);
    transitions::render(i, 1.5f, 3, imageA, imageB, rendered);
    TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(imageB, rendered, transitions::PIXELS, transitions::info(i).name);
  }
}

// Sweeps every transition through its whole range. Nothing to assert beyond
// not crashing, but run under a memory checker this would catch an
// out-of-range read.
static void test_transitions_survive_a_full_sweep() {
  fillImages();
  for (int i = 0; i < transitions::count(); i++) {
    for (int step = 0; step <= 100; step++) {
      transitions::render(i, step / 100.0f, SEEDS[step % 6], imageA, imageB, rendered);
    }
  }
  TEST_ASSERT_TRUE(true);
}

// A wipe only ever shows pixels in their own place, from one image or the other.
static void test_transitions_wipe_only_moves_the_edge() {
  fillImages();
  int index = transitions::find("wipe");
  for (uint32_t seed : SEEDS) {
    transitions::render(index, 0.25f, seed, imageA, imageB, rendered);
    int fromNew = 0;
    for (int i = 0; i < transitions::PIXELS; i++) {
      TEST_ASSERT_TRUE(rendered[i] == imageA[i] || rendered[i] == imageB[i]);
      fromNew += rendered[i] == imageB[i];
    }
    TEST_ASSERT_EQUAL_INT(transitions::PIXELS / 4, fromNew); // a quarter revealed
  }
}

// --- cache_index -----------------------------------------------------------

static cache::Entry cacheStorage[8];

static void test_cache_records_survive_a_round_trip() {
  cache::Index index(cacheStorage, 8);
  cache::Entry entry = cache::Index::make(4242, 7, 30000, 1234, "Odio");
  std::string line = cache::Index::addLine(entry);
  TEST_ASSERT_EQUAL_STRING("A 4242 7 30000 1234 Odio\n", line.c_str());

  TEST_ASSERT_TRUE(index.apply(line.c_str()));
  const cache::Entry *found = index.find(4242);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_UINT32(7, found->seq);
  TEST_ASSERT_EQUAL_UINT32(30000, found->dwellMs);
  TEST_ASSERT_EQUAL_UINT32(1234, found->bytes);
  TEST_ASSERT_EQUAL_STRING("Odio", found->owner);
}

static void test_cache_replay_keeps_the_latest_and_honours_removals() {
  cache::Index index(cacheStorage, 8);
  const char *log[] = {"A 1 1 30000 100 alpha\n", "A 2 2 30000 200 beta\n", "A 1 3 30000 100 alpha\n",
                       "R 2\n", "A 3 4 15000 300 gamma\n"};
  for (const char *line : log) {
    TEST_ASSERT_TRUE(index.apply(line));
  }
  TEST_ASSERT_EQUAL_INT(2, index.count());
  TEST_ASSERT_EQUAL_UINT32(3, index.find(1)->seq); // the later record wins
  TEST_ASSERT_NULL(index.find(2));
  TEST_ASSERT_EQUAL_UINT64(400, index.totalBytes());
  TEST_ASSERT_EQUAL_UINT32(5, index.nextSeq());
}

static void test_cache_ignores_damaged_lines() {
  cache::Index index(cacheStorage, 8);
  const char *bad[] = {"", "\n", "X 1 2 3 4 who\n", "A\n", "A 1 2 3\n", "A 1 2 3 4\n", // cut short
                       "A 1 2 3 4 \n", "A x 2 3 4 who\n", "A 1 2 3 4z who\n", "A 0 2 3 4 who\n",
                       "A 99999999999 2 3 4 who\n", "R\n", "R abc\n", "A -1 2 3 4 who\n"};
  for (const char *line : bad) {
    TEST_ASSERT_FALSE_MESSAGE(index.apply(line), line);
  }
  TEST_ASSERT_EQUAL_INT(0, index.count());
}

static void test_cache_oldest_is_the_lowest_sequence() {
  cache::Index index(cacheStorage, 8);
  TEST_ASSERT_EQUAL_INT(-1, index.oldest());
  index.put(cache::Index::make(10, 5, 0, 1, "a"));
  index.put(cache::Index::make(11, 2, 0, 1, "b"));
  index.put(cache::Index::make(12, 9, 0, 1, "c"));
  TEST_ASSERT_EQUAL_UINT32(11, index.at(index.oldest()).postId);

  index.put(cache::Index::make(11, 10, 0, 1, "b")); // played again: no longer the oldest
  TEST_ASSERT_EQUAL_UINT32(10, index.at(index.oldest()).postId);
}

static void test_cache_stops_at_capacity_but_still_updates() {
  cache::Index index(cacheStorage, 3);
  for (uint32_t id = 1; id <= 3; id++) {
    TEST_ASSERT_TRUE(index.put(cache::Index::make(id, id, 0, 1, "x")));
  }
  TEST_ASSERT_FALSE(index.put(cache::Index::make(4, 4, 0, 1, "x")));
  TEST_ASSERT_TRUE(index.put(cache::Index::make(2, 9, 0, 1, "x"))); // existing entry
  TEST_ASSERT_EQUAL_INT(3, index.count());
  TEST_ASSERT_TRUE(index.remove(1));
  TEST_ASSERT_FALSE(index.remove(1));
  TEST_ASSERT_TRUE(index.put(cache::Index::make(4, 4, 0, 1, "x")));
}

static void test_cache_owner_names_cannot_break_a_line() {
  cache::Entry entry = cache::Index::make(5, 1, 0, 1, "two words\nA 6 1 1 1 forged");
  TEST_ASSERT_TRUE(strchr(entry.owner, ' ') == nullptr);
  TEST_ASSERT_TRUE(strchr(entry.owner, '\n') == nullptr);

  cache::Index index(cacheStorage, 8);
  TEST_ASSERT_TRUE(index.apply(cache::Index::addLine(entry).c_str()));
  TEST_ASSERT_EQUAL_INT(1, index.count()); // the forged record did not appear
  TEST_ASSERT_EQUAL_STRING("-", cache::Index::make(5, 1, 0, 1, "").owner);
  TEST_ASSERT_EQUAL_INT(cache::OWNER_CHARS - 1,
                        (int)strlen(cache::Index::make(5, 1, 0, 1, "abcdefghijklmnopqrstuvwxyz0123456789").owner));
}

// ---------------------------------------------------------------------------

// --- taps --------------------------------------------------------------------

// Feeds the detector 4 ms samples (250 Hz, as on the board) of the panel lying
// still, with `extra` g added straight down from `from` to `to` ms. Returns how
// many single and double taps it reported up to `until`.
struct TapRun {
  tap::Detector detector;
  uint32_t ms = 0;
  int singles = 0, doubles = 0;

  void run(uint32_t until, uint32_t from = 0, uint32_t to = 0, float extra = 0) {
    for (; ms < until; ms += 4) {
      float z = 1.0f + (ms >= from && ms < to ? extra : 0.0f);
      tap::Gesture g = detector.feed(ms, 0.0f, 0.0f, z);
      if (g == tap::Gesture::Single) singles++;
      if (g == tap::Gesture::Double) doubles++;
    }
  }
};

static void test_taps_threshold_follows_sensitivity() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, tap::Detector::thresholdFor(1));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, tap::Detector::thresholdFor(5));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, tap::Detector::thresholdFor(10));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, tap::Detector::thresholdFor(-3));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, tap::Detector::thresholdFor(40));
}

static void test_taps_stillness_is_not_a_tap() {
  TapRun r;
  r.run(3000);
  TEST_ASSERT_EQUAL(0, r.singles + r.doubles);
}

static void test_taps_one_knock_is_a_single_tap() {
  TapRun r;
  r.run(500);
  r.run(1500, 500, 512, 2.0f);
  TEST_ASSERT_EQUAL(1, r.singles);
  TEST_ASSERT_EQUAL(0, r.doubles);
  TEST_ASSERT_TRUE(r.detector.lastTapPeak() > 1.5f);
}

static void test_taps_two_knocks_are_a_double_tap() {
  TapRun r;
  r.run(500);
  r.run(700, 500, 512, 2.0f);
  r.run(2000, 700, 712, 2.0f);
  TEST_ASSERT_EQUAL(0, r.singles);
  TEST_ASSERT_EQUAL(1, r.doubles);
}

static void test_taps_a_soft_knock_needs_a_higher_sensitivity() {
  TapRun soft;
  soft.run(500);
  soft.run(1500, 500, 512, 0.8f);
  TEST_ASSERT_EQUAL(0, soft.singles + soft.doubles);

  TapRun sensitive;
  sensitive.detector.setSensitivity(8); // 0.75 g
  sensitive.run(500);
  sensitive.run(1500, 500, 512, 0.8f);
  TEST_ASSERT_EQUAL(1, sensitive.singles);
}

static void test_taps_moving_the_panel_is_not_a_tap() {
  TapRun r;
  r.run(500);
  r.run(2500, 500, 700, 2.0f); // 200 ms of push, far longer than a knock
  TEST_ASSERT_EQUAL(0, r.singles + r.doubles);
}

static void test_taps_knocks_far_apart_are_two_singles() {
  TapRun r;
  r.run(500);
  r.run(2000, 500, 512, 2.0f);
  r.run(3500, 2000, 2012, 2.0f); // after the lockout has ended
  TEST_ASSERT_EQUAL(2, r.singles);
  TEST_ASSERT_EQUAL(0, r.doubles);
}

static void test_taps_lockout_swallows_a_quick_third_knock() {
  TapRun r;
  r.run(500);
  r.run(700, 500, 512, 2.0f);
  r.run(900, 700, 712, 2.0f);   // double tap reported here
  r.run(2500, 900, 912, 2.0f);  // inside the lockout
  TEST_ASSERT_EQUAL(1, r.doubles);
  TEST_ASSERT_EQUAL(0, r.singles);
}

// --- likes -------------------------------------------------------------------

static void test_likes_a_like_waits_to_be_sent() {
  likes::Book book;
  book.set(10, true);
  TEST_ASSERT_TRUE(book.liked(10));
  uint32_t post;
  bool wanted;
  TEST_ASSERT_TRUE(book.nextPending(post, wanted));
  TEST_ASSERT_EQUAL_UINT32(10, post);
  TEST_ASSERT_TRUE(wanted);
  book.confirmed(10, true);
  TEST_ASSERT_FALSE(book.nextPending(post, wanted));
  TEST_ASSERT_TRUE(book.liked(10));
  TEST_ASSERT_EQUAL(1, book.count());
}

static void test_likes_taken_back_before_sending_is_never_sent() {
  likes::Book book;
  book.set(10, true);
  book.set(10, false);
  uint32_t post;
  bool wanted;
  TEST_ASSERT_FALSE(book.nextPending(post, wanted));
  TEST_ASSERT_FALSE(book.liked(10));
  TEST_ASSERT_EQUAL(0, (int)book.save().size());
}

static void test_likes_unlike_after_sending_sends_a_revoke() {
  likes::Book book;
  book.set(10, true);
  book.confirmed(10, true);
  book.set(10, false);
  uint32_t post;
  bool wanted;
  TEST_ASSERT_TRUE(book.nextPending(post, wanted));
  TEST_ASSERT_FALSE(wanted);
  book.confirmed(10, false);
  TEST_ASSERT_FALSE(book.nextPending(post, wanted));
  TEST_ASSERT_EQUAL(0, (int)book.save().size()); // nothing left to remember
}

static void test_likes_oldest_change_is_sent_first() {
  likes::Book book;
  book.set(1, true);
  book.set(2, true);
  book.set(3, true);
  uint32_t post;
  bool wanted;
  book.nextPending(post, wanted);
  TEST_ASSERT_EQUAL_UINT32(1, post);
  book.drop(1); // deleted on Makapix
  book.nextPending(post, wanted);
  TEST_ASSERT_EQUAL_UINT32(2, post);
  TEST_ASSERT_EQUAL(2, book.pending());
}

static void test_likes_list_is_most_recent_first() {
  likes::Book book;
  book.set(1, true);
  book.set(2, true);
  book.set(3, true);
  book.set(1, true); // liked again: now the most recent
  std::vector<uint32_t> ids = book.list();
  TEST_ASSERT_EQUAL(3, (int)ids.size());
  TEST_ASSERT_EQUAL_UINT32(1, ids[0]);
  TEST_ASSERT_EQUAL_UINT32(3, ids[1]);
  TEST_ASSERT_EQUAL_UINT32(2, ids[2]);
}

static void test_likes_capacity_forgets_only_sent_entries() {
  likes::Book book;
  for (uint32_t i = 1; i <= (uint32_t)likes::Book::CAPACITY; i++) {
    book.set(i, true);
    book.confirmed(i, true);
  }
  book.set(100000, true); // unsent, over capacity
  TEST_ASSERT_EQUAL(likes::Book::CAPACITY, book.count());
  TEST_ASSERT_FALSE(book.liked(1)); // the oldest sent one made room
  TEST_ASSERT_TRUE(book.liked(100000));
  TEST_ASSERT_EQUAL(1, book.pending());
}

static void test_likes_survive_saving_and_loading() {
  likes::Book book;
  book.set(7, true);
  book.confirmed(7, true);
  book.set(4000000000u, true);
  book.set(9, true);
  book.confirmed(9, true);
  book.set(9, false); // a revoke still to send

  likes::Book copy;
  TEST_ASSERT_TRUE(copy.load(book.save()));
  TEST_ASSERT_TRUE(copy.liked(7));
  TEST_ASSERT_TRUE(copy.liked(4000000000u));
  TEST_ASSERT_FALSE(copy.liked(9));
  TEST_ASSERT_EQUAL(2, copy.pending());
}

static void test_likes_refuse_damaged_data() {
  likes::Book book;
  book.set(5, true);
  TEST_ASSERT_FALSE(book.load(std::string("\x01\x02\x03", 3)));                 // not whole entries
  TEST_ASSERT_FALSE(book.load(std::string("\x00\x00\x00\x00\x01", 5)));         // post 0
  TEST_ASSERT_FALSE(book.load(std::string("\x05\x00\x00\x00\x09", 5)));         // unknown flags
  TEST_ASSERT_TRUE(book.liked(5)); // a refused load changes nothing
}

// --- views -------------------------------------------------------------------

static void test_views_queue_drops_the_oldest_when_full() {
  views::Queue queue;
  for (uint32_t i = 1; i <= views::Queue::CAPACITY; i++) {
    TEST_ASSERT_TRUE(queue.push({i, 0, false}));
  }
  TEST_ASSERT_FALSE(queue.push({5000, 0, false}));
  TEST_ASSERT_EQUAL((int)views::Queue::CAPACITY, (int)queue.size());
  TEST_ASSERT_EQUAL_UINT32(2, queue.front().postId);
}

static void test_views_lines_round_trip() {
  views::View view = {2486, 1792160700, true}, back = {0, 0, false};
  std::string line = views::toLine(view);
  TEST_ASSERT_EQUAL_STRING("V 2486 1792160700 a\n", line.c_str());
  TEST_ASSERT_TRUE(views::fromLine(line.c_str(), back));
  TEST_ASSERT_EQUAL_UINT32(2486, back.postId);
  TEST_ASSERT_EQUAL_UINT32(1792160700, back.unixSeconds);
  TEST_ASSERT_TRUE(back.requested);
  TEST_ASSERT_TRUE(views::fromLine("V 7 0 c", back));
  TEST_ASSERT_FALSE(back.requested);
}

static void test_views_damaged_lines_are_refused() {
  views::View view = {1, 2, false};
  const char *bad[] = {"", "V", "V 12", "V 12 34", "V 12 34 x\n", "X 12 34 c\n", "V 0 34 c\n",
                       "V -5 34 c\n", "V 12 -1 c\n", "V 12 34 cc\n", "V 12 3a c\n"};
  for (const char *line : bad) {
    TEST_ASSERT_FALSE_MESSAGE(views::fromLine(line, view), line);
  }
  TEST_ASSERT_EQUAL_UINT32(1, view.postId); // untouched
}

static void test_views_times_are_iso_utc() {
  TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z", views::isoTime(0).c_str());
  TEST_ASSERT_EQUAL_STRING("2000-02-29T12:34:56Z", views::isoTime(951827696).c_str());
  TEST_ASSERT_EQUAL_STRING("2026-10-16T14:05:00Z", views::isoTime(1792159500).c_str());
  TEST_ASSERT_EQUAL_STRING("2106-02-07T06:28:15Z", views::isoTime(0xFFFFFFFFu).c_str());
}

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

  RUN_TEST(test_transitions_list_is_complete_and_findable);
  RUN_TEST(test_transitions_start_on_the_old_image);
  RUN_TEST(test_transitions_end_on_the_new_image);
  RUN_TEST(test_transitions_change_something_part_way);
  RUN_TEST(test_transitions_repeat_exactly_for_one_seed);
  RUN_TEST(test_transitions_tolerate_progress_out_of_range);
  RUN_TEST(test_transitions_survive_a_full_sweep);
  RUN_TEST(test_transitions_wipe_only_moves_the_edge);

  RUN_TEST(test_cache_records_survive_a_round_trip);
  RUN_TEST(test_cache_replay_keeps_the_latest_and_honours_removals);
  RUN_TEST(test_cache_ignores_damaged_lines);
  RUN_TEST(test_cache_oldest_is_the_lowest_sequence);
  RUN_TEST(test_cache_stops_at_capacity_but_still_updates);
  RUN_TEST(test_cache_owner_names_cannot_break_a_line);

  RUN_TEST(test_taps_threshold_follows_sensitivity);
  RUN_TEST(test_taps_stillness_is_not_a_tap);
  RUN_TEST(test_taps_one_knock_is_a_single_tap);
  RUN_TEST(test_taps_two_knocks_are_a_double_tap);
  RUN_TEST(test_taps_a_soft_knock_needs_a_higher_sensitivity);
  RUN_TEST(test_taps_moving_the_panel_is_not_a_tap);
  RUN_TEST(test_taps_knocks_far_apart_are_two_singles);
  RUN_TEST(test_taps_lockout_swallows_a_quick_third_knock);

  RUN_TEST(test_likes_a_like_waits_to_be_sent);
  RUN_TEST(test_likes_taken_back_before_sending_is_never_sent);
  RUN_TEST(test_likes_unlike_after_sending_sends_a_revoke);
  RUN_TEST(test_likes_oldest_change_is_sent_first);
  RUN_TEST(test_likes_list_is_most_recent_first);
  RUN_TEST(test_likes_capacity_forgets_only_sent_entries);
  RUN_TEST(test_likes_survive_saving_and_loading);
  RUN_TEST(test_likes_refuse_damaged_data);

  RUN_TEST(test_views_queue_drops_the_oldest_when_full);
  RUN_TEST(test_views_lines_round_trip);
  RUN_TEST(test_views_damaged_lines_are_refused);
  RUN_TEST(test_views_times_are_iso_utc);
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
