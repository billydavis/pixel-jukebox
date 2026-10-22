// Unit tests for lib/jukebox_core, the code with no hardware dependencies.
// They run on the board (pio test) and would run unchanged on a PC.

#include <unity.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "beat_follower.h"
#include "cache_index.h"
#include "channels.h"
#include "crash_cause.h"
#include "gif_pad.h"
#include "html_escape.h"
#include "likes_book.h"
#include "live_rules.h"
#include "night_rules.h"
#include "orientation.h"
#include "pin_rules.h"
#include "rotation_rules.h"
#include "screen_rules.h"
#include "rtc_time.h"
#include "shtc3_math.h"
#include "spectrum_math.h"
#include "starter_pack.h"
#include "tap_detector.h"
#include "test_patterns.h"
#include "transitions.h"
#include "upload_rules.h"
#include "view_queue.h"
#include "weather_rules.h"

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
  TEST_ASSERT_TRUE(valid({{"makapix", 3, true}, {"clock", 1, true}}));
}

static void test_rotation_accepts_repeats_and_the_limits() {
  TEST_ASSERT_TRUE(valid({{"makapix", 1, true}, {"clock", 1, true}, {"makapix", 1, true}, {"clock", 1, true}}));
  TEST_ASSERT_TRUE(valid({{"clock", MAX_TURNS, true}}));
  TEST_ASSERT_TRUE(valid(std::vector<RotationItem>(MAX_ENTRIES, {"makapix", 1, true})));
}

static void test_rotation_rejects_empty_and_oversized() {
  std::string error;
  TEST_ASSERT_FALSE(valid({}, &error));
  TEST_ASSERT_EQUAL_STRING("rotation must have 1 to 8 entries", error.c_str());
  TEST_ASSERT_FALSE(valid(std::vector<RotationItem>(MAX_ENTRIES + 1, {"makapix", 1, true})));
}

static void test_rotation_rejects_unknown_slots() {
  std::string error;
  TEST_ASSERT_FALSE(valid({{"makapix", 1, true}, {"fire", 1, true}}, &error));
  TEST_ASSERT_EQUAL_STRING("unknown slot: fire", error.c_str());
  TEST_ASSERT_FALSE(valid({{"", 1, true}}));
  TEST_ASSERT_FALSE(valid({{"Makapix", 1, true}})); // names are case-sensitive
}

static void test_rotation_pausing_keeps_entries_but_needs_one_on() {
  std::string error;
  TEST_ASSERT_TRUE(valid({{"makapix", 3, false}, {"clock", 1, true}}));
  TEST_ASSERT_FALSE(valid({{"makapix", 3, false}, {"clock", 1, false}}, &error));
  TEST_ASSERT_EQUAL_STRING("at least one slot must stay on", error.c_str());
  // A paused entry is still checked: a bad one is refused even while paused.
  TEST_ASSERT_FALSE(valid({{"nope", 1, false}, {"clock", 1, true}}));
}

static void test_rotation_rejects_turns_out_of_range() {
  std::string error;
  TEST_ASSERT_FALSE(valid({{"clock", 0, true}}, &error));
  TEST_ASSERT_EQUAL_STRING("turns must be 1 to 20", error.c_str());
  TEST_ASSERT_FALSE(valid({{"clock", -1, true}}));
  TEST_ASSERT_FALSE(valid({{"clock", MAX_TURNS + 1, true}}));
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

static void test_cache_index_keeps_the_storage_key() {
  cache::Entry storage[4];
  cache::Index index(storage, 4);
  const char *key = "586cbe4d-3161-4dbf-8824-9dc8c317b7b8";
  cache::Entry entry = cache::Index::make(3898, 3, 30000, 900, "Fab", key);
  std::string line = cache::Index::addLine(entry);
  TEST_ASSERT_EQUAL_STRING("A 3898 3 30000 900 Fab 586cbe4d-3161-4dbf-8824-9dc8c317b7b8\n", line.c_str());
  TEST_ASSERT_TRUE(index.apply(line.c_str()));
  TEST_ASSERT_EQUAL_STRING(key, index.find(3898)->key);
  TEST_ASSERT_EQUAL_STRING("Fab", index.find(3898)->owner);

  // A line from before keys were kept: no key, owner as before.
  TEST_ASSERT_TRUE(index.apply("A 77 1 30000 500 Odio\n"));
  TEST_ASSERT_EQUAL_STRING("", index.find(77)->key);
  TEST_ASSERT_EQUAL_STRING("Odio", index.find(77)->owner);
  TEST_ASSERT_EQUAL_STRING("A 77 1 30000 500 Odio\n", cache::Index::addLine(*index.find(77)).c_str());

  // Something that is not a key is dropped, the rest kept.
  TEST_ASSERT_TRUE(index.apply("A 78 1 30000 500 Odio <script>\n"));
  TEST_ASSERT_EQUAL_STRING("", index.find(78)->key);
  TEST_ASSERT_EQUAL_STRING("", cache::Index::make(1, 1, 1, 1, "x", "not a key").key);
}

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
  views::View view = {2486, 1792160700, true, "", ""}, back = {0, 0, false, "x", "y"};
  std::string line = views::toLine(view);
  TEST_ASSERT_EQUAL_STRING("V 2486 1792160700 a all\n", line.c_str());
  TEST_ASSERT_TRUE(views::fromLine(line.c_str(), back));
  TEST_ASSERT_EQUAL_UINT32(2486, back.postId);
  TEST_ASSERT_EQUAL_UINT32(1792160700, back.unixSeconds);
  TEST_ASSERT_TRUE(back.requested);
  TEST_ASSERT_TRUE(back.channel.empty());
  TEST_ASSERT_TRUE(views::fromLine("V 7 0 c", back)); // written before channels were recorded
  TEST_ASSERT_FALSE(back.requested);
  TEST_ASSERT_TRUE(back.channel.empty());
}

static void test_views_remember_their_channel() {
  views::View view = {12, 34, false, "hashtag", "cat"}, back;
  std::string line = views::toLine(view);
  TEST_ASSERT_EQUAL_STRING("V 12 34 c hashtag cat\n", line.c_str());
  TEST_ASSERT_TRUE(views::fromLine(line.c_str(), back));
  TEST_ASSERT_EQUAL_STRING("hashtag", back.channel.c_str());
  TEST_ASSERT_EQUAL_STRING("cat", back.hashtag.c_str());
  TEST_ASSERT_TRUE(views::fromLine("V 12 34 c promoted\n", back));
  TEST_ASSERT_EQUAL_STRING("promoted", back.channel.c_str());
  TEST_ASSERT_TRUE(back.hashtag.empty());
  TEST_ASSERT_FALSE(views::fromLine("V 12 34 c Promoted\n", back)); // not a channel name
  TEST_ASSERT_FALSE(views::fromLine("V 12 34 c hashtag cat extra\n", back));
}

static void test_views_damaged_lines_are_refused() {
  views::View view = {1, 2, false};
  const char *bad[] = {"", "V", "V 12", "V 12 34", "V 12 34 x\n", "X 12 34 c\n", "V 0 34 c\n",
                       "V -5 34 c\n", "V 12 -1 c\n", "V 12 34 cc\n", "V 12 3a c\n", "V 12 34 c \n"};
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

// --- the clock chip ------------------------------------------------------------

static void test_rtc_encodes_a_known_time() {
  uint8_t regs[rtc::REGISTERS];
  rtc::encode(1792159500, regs); // Friday 2026-10-16 14:05:00 UTC
  const uint8_t expected[rtc::REGISTERS] = {0x00, 0x05, 0x14, 0x16, 5, 0x10, 0x26};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, regs, rtc::REGISTERS);
}

static void test_rtc_round_trips() {
  const uint32_t times[] = {946684800, 951827696, 1709251199, 1792159500, 4102444799u};
  for (uint32_t t : times) {
    uint8_t regs[rtc::REGISTERS];
    uint32_t back = 0;
    rtc::encode(t, regs);
    TEST_ASSERT_TRUE(rtc::decode(regs, back));
    TEST_ASSERT_EQUAL_UINT32(t, back);
  }
}

static void test_rtc_refuses_a_stopped_oscillator() {
  uint8_t regs[rtc::REGISTERS];
  rtc::encode(1792159500, regs);
  regs[0] |= 0x80;
  uint32_t t = 123;
  TEST_ASSERT_FALSE(rtc::decode(regs, t));
  TEST_ASSERT_EQUAL_UINT32(123, t);
}

static void test_rtc_refuses_impossible_values() {
  uint8_t regs[rtc::REGISTERS];
  uint32_t t;
  rtc::encode(1792159500, regs);
  regs[0] = 0x1A; // not BCD
  TEST_ASSERT_FALSE(rtc::decode(regs, t));
  rtc::encode(1792159500, regs);
  regs[5] = 0x13; // month 13
  TEST_ASSERT_FALSE(rtc::decode(regs, t));
  rtc::encode(1792159500, regs);
  regs[3] = 0x30; // 30 February
  regs[5] = 0x02;
  TEST_ASSERT_FALSE(rtc::decode(regs, t));
  regs[3] = 0x29; // 29 February 2026 is not a leap day either
  TEST_ASSERT_FALSE(rtc::decode(regs, t));
  regs[6] = 0x24; // but in 2024 it is
  TEST_ASSERT_TRUE(rtc::decode(regs, t));
}

static void test_rtc_clamps_to_the_chips_century() {
  uint8_t regs[rtc::REGISTERS];
  uint32_t t;
  rtc::encode(0, regs);
  TEST_ASSERT_TRUE(rtc::decode(regs, t));
  TEST_ASSERT_EQUAL_UINT32(946684800, t); // 2000-01-01
  TEST_ASSERT_EQUAL_UINT8(6, regs[4]);    // a Saturday
}

static void test_rtc_dates_and_days_agree() {
  TEST_ASSERT_EQUAL_INT32(0, rtc::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(10957, rtc::daysFromCivil(2000, 1, 1));
  int y, m, d;
  for (int32_t days = 10957; days < 10957 + 366 * 8; days += 13) {
    rtc::civilFromDays(days, y, m, d);
    TEST_ASSERT_EQUAL_INT32(days, rtc::daysFromCivil(y, m, d));
  }
}

// --- crash records ----------------------------------------------------------------

static void test_crash_causes_have_their_idf_names() {
  TEST_ASSERT_EQUAL_STRING("IllegalInstruction", crash::causeName(0));
  TEST_ASSERT_EQUAL_STRING("IntegerDivideByZero", crash::causeName(6));
  TEST_ASSERT_EQUAL_STRING("LoadStoreAlignment", crash::causeName(9));
  TEST_ASSERT_EQUAL_STRING("InstFetchProhibited", crash::causeName(20));
  TEST_ASSERT_EQUAL_STRING("LoadProhibited", crash::causeName(28));
  TEST_ASSERT_EQUAL_STRING("StoreProhibited", crash::causeName(29));
}

static void test_crash_unknown_causes_are_named_so() {
  TEST_ASSERT_EQUAL_STRING("Unknown", crash::causeName(10));
  TEST_ASSERT_EQUAL_STRING("Unknown", crash::causeName(30));
  TEST_ASSERT_EQUAL_STRING("Unknown", crash::causeName(0xFFFFFFFFu));
}

static void test_crash_hints_cover_the_common_causes() {
  TEST_ASSERT_NOT_NULL(crash::causeHint(28));
  TEST_ASSERT_NOT_NULL(crash::causeHint(29));
  TEST_ASSERT_NULL(crash::causeHint(4));
}

// --- channels ------------------------------------------------------------------

static void test_channels_simple_ones_parse() {
  channels::Channel c;
  std::string error;
  TEST_ASSERT_TRUE(channels::parse("all", c, error));
  TEST_ASSERT_TRUE(c.kind == channels::Kind::All);
  TEST_ASSERT_TRUE(channels::parse("", c, error)); // no channel: everything
  TEST_ASSERT_TRUE(c.kind == channels::Kind::All);
  TEST_ASSERT_TRUE(channels::parse("promoted", c, error));
  TEST_ASSERT_EQUAL_STRING("promoted", channels::makapixName(c.kind));
  TEST_ASSERT_TRUE(channels::parse("mine", c, error));
  TEST_ASSERT_EQUAL_STRING("user", channels::makapixName(c.kind));
  TEST_ASSERT_EQUAL_STRING("My art", channels::label(c).c_str());
}

static void test_channels_names_and_tags_parse_and_tidy() {
  channels::Channel c;
  std::string error;
  TEST_ASSERT_TRUE(channels::parse("artist:@Fab", c, error));
  TEST_ASSERT_TRUE(c.kind == channels::Kind::Artist);
  TEST_ASSERT_EQUAL_STRING("Fab", c.value.c_str()); // @ dropped, case kept
  TEST_ASSERT_EQUAL_STRING("artist:Fab", channels::format(c).c_str());
  TEST_ASSERT_EQUAL_STRING("by_user", channels::makapixName(c.kind));
  TEST_ASSERT_EQUAL_STRING("@Fab", channels::label(c).c_str());

  TEST_ASSERT_TRUE(channels::parse("hashtag:#Cat", c, error));
  TEST_ASSERT_EQUAL_STRING("cat", c.value.c_str()); // hashtags are lower case on Makapix
  TEST_ASSERT_EQUAL_STRING("#cat", channels::label(c).c_str());

  TEST_ASSERT_TRUE(channels::parse("likedby:Odio", c, error));
  TEST_ASSERT_EQUAL_STRING("reactions", channels::makapixName(c.kind));
  TEST_ASSERT_EQUAL_STRING("Liked by @Odio", channels::label(c).c_str());
}

static void test_channels_names_in_any_script_are_counted_in_characters() {
  channels::Channel c;
  std::string error;
  // 32 three-byte characters is 96 bytes but still 32 characters.
  std::string name;
  for (int i = 0; i < 32; i++) name += "\xE3\x81\x82"; // あ
  TEST_ASSERT_TRUE(channels::parse("artist:" + name, c, error));
  TEST_ASSERT_FALSE(channels::parse("artist:" + name + "x", c, error));
}

static void test_channels_reject_what_is_not_a_channel() {
  channels::Channel c;
  std::string error;
  const char *bad[] = {"everything", "artist", "artist:", "artist:@", "hashtag:#", "likedby:",
                       "artist:two words", "hashtag:a:b", "artist:a/b", "ARTIST:Fab",
                       "artist:abcdefghijklmnopqrstuvwxyz0123456"};
  for (const char *spec : bad) {
    error.clear();
    TEST_ASSERT_FALSE_MESSAGE(channels::parse(spec, c, error), spec);
    TEST_ASSERT_FALSE_MESSAGE(error.empty(), spec);
  }
}

static void test_channels_a_refused_parse_changes_nothing() {
  channels::Channel c;
  std::string error;
  channels::parse("hashtag:cat", c, error);
  TEST_ASSERT_FALSE(channels::parse("hashtag:", c, error));
  TEST_ASSERT_EQUAL_STRING("cat", c.value.c_str());
}

// --- the night schedule --------------------------------------------------------

static uint32_t at(int hours, int minutes, int seconds = 0) { return hours * 3600 + minutes * 60 + seconds; }

static night::Settings tenToSeven() {
  night::Settings s;
  s.enabled = true;
  s.start = 22 * 60;
  s.end = 7 * 60;
  s.fadeMinutes = 10;
  return s;
}

static void test_night_times_parse_and_format() {
  uint16_t m = 0;
  TEST_ASSERT_TRUE(night::parseTime("22:00", m));
  TEST_ASSERT_EQUAL_UINT16(1320, m);
  TEST_ASSERT_TRUE(night::parseTime("00:00", m));
  TEST_ASSERT_EQUAL_UINT16(0, m);
  TEST_ASSERT_TRUE(night::parseTime("23:59", m));
  TEST_ASSERT_EQUAL_STRING("07:05", night::formatTime(425).c_str());
  const char *bad[] = {"24:00", "7:00", "07:60", "0700", "07-00", "", "ab:cd", "07:00 "};
  for (const char *text : bad) {
    TEST_ASSERT_FALSE_MESSAGE(night::parseTime(text, m), text);
  }
}

static void test_night_wraps_past_midnight() {
  night::Settings s = tenToSeven();
  TEST_ASSERT_FALSE(night::isNight(s, at(21, 59, 59)));
  TEST_ASSERT_TRUE(night::isNight(s, at(22, 0)));
  TEST_ASSERT_TRUE(night::isNight(s, at(0, 0)));
  TEST_ASSERT_TRUE(night::isNight(s, at(6, 59, 59)));
  TEST_ASSERT_FALSE(night::isNight(s, at(7, 0)));
  TEST_ASSERT_FALSE(night::isNight(s, at(12, 0)));
}

static void test_night_within_one_day_works_too() {
  night::Settings s = tenToSeven();
  s.start = 13 * 60; // an afternoon nap
  s.end = 15 * 60;
  TEST_ASSERT_TRUE(night::isNight(s, at(14, 0)));
  TEST_ASSERT_FALSE(night::isNight(s, at(16, 0)));
  TEST_ASSERT_FALSE(night::isNight(s, at(2, 0)));
}

static void test_night_off_or_empty_is_never_night() {
  night::Settings s = tenToSeven();
  s.enabled = false;
  TEST_ASSERT_FALSE(night::isNight(s, at(23, 0)));
  TEST_ASSERT_EQUAL_FLOAT(0, night::depth(s, at(23, 0)));
  s.enabled = true;
  s.end = s.start;
  TEST_ASSERT_FALSE(night::isNight(s, at(22, 0)));
  std::string error;
  TEST_ASSERT_FALSE(night::valid(s, error));
}

static void test_night_fades_in_and_out() {
  night::Settings s = tenToSeven();
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, night::depth(s, at(21, 59)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, night::depth(s, at(22, 0)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, night::depth(s, at(22, 5)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, night::depth(s, at(22, 10)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, night::depth(s, at(3, 0)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, night::depth(s, at(7, 0)));   // day starts here, fading up
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, night::depth(s, at(7, 5)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, night::depth(s, at(7, 10)));
  s.fadeMinutes = 0;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, night::depth(s, at(22, 0)));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, night::depth(s, at(7, 0)));
}

static void test_night_blend_goes_from_day_to_night() {
  TEST_ASSERT_EQUAL_UINT8(51, night::blend(51, 8, 0));
  TEST_ASSERT_EQUAL_UINT8(8, night::blend(51, 8, 1));
  TEST_ASSERT_EQUAL_UINT8(30, night::blend(51, 8, 0.5f)); // 29.5 rounds up
  TEST_ASSERT_EQUAL_UINT8(0, night::blend(51, 0, 1));
  TEST_ASSERT_EQUAL_UINT8(100, night::blend(10, 100, 2.0f)); // out of range is clamped
}

static void test_night_next_switch() {
  night::Settings s = tenToSeven();
  TEST_ASSERT_EQUAL_UINT32(3600, night::secondsToNextSwitch(s, at(21, 0)));
  TEST_ASSERT_EQUAL_UINT32(7 * 3600, night::secondsToNextSwitch(s, at(0, 0)));
  TEST_ASSERT_EQUAL_UINT32(9 * 3600, night::secondsToNextSwitch(s, at(22, 0))); // just started: next is 07:00
}

static void test_night_actions_and_limits() {
  night::Action a;
  TEST_ASSERT_TRUE(night::actionFromName("clock", a));
  TEST_ASSERT_EQUAL_STRING("clock", night::actionName(a));
  TEST_ASSERT_FALSE(night::actionFromName("sleep", a));
  night::Settings s = tenToSeven();
  std::string error;
  TEST_ASSERT_TRUE(night::valid(s, error));
  s.fadeMinutes = 61;
  TEST_ASSERT_FALSE(night::valid(s, error));
}

// --- the case temperature sensor --------------------------------------------------

static void test_shtc3_checksum_matches_the_datasheet() {
  const uint8_t example[] = {0xBE, 0xEF}; // Sensirion's worked example
  TEST_ASSERT_EQUAL_HEX8(0x92, shtc3::crc(example, 2));
}

static void test_shtc3_decodes_a_measurement() {
  // 0x6666 is 40 % of full scale: 25 degrees and 40 percent.
  uint8_t bytes[6] = {0x66, 0x66, 0, 0x66, 0x66, 0};
  bytes[2] = shtc3::crc(bytes, 2);
  bytes[5] = shtc3::crc(bytes + 3, 2);
  float t = 0, h = 0;
  TEST_ASSERT_TRUE(shtc3::decode(bytes, t, h));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, t);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, h);
}

static void test_shtc3_refuses_a_damaged_measurement() {
  uint8_t bytes[6] = {0x66, 0x66, 0, 0x66, 0x66, 0};
  bytes[2] = shtc3::crc(bytes, 2);
  bytes[5] = shtc3::crc(bytes + 3, 2);
  bytes[4] ^= 0x01; // one bit flipped on the wire
  float t = 1, h = 2;
  TEST_ASSERT_FALSE(shtc3::decode(bytes, t, h));
  TEST_ASSERT_EQUAL_FLOAT(1, t);
}

static void test_shtc3_recognises_its_id() {
  TEST_ASSERT_TRUE(shtc3::isShtc3(0x0887));  // a typical SHTC3 ID
  TEST_ASSERT_TRUE(shtc3::isShtc3(0x0807));
  TEST_ASSERT_FALSE(shtc3::isShtc3(0x0000));
  TEST_ASSERT_FALSE(shtc3::isShtc3(0xFFFF));
}

// ---- pin_rules ---------------------------------------------------------------

static void test_pin_accepts_four_to_eight_digits() {
  TEST_ASSERT_TRUE(pin::valid("1234"));
  TEST_ASSERT_TRUE(pin::valid("12345678"));
  TEST_ASSERT_FALSE(pin::valid("123"));
  TEST_ASSERT_FALSE(pin::valid("123456789"));
  TEST_ASSERT_FALSE(pin::valid("12a4"));
  TEST_ASSERT_FALSE(pin::valid(" 1234"));
  TEST_ASSERT_FALSE(pin::valid(""));
}

static void test_pin_locks_after_five_wrong() {
  pin::Lockout lockout;
  bool justLocked = false;
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_FALSE(lockout.attempt(false, 1000, &justLocked));
    TEST_ASSERT_FALSE(justLocked);
  }
  TEST_ASSERT_FALSE(lockout.attempt(false, 1000, &justLocked));
  TEST_ASSERT_TRUE(justLocked);
  TEST_ASSERT_EQUAL(30, lockout.lockedSeconds(1000));
  TEST_ASSERT_EQUAL(1, lockout.lockedSeconds(30500));
  TEST_ASSERT_FALSE(lockout.attempt(true, 20000)); // even the right PIN waits
  TEST_ASSERT_EQUAL(0, lockout.lockedSeconds(31000));
  TEST_ASSERT_TRUE(lockout.attempt(true, 31000));
}

static void test_pin_right_one_resets_the_count() {
  pin::Lockout lockout;
  for (int i = 0; i < 4; i++) {
    lockout.attempt(false, 0);
  }
  TEST_ASSERT_TRUE(lockout.attempt(true, 0));
  for (int i = 0; i < 4; i++) {
    lockout.attempt(false, 0);
  }
  TEST_ASSERT_EQUAL(0, lockout.lockedSeconds(0));
}

static void test_pin_lockout_survives_the_clock_wrapping() {
  pin::Lockout lockout;
  uint32_t nearWrap = 0xFFFFF000u;
  for (int i = 0; i < 5; i++) {
    lockout.attempt(false, nearWrap);
  }
  TEST_ASSERT_TRUE(lockout.lockedSeconds(nearWrap + 10000) > 0); // past the wrap
  TEST_ASSERT_EQUAL(0, lockout.lockedSeconds(nearWrap + 30000));
}

static void test_pin_sessions_create_and_end() {
  pin::Sessions sessions;
  uint8_t random[16];
  for (int i = 0; i < 16; i++) {
    random[i] = (uint8_t)(i * 17);
  }
  std::string token = sessions.create(random, 0);
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff", token.c_str());
  TEST_ASSERT_TRUE(sessions.touch(token, 5));
  TEST_ASSERT_FALSE(sessions.touch("00112233445566778899aabbccddeefe", 5));
  TEST_ASSERT_FALSE(sessions.touch("", 5));
  sessions.end(token);
  TEST_ASSERT_FALSE(sessions.touch(token, 6));
  TEST_ASSERT_EQUAL(0, sessions.count());
}

static void test_pin_ninth_session_replaces_the_least_recent() {
  pin::Sessions sessions;
  std::vector<std::string> tokens;
  for (int i = 0; i < pin::Sessions::MAX; i++) {
    uint8_t random[16] = {(uint8_t)i};
    tokens.push_back(sessions.create(random, (uint32_t)i * 10));
  }
  sessions.touch(tokens[0], 100); // the first is used again, so the second is oldest
  uint8_t random[16] = {0xEE};
  std::string newest = sessions.create(random, 200);
  TEST_ASSERT_EQUAL(pin::Sessions::MAX, sessions.count());
  TEST_ASSERT_TRUE(sessions.touch(tokens[0], 201));
  TEST_ASSERT_FALSE(sessions.touch(tokens[1], 201));
  TEST_ASSERT_TRUE(sessions.touch(newest, 201));
}

static void test_pin_sessions_save_and_restore() {
  pin::Sessions sessions;
  uint8_t a[16] = {1}, b[16] = {2};
  std::string first = sessions.create(a, 0), second = sessions.create(b, 0);
  std::vector<std::string> saved = sessions.tokens();
  saved.push_back("not a token");
  pin::Sessions after;
  after.restore(saved, 0);
  TEST_ASSERT_EQUAL(2, after.count());
  TEST_ASSERT_TRUE(after.touch(first, 1));
  TEST_ASSERT_TRUE(after.touch(second, 1));
}

static void test_pin_reads_cookies() {
  TEST_ASSERT_EQUAL_STRING("abc", pin::cookieValue("jukebox_session=abc", "jukebox_session").c_str());
  TEST_ASSERT_EQUAL_STRING("abc", pin::cookieValue("x=1; jukebox_session=abc; y=2", "jukebox_session").c_str());
  TEST_ASSERT_EQUAL_STRING("", pin::cookieValue("old_jukebox_session=abc", "jukebox_session").c_str());
  TEST_ASSERT_EQUAL_STRING("", pin::cookieValue("jukebox_sessionx=abc", "jukebox_session").c_str());
  TEST_ASSERT_EQUAL_STRING("", pin::cookieValue("", "jukebox_session").c_str());
}

static void test_pin_compares_bytes() {
  uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 4}, c[4] = {1, 2, 3, 5};
  TEST_ASSERT_TRUE(pin::sameBytes(a, b, 4));
  TEST_ASSERT_FALSE(pin::sameBytes(a, c, 4));
}

// ---- screen_rules ------------------------------------------------------------

static screen::Inputs inputs(bool nightOff, bool nightClock, bool sound, bool request) {
  screen::Inputs in;
  in.nightOff = nightOff;
  in.nightClock = nightClock;
  in.sound = sound;
  in.requestPending = request;
  return in;
}

static void test_screen_order_of_who_wins() {
  using screen::Source;
  TEST_ASSERT_TRUE(screen::choose(inputs(false, false, false, false)) == Source::Rotation);
  TEST_ASSERT_TRUE(screen::choose(inputs(false, true, false, false)) == Source::NightClock);
  TEST_ASSERT_TRUE(screen::choose(inputs(false, true, true, false)) == Source::Sound);
  TEST_ASSERT_TRUE(screen::choose(inputs(true, false, true, false)) == Source::Dark);
  TEST_ASSERT_TRUE(screen::choose(inputs(true, true, true, true)) == Source::Request); // wakes the panel
  TEST_ASSERT_TRUE(screen::choose(inputs(false, false, true, true)) == Source::Request);
  TEST_ASSERT_EQUAL_STRING("nightClock", screen::name(Source::NightClock));
}

static void test_screen_giving_way() {
  using screen::Source;
  // A request runs its turn even if night falls meanwhile.
  TEST_ASSERT_FALSE(screen::shouldGiveWay(Source::Request, inputs(true, false, false, false)));
  // The rotation gives way to the night clock, the night clock to the morning.
  TEST_ASSERT_TRUE(screen::shouldGiveWay(Source::Rotation, inputs(false, true, false, false)));
  TEST_ASSERT_TRUE(screen::shouldGiveWay(Source::NightClock, inputs(false, false, false, false)));
  TEST_ASSERT_FALSE(screen::shouldGiveWay(Source::NightClock, inputs(false, true, false, false)));
  // Sound only gives way to dark.
  TEST_ASSERT_FALSE(screen::shouldGiveWay(Source::Sound, inputs(false, true, true, false)));
  TEST_ASSERT_TRUE(screen::shouldGiveWay(Source::Sound, inputs(true, false, true, false)));
  // A new request does not cut the running turn short; it is picked up next.
  TEST_ASSERT_FALSE(screen::shouldGiveWay(Source::Rotation, inputs(false, false, false, true)));
}

static void test_screen_what_next_and_hold_mean() {
  using screen::Source;
  TEST_ASSERT_TRUE(screen::nextChangesVisualizer(Source::Sound));
  TEST_ASSERT_FALSE(screen::nextChangesVisualizer(Source::Request));
  TEST_ASSERT_FALSE(screen::holdAllowed(Source::Sound));
  TEST_ASSERT_TRUE(screen::holdAllowed(Source::Request));
  TEST_ASSERT_TRUE(screen::holdAllowed(Source::NightClock));
  TEST_ASSERT_TRUE(screen::advancesRotation(Source::Rotation));
  TEST_ASSERT_FALSE(screen::advancesRotation(Source::Request));
  TEST_ASSERT_FALSE(screen::transitionAllowed(Source::Sound, Source::Rotation));
  TEST_ASSERT_FALSE(screen::transitionAllowed(Source::NightClock, Source::NightClock));
  TEST_ASSERT_TRUE(screen::transitionAllowed(Source::NightClock, Source::Rotation));
  TEST_ASSERT_TRUE(screen::transitionAllowed(Source::Request, Source::Sound));
}

static void test_screen_history_walks_back() {
  screen::History history;
  TEST_ASSERT_EQUAL_UINT32(0, history.back());
  history.shown(10);
  TEST_ASSERT_EQUAL_UINT32(0, history.back()); // nothing before the first
  history.shown(20);
  history.shown(20); // the same one again is not a new step
  history.shown(30);
  TEST_ASSERT_EQUAL_UINT32(20, history.back());
  TEST_ASSERT_EQUAL_UINT32(10, history.back());
  TEST_ASSERT_EQUAL_UINT32(0, history.back());
  history.shown(40); // after going back, new art carries on from there
  TEST_ASSERT_EQUAL_UINT32(10, history.back());
}

static void test_screen_history_keeps_the_latest() {
  screen::History history;
  for (uint32_t id = 1; id <= 40; id++) {
    history.shown(id);
  }
  TEST_ASSERT_EQUAL(screen::History::MAX, history.size());
  TEST_ASSERT_EQUAL_UINT32(39, history.back());
}

static void test_channels_from_makapix_commands() {
  channels::Channel channel;
  TEST_ASSERT_TRUE(channels::fromMakapix("all", "", "", channel));
  TEST_ASSERT_EQUAL_STRING("all", channels::format(channel).c_str());
  TEST_ASSERT_TRUE(channels::fromMakapix("promoted", "", "", channel));
  TEST_ASSERT_EQUAL_STRING("promoted", channels::format(channel).c_str());
  TEST_ASSERT_TRUE(channels::fromMakapix("by_user", "PixelMaster", "", channel));
  TEST_ASSERT_EQUAL_STRING("artist:PixelMaster", channels::format(channel).c_str());
  TEST_ASSERT_TRUE(channels::fromMakapix("reactions", "Fab", "", channel));
  TEST_ASSERT_EQUAL_STRING("likedby:Fab", channels::format(channel).c_str());
  TEST_ASSERT_TRUE(channels::fromMakapix("hashtag", "", "PixelArt", channel));
  TEST_ASSERT_EQUAL_STRING("hashtag:pixelart", channels::format(channel).c_str());
  TEST_ASSERT_FALSE(channels::fromMakapix("by_user", "", "", channel));   // no handle
  TEST_ASSERT_FALSE(channels::fromMakapix("hashtag", "", "", channel));   // no tag
  TEST_ASSERT_FALSE(channels::fromMakapix("trending", "", "", channel));  // not one we know
}

// ---- live_rules ---------------------------------------------------------------

static void test_live_backs_off_to_five_minutes() {
  TEST_ASSERT_EQUAL_UINT32(1, live::retryDelaySeconds(0));
  TEST_ASSERT_EQUAL_UINT32(1, live::retryDelaySeconds(1));
  TEST_ASSERT_EQUAL_UINT32(2, live::retryDelaySeconds(2));
  TEST_ASSERT_EQUAL_UINT32(256, live::retryDelaySeconds(9));
  TEST_ASSERT_EQUAL_UINT32(300, live::retryDelaySeconds(10));
  TEST_ASSERT_EQUAL_UINT32(300, live::retryDelaySeconds(1000));
}

static void test_live_counts_days_to_expiry() {
  const int64_t oct18 = 1792281600; // 2026-10-18T00:00:00Z
  TEST_ASSERT_EQUAL(0, live::daysUntil(2026, 10, 18, oct18));
  TEST_ASSERT_EQUAL(0, live::daysUntil(2026, 10, 18, oct18 + 86399));
  TEST_ASSERT_EQUAL(1, live::daysUntil(2026, 10, 19, oct18 + 3600));
  TEST_ASSERT_EQUAL(-1, live::daysUntil(2026, 10, 17, oct18));
  TEST_ASSERT_EQUAL(1090, live::daysUntil(2029, 10, 12, oct18)); // a 3-year certificate
  TEST_ASSERT_EQUAL(366, live::daysUntil(2028, 2, 29, 1803772800)); // from 2027-02-28, past 2028-02-29
}

static void test_live_renews_when_due_or_refused() {
  TEST_ASSERT_FALSE(live::shouldRenew(400, true, 0, false));
  TEST_ASSERT_TRUE(live::shouldRenew(30, true, 0, false));
  TEST_ASSERT_TRUE(live::shouldRenew(-5, true, 0, false)); // already expired
  TEST_ASSERT_FALSE(live::shouldRenew(10, false, 0, false)); // no clock: wait for the broker to say
  TEST_ASSERT_TRUE(live::shouldRenew(400, true, 3, false));
  TEST_ASSERT_FALSE(live::shouldRenew(10, true, 5, true)); // once a day at most
}

// ---- beat_follower ------------------------------------------------------------

static void test_beat_steady_loud_music_is_not_a_beat() {
  BeatFollower beats;
  for (int i = 0; i < 300; i++) { // ten seconds of the level pinned high
    beats.update(0.9f);
  }
  TEST_ASSERT_TRUE(beats.punch() < 0.01f);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.9f, beats.average());
}

static void test_beat_a_kick_stands_out_and_fades() {
  BeatFollower beats;
  for (int i = 0; i < 300; i++) {
    beats.update(0.5f);
  }
  float hit = beats.update(0.9f); // a kick drum over steady music
  TEST_ASSERT_TRUE(hit > 0.9f);
  float later = hit;
  for (int i = 0; i < 8; i++) { // a quarter of a second back at the steady level
    later = beats.update(0.5f);
  }
  TEST_ASSERT_TRUE(later < 0.2f);
}

static void test_beat_small_wobbles_are_ignored() {
  BeatFollower beats;
  for (int i = 0; i < 300; i++) {
    beats.update(i % 2 ? 0.52f : 0.48f);
  }
  TEST_ASSERT_TRUE(beats.punch() < 0.01f);
}

static void test_beat_repeated_beats_each_show() {
  BeatFollower beats;
  for (int i = 0; i < 300; i++) {
    beats.update(0.4f);
  }
  int seen = 0;
  for (int beat = 0; beat < 8; beat++) { // 120 bpm: one beat every 16 frames
    if (beats.update(0.85f) > 0.5f) {
      seen++;
    }
    for (int i = 0; i < 15; i++) {
      beats.update(0.4f);
    }
  }
  TEST_ASSERT_EQUAL(8, seen); // the average does not climb so far that later beats vanish
}

// ---- upload_rules -------------------------------------------------------------

static void test_upload_reads_gif_size() {
  const uint8_t gif[] = {'G', 'I', 'F', '8', '9', 'a', 32, 0, 16, 0, 0};
  int w = 0, h = 0;
  TEST_ASSERT_TRUE(uploads::gifSize(gif, sizeof(gif), w, h));
  TEST_ASSERT_EQUAL(32, w);
  TEST_ASSERT_EQUAL(16, h);
  const uint8_t big[] = {'G', 'I', 'F', '8', '7', 'a', 0x40, 0x01, 0x40, 0x01};
  TEST_ASSERT_TRUE(uploads::gifSize(big, sizeof(big), w, h));
  TEST_ASSERT_EQUAL(320, w);
  const uint8_t png[] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0};
  TEST_ASSERT_FALSE(uploads::gifSize(png, sizeof(png), w, h));
  TEST_ASSERT_FALSE(uploads::gifSize(gif, 9, w, h)); // too short for a header
}

static void test_upload_size_rules() {
  TEST_ASSERT_EQUAL_STRING("", uploads::sizeProblem(64, 64, 1000).c_str());
  TEST_ASSERT_EQUAL_STRING("", uploads::sizeProblem(16, 16, 100).c_str());
  TEST_ASSERT_TRUE(uploads::sizeProblem(65, 64, 1000).find("64 x 64") != std::string::npos);
  TEST_ASSERT_TRUE(uploads::sizeProblem(64, 64, 600 * 1024).find("512 KB") != std::string::npos);
  TEST_ASSERT_FALSE(uploads::sizeProblem(0, 64, 10).empty());
}

static void test_upload_tidies_names() {
  TEST_ASSERT_EQUAL_STRING("cat on a mat", uploads::tidyName("cat on a mat.GIF").c_str());
  TEST_ASSERT_EQUAL_STRING("a_b_c", uploads::tidyName("a/b:c").c_str());
  TEST_ASSERT_EQUAL_STRING("upload", uploads::tidyName("").c_str());
  TEST_ASSERT_EQUAL_STRING("upload", uploads::tidyName(" . ").c_str());
  TEST_ASSERT_EQUAL_STRING("caf__", uploads::tidyName("caf\xC3\xA9.gif").c_str()); // one non-ASCII letter, two bytes
  TEST_ASSERT_EQUAL(uploads::MAX_NAME, uploads::tidyName(std::string(100, 'x')).size());
}

static void test_upload_file_names_round_trip() {
  std::string file = uploads::fileName(12, 32, 16, "cat on a mat.gif");
  TEST_ASSERT_EQUAL_STRING("12-32x16-cat on a mat.gif", file.c_str());
  uint32_t id = 0;
  int w = 0, h = 0;
  std::string name;
  TEST_ASSERT_TRUE(uploads::parseFileName(file, id, w, h, name));
  TEST_ASSERT_EQUAL_UINT32(12, id);
  TEST_ASSERT_EQUAL(32, w);
  TEST_ASSERT_EQUAL(16, h);
  TEST_ASSERT_EQUAL_STRING("cat on a mat", name.c_str());
  TEST_ASSERT_TRUE(uploads::parseFileName("3-64x64-a-b-c.gif", id, w, h, name)); // dashes in the name
  TEST_ASSERT_EQUAL_STRING("a-b-c", name.c_str());
  TEST_ASSERT_FALSE(uploads::parseFileName("12.part", id, w, h, name));
  TEST_ASSERT_FALSE(uploads::parseFileName("cat.gif", id, w, h, name));
  TEST_ASSERT_FALSE(uploads::parseFileName("0-8x8-x.gif", id, w, h, name));
  TEST_ASSERT_FALSE(uploads::parseFileName("5-99x8-x.gif", id, w, h, name)); // bigger than the panel
  TEST_ASSERT_FALSE(uploads::parseFileName("5-8x8-.gif", id, w, h, name));
}

static void test_upload_decodes_percent_names() {
  TEST_ASSERT_EQUAL_STRING("cat on a mat.gif", uploads::percentDecode("cat%20on%20a%20mat.gif").c_str());
  TEST_ASSERT_EQUAL_STRING("100%", uploads::percentDecode("100%").c_str());
  TEST_ASSERT_EQUAL_STRING("a%zzb", uploads::percentDecode("a%zzb").c_str());
}

static void test_channels_uploads() {
  channels::Channel channel;
  std::string error;
  TEST_ASSERT_TRUE(channels::parse("uploads", channel, error));
  TEST_ASSERT_TRUE(channel.kind == channels::Kind::Uploads);
  TEST_ASSERT_EQUAL_STRING("uploads", channels::format(channel).c_str());
  TEST_ASSERT_EQUAL_STRING("My uploads", channels::label(channel).c_str());
  TEST_ASSERT_FALSE(channels::parse("uploads:x", channel, error));
  TEST_ASSERT_TRUE(channels::isLocal(channel.kind));
}

static void test_channels_starter() {
  channels::Channel channel;
  std::string error;
  TEST_ASSERT_TRUE(channels::parse("starter", channel, error));
  TEST_ASSERT_TRUE(channel.kind == channels::Kind::Starter);
  TEST_ASSERT_EQUAL_STRING("starter", channels::format(channel).c_str());
  TEST_ASSERT_EQUAL_STRING("Starter art", channels::label(channel).c_str());
  TEST_ASSERT_EQUAL_STRING("all", channels::makapixName(channel.kind));
  TEST_ASSERT_TRUE(channels::isLocal(channel.kind));
  TEST_ASSERT_TRUE(channels::parse("promoted", channel, error));
  TEST_ASSERT_FALSE(channels::isLocal(channel.kind));
}

// ---- starter_pack -------------------------------------------------------------

// A pack as tools/stage_starter_art.py writes it, with GIFs of the given bodies.
static std::vector<uint8_t> makePack(const std::vector<std::string> &gifs) {
// Each piece's still is "GIF89s".
  const std::string still = "GIF89s";
  std::vector<uint8_t> index = {'P', 'J', 'S', 'A', 2, (uint8_t)gifs.size()};
  size_t indexSize = 6;
  for (size_t i = 0; i < gifs.size(); i++) {
    indexSize += 16 + 1 + 5 + 1 + 3; // title "Piece", credit "Kim"
  }
  uint32_t offset = (uint32_t)indexSize;
  auto u32 = [&index](uint32_t value) {
    for (int b = 0; b < 4; b++) index.push_back((uint8_t)(value >> (8 * b)));
  };
  for (const std::string &gif : gifs) {
    u32(offset);
    u32((uint32_t)gif.size());
    u32(offset + (uint32_t)gif.size());
    u32((uint32_t)still.size());
    index.push_back(5);
    for (char c : std::string("Piece")) index.push_back((uint8_t)c);
    index.push_back(3);
    for (char c : std::string("Kim")) index.push_back((uint8_t)c);
    offset += (uint32_t)(gif.size() + still.size());
  }
  for (const std::string &gif : gifs) {
    index.insert(index.end(), gif.begin(), gif.end());
    index.insert(index.end(), still.begin(), still.end());
  }
  return index;
}

static void test_starter_pack_reads_pieces() {
  std::vector<uint8_t> pack = makePack({"GIF89a-one", "GIF89a-second"});
  std::vector<starterpack::Piece> pieces;
  TEST_ASSERT_TRUE(starterpack::parse(pack.data(), pack.size(), pieces));
  TEST_ASSERT_EQUAL(2, (int)pieces.size());
  TEST_ASSERT_EQUAL_STRING("Piece", pieces[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Kim", pieces[1].credit.c_str());
  TEST_ASSERT_EQUAL(10, (int)pieces[0].size);
  TEST_ASSERT_EQUAL(0, memcmp(pack.data() + pieces[1].offset, "GIF89a-second", 13));
  TEST_ASSERT_EQUAL(6, (int)pieces[1].stillSize);
  TEST_ASSERT_EQUAL(0, memcmp(pack.data() + pieces[1].stillOffset, "GIF89s", 6));
}

static void test_starter_pack_refuses_damage() {
  std::vector<starterpack::Piece> pieces;
  std::vector<uint8_t> pack = makePack({"GIF89a-one"});
  TEST_ASSERT_FALSE(starterpack::parse(pack.data(), pack.size() - 1, pieces)); // cut short
  TEST_ASSERT_TRUE(pieces.empty());
  std::vector<uint8_t> wrong = pack;
  wrong[4] = 3; // a later version
  TEST_ASSERT_FALSE(starterpack::parse(wrong.data(), wrong.size(), pieces));
  wrong = pack;
  wrong[wrong.size() - 16] = 'X'; // the piece is not a GIF
  TEST_ASSERT_FALSE(starterpack::parse(wrong.data(), wrong.size(), pieces));
  wrong = pack;
  wrong[wrong.size() - 6] = 'X'; // nor is its still
  TEST_ASSERT_FALSE(starterpack::parse(wrong.data(), wrong.size(), pieces));
  wrong = pack;
  wrong[1] = 'X'; // not a pack
  TEST_ASSERT_FALSE(starterpack::parse(wrong.data(), wrong.size(), pieces));
  wrong = pack;
  wrong[6] = 0; // offset 0: over the index
  TEST_ASSERT_FALSE(starterpack::parse(wrong.data(), wrong.size(), pieces));
  TEST_ASSERT_FALSE(starterpack::parse(nullptr, 0, pieces));
  std::vector<uint8_t> empty = makePack({});
  TEST_ASSERT_TRUE(starterpack::parse(empty.data(), empty.size(), pieces));
  TEST_ASSERT_TRUE(pieces.empty());
}

// ---- orientation --------------------------------------------------------------

static orientation::Orientation makeOrientation(int turns, bool mirror) {
  orientation::Orientation o;
  o.quarterTurns = (uint8_t)turns;
  o.mirror = mirror;
  return o;
}

// Where the top-left corner and the pixel just right of it end up, on 64 x 64.
static void assertCorner(int turns, bool mirror, int cornerX, int cornerY, int nextX, int nextY) {
  int x, y;
  orientation::map(makeOrientation(turns, mirror), 64, 0, 0, x, y);
  TEST_ASSERT_EQUAL(cornerX, x);
  TEST_ASSERT_EQUAL(cornerY, y);
  orientation::map(makeOrientation(turns, mirror), 64, 1, 0, x, y);
  TEST_ASSERT_EQUAL(nextX, x);
  TEST_ASSERT_EQUAL(nextY, y);
}

static void test_orientation_corners() {
  assertCorner(0, false, 0, 0, 1, 0);    // as drawn
  assertCorner(1, false, 63, 0, 63, 1);  // 90 clockwise: the top row runs down the right
  assertCorner(2, false, 63, 63, 62, 63);
  assertCorner(3, false, 0, 63, 0, 62);
  assertCorner(0, true, 63, 0, 62, 0);   // mirrored: the top row runs right to left
  assertCorner(1, true, 63, 63, 63, 62);
  assertCorner(2, true, 0, 63, 1, 63);
  assertCorner(3, true, 0, 0, 0, 1);
}

// Every one of the eight is a one-to-one mapping onto the panel, and a
// rectangle maps onto exactly the pixels its own pixels map to.
static void test_orientation_maps_every_pixel_once() {
  static const int N = 8;
  for (int turns = 0; turns < 4; turns++) {
    for (int m = 0; m < 2; m++) {
      orientation::Orientation o = makeOrientation(turns, m == 1);
      bool seen[N][N] = {};
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          int px, py;
          orientation::map(o, N, x, y, px, py);
          TEST_ASSERT_TRUE(px >= 0 && px < N && py >= 0 && py < N);
          TEST_ASSERT_FALSE(seen[py][px]);
          seen[py][px] = true;
        }
      }
      int x = 1, y = 2, w = 5, h = 3;
      orientation::mapRect(o, N, x, y, w, h);
      TEST_ASSERT_EQUAL(15, w * h);
      for (int ry = 2; ry < 5; ry++) {
        for (int rx = 1; rx < 6; rx++) {
          int px, py;
          orientation::map(o, N, rx, ry, px, py);
          TEST_ASSERT_TRUE(px >= x && px < x + w && py >= y && py < y + h);
        }
      }
    }
  }
}

static void test_orientation_degrees_and_packing() {
  uint8_t turns = 9;
  TEST_ASSERT_TRUE(orientation::fromDegrees(270, turns));
  TEST_ASSERT_EQUAL(3, turns);
  TEST_ASSERT_TRUE(orientation::fromDegrees(0, turns));
  TEST_ASSERT_EQUAL(0, turns);
  TEST_ASSERT_FALSE(orientation::fromDegrees(45, turns));
  TEST_ASSERT_FALSE(orientation::fromDegrees(360, turns));
  TEST_ASSERT_FALSE(orientation::fromDegrees(-90, turns));
  for (int t = 0; t < 4; t++) {
    for (int m = 0; m < 2; m++) {
      orientation::Orientation o = makeOrientation(t, m == 1);
      TEST_ASSERT_TRUE(orientation::unpack(orientation::pack(o)) == o);
      TEST_ASSERT_EQUAL(t * 90, orientation::toDegrees(o));
    }
  }
}

// ---- weather_rules ------------------------------------------------------------

// Every one of WeatherAPI.com's 60 codes (weather_conditions.json, 2026-10-22).
static void test_weather_every_code_has_a_picture() {
  const int codes[] = {1000, 1003, 1006, 1009, 1012, 1015, 1018, 1021, 1024, 1027, 1030, 1033, 1036, 1039, 1042,
                       1045, 1048, 1063, 1066, 1069, 1072, 1087, 1114, 1117, 1135, 1147, 1150, 1153, 1168, 1171,
                       1180, 1183, 1186, 1189, 1192, 1195, 1198, 1201, 1204, 1207, 1210, 1213, 1216, 1219, 1222,
                       1225, 1237, 1240, 1243, 1246, 1249, 1252, 1255, 1258, 1261, 1264, 1273, 1276, 1279, 1282};
  TEST_ASSERT_EQUAL(60, (int)(sizeof(codes) / sizeof(codes[0])));
  int counts[(int)weather::Icon::COUNT] = {};
  for (int code : codes) {
    counts[(int)weather::iconFor(code)]++;
  }
  for (int i = 0; i < (int)weather::Icon::COUNT; i++) {
    TEST_ASSERT_TRUE(counts[i] > 0); // every picture is used
  }
  TEST_ASSERT_TRUE(weather::iconFor(1000) == weather::Icon::Clear);
  TEST_ASSERT_TRUE(weather::iconFor(1003) == weather::Icon::PartlyCloudy);
  TEST_ASSERT_TRUE(weather::iconFor(1033) == weather::Icon::Haze);     // smoke
  TEST_ASSERT_TRUE(weather::iconFor(1030) == weather::Icon::Fog);      // mist
  TEST_ASSERT_TRUE(weather::iconFor(1153) == weather::Icon::Drizzle);
  TEST_ASSERT_TRUE(weather::iconFor(1246) == weather::Icon::Rain);     // torrential shower
  TEST_ASSERT_TRUE(weather::iconFor(1117) == weather::Icon::Snow);     // blizzard
  TEST_ASSERT_TRUE(weather::iconFor(1198) == weather::Icon::Sleet);    // freezing rain
  TEST_ASSERT_TRUE(weather::iconFor(1087) == weather::Icon::Storm);
  TEST_ASSERT_TRUE(weather::iconFor(9999) == weather::Icon::Cloudy);   // unknown
  TEST_ASSERT_EQUAL_STRING("partly-cloudy", weather::iconName(weather::Icon::PartlyCloudy));
  TEST_ASSERT_TRUE(weather::hasNightPicture(weather::Icon::Clear));
  TEST_ASSERT_FALSE(weather::hasNightPicture(weather::Icon::Rain));
}

static void test_weather_fetch_timing() {
  const uint32_t MIN = 60000;
  TEST_ASSERT_TRUE(weather::fetchDue(false, 1000, 0, 0, 30));                 // nothing yet
  TEST_ASSERT_FALSE(weather::fetchDue(false, 29 * MIN, 1, 0, 30));            // still fresh
  TEST_ASSERT_TRUE(weather::fetchDue(false, 30 * MIN + 1, 1, 0, 30));         // due
  TEST_ASSERT_TRUE(weather::fetchDue(true, 2 * MIN, 1 * MIN, 0, 30));         // place changed
  TEST_ASSERT_FALSE(weather::fetchDue(false, 10 * MIN, 0, 12 * MIN, 30));     // waiting after a failure
  TEST_ASSERT_TRUE(weather::fetchDue(false, 12 * MIN, 0, 12 * MIN, 30));
  TEST_ASSERT_TRUE(weather::fetchDue(true, 10 * MIN, 0, 12 * MIN, 30));       // a change does not wait
  // Across the millisecond clock wrapping, about every 49.7 days.
  TEST_ASSERT_FALSE(weather::fetchDue(false, 5 * MIN, 0xFFFFFFFFu - MIN, 0, 30));
  TEST_ASSERT_TRUE(weather::fetchDue(false, 30 * MIN, 0xFFFFFFFFu - MIN, 0, 30));
}

static void test_weather_freshness_and_settings() {
  const uint32_t MIN = 60000;
  TEST_ASSERT_TRUE(weather::freshness(10 * MIN, 30) == weather::Freshness::Fresh);
  TEST_ASSERT_TRUE(weather::freshness(34 * MIN, 30) == weather::Freshness::Fresh);
  TEST_ASSERT_TRUE(weather::freshness(35 * MIN, 30) == weather::Freshness::Aged);
  TEST_ASSERT_TRUE(weather::freshness(60 * MIN, 30) == weather::Freshness::Aged);
  TEST_ASSERT_TRUE(weather::freshness(60 * MIN + 1, 30) == weather::Freshness::Expired);
  TEST_ASSERT_TRUE(weather::freshness(59 * MIN, 60) == weather::Freshness::Fresh); // never aged before expiry
  TEST_ASSERT_FALSE(weather::refreshValid(14));
  TEST_ASSERT_TRUE(weather::refreshValid(15));
  TEST_ASSERT_TRUE(weather::refreshValid(60));
  TEST_ASSERT_FALSE(weather::refreshValid(61));
  TEST_ASSERT_EQUAL(72, weather::roundTemperature(71.5f));
  TEST_ASSERT_EQUAL(-3, weather::roundTemperature(-2.6f));
  TEST_ASSERT_EQUAL(0, weather::roundTemperature(-0.4f));
}

// ---- test_patterns ------------------------------------------------------------

static void test_pattern_names_round_trip() {
  for (int i = 0; i < testpattern::COUNT; i++) {
    testpattern::Pattern pattern;
    TEST_ASSERT_TRUE(testpattern::fromName(testpattern::name((testpattern::Pattern)i), pattern));
    TEST_ASSERT_EQUAL(i, (int)pattern);
  }
  testpattern::Pattern pattern;
  TEST_ASSERT_FALSE(testpattern::fromName("nope", pattern));
}

static void test_pattern_dark_steps_climb() {
  uint8_t r, g, b, last = 0;
  // Along the top half and on along the bottom half: 32 steps, each brighter.
  for (int i = 0; i < 32; i++) {
    int x = (i % 16) * 4 + 1, y = i < 16 ? 5 : 50;
    testpattern::colour(testpattern::Pattern::Darks, x, y, r, g, b);
    TEST_ASSERT_TRUE(r == g && g == b);
    if (i > 0) {
      TEST_ASSERT_EQUAL(last + 8, r); // one 5-bit level apart: exact in RGB565
    }
    last = r;
  }
  testpattern::colour(testpattern::Pattern::Darks, 10, 31, r, g, b);
  TEST_ASSERT_EQUAL(0, r); // the dark row between the halves
}

static void test_pattern_bars_and_ramps() {
  uint8_t r, g, b;
  testpattern::colour(testpattern::Pattern::Bars, 0, 0, r, g, b);
  TEST_ASSERT_TRUE(r == 255 && g == 255 && b == 255); // white
  testpattern::colour(testpattern::Pattern::Bars, 8 * 2 + 3, 40, r, g, b);
  TEST_ASSERT_TRUE(r == 0 && g == 255 && b == 255); // cyan
  testpattern::colour(testpattern::Pattern::Bars, 8 * 5, 0, r, g, b);
  TEST_ASSERT_TRUE(r == 255 && g == 0 && b == 0); // red
  testpattern::colour(testpattern::Pattern::Bars, 63, 63, r, g, b);
  TEST_ASSERT_TRUE(r == 0 && g == 0 && b == 0); // black
  testpattern::colour(testpattern::Pattern::Ramps, 63, 20, r, g, b);
  TEST_ASSERT_TRUE(r == 248 && g == 0 && b == 0); // the red band, brightest
  testpattern::colour(testpattern::Pattern::Ramps, 0, 0, r, g, b);
  TEST_ASSERT_TRUE(r == 0 && g == 0 && b == 0);
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
  RUN_TEST(test_rotation_pausing_keeps_entries_but_needs_one_on);

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
  RUN_TEST(test_views_remember_their_channel);
  RUN_TEST(test_views_damaged_lines_are_refused);
  RUN_TEST(test_views_times_are_iso_utc);

  RUN_TEST(test_rtc_encodes_a_known_time);
  RUN_TEST(test_rtc_round_trips);
  RUN_TEST(test_rtc_refuses_a_stopped_oscillator);
  RUN_TEST(test_rtc_refuses_impossible_values);
  RUN_TEST(test_rtc_clamps_to_the_chips_century);
  RUN_TEST(test_rtc_dates_and_days_agree);

  RUN_TEST(test_crash_causes_have_their_idf_names);
  RUN_TEST(test_crash_unknown_causes_are_named_so);
  RUN_TEST(test_crash_hints_cover_the_common_causes);

  RUN_TEST(test_channels_simple_ones_parse);
  RUN_TEST(test_channels_names_and_tags_parse_and_tidy);
  RUN_TEST(test_channels_names_in_any_script_are_counted_in_characters);
  RUN_TEST(test_channels_reject_what_is_not_a_channel);
  RUN_TEST(test_channels_a_refused_parse_changes_nothing);

  RUN_TEST(test_night_times_parse_and_format);
  RUN_TEST(test_night_wraps_past_midnight);
  RUN_TEST(test_night_within_one_day_works_too);
  RUN_TEST(test_night_off_or_empty_is_never_night);
  RUN_TEST(test_night_fades_in_and_out);
  RUN_TEST(test_night_blend_goes_from_day_to_night);
  RUN_TEST(test_night_next_switch);
  RUN_TEST(test_night_actions_and_limits);

  RUN_TEST(test_shtc3_checksum_matches_the_datasheet);
  RUN_TEST(test_shtc3_decodes_a_measurement);
  RUN_TEST(test_shtc3_refuses_a_damaged_measurement);
  RUN_TEST(test_shtc3_recognises_its_id);
  RUN_TEST(test_pin_accepts_four_to_eight_digits);
  RUN_TEST(test_pin_locks_after_five_wrong);
  RUN_TEST(test_pin_right_one_resets_the_count);
  RUN_TEST(test_pin_lockout_survives_the_clock_wrapping);
  RUN_TEST(test_pin_sessions_create_and_end);
  RUN_TEST(test_pin_ninth_session_replaces_the_least_recent);
  RUN_TEST(test_pin_sessions_save_and_restore);
  RUN_TEST(test_pin_reads_cookies);
  RUN_TEST(test_pin_compares_bytes);
  RUN_TEST(test_screen_order_of_who_wins);
  RUN_TEST(test_screen_giving_way);
  RUN_TEST(test_screen_what_next_and_hold_mean);
  RUN_TEST(test_screen_history_walks_back);
  RUN_TEST(test_screen_history_keeps_the_latest);
  RUN_TEST(test_channels_from_makapix_commands);
  RUN_TEST(test_live_backs_off_to_five_minutes);
  RUN_TEST(test_live_counts_days_to_expiry);
  RUN_TEST(test_live_renews_when_due_or_refused);
  RUN_TEST(test_beat_steady_loud_music_is_not_a_beat);
  RUN_TEST(test_beat_a_kick_stands_out_and_fades);
  RUN_TEST(test_beat_small_wobbles_are_ignored);
  RUN_TEST(test_beat_repeated_beats_each_show);
  RUN_TEST(test_upload_reads_gif_size);
  RUN_TEST(test_upload_size_rules);
  RUN_TEST(test_upload_tidies_names);
  RUN_TEST(test_upload_file_names_round_trip);
  RUN_TEST(test_upload_decodes_percent_names);
  RUN_TEST(test_channels_uploads);
  RUN_TEST(test_channels_starter);
  RUN_TEST(test_weather_every_code_has_a_picture);
  RUN_TEST(test_weather_fetch_timing);
  RUN_TEST(test_weather_freshness_and_settings);
  RUN_TEST(test_cache_index_keeps_the_storage_key);
  RUN_TEST(test_orientation_corners);
  RUN_TEST(test_orientation_maps_every_pixel_once);
  RUN_TEST(test_orientation_degrees_and_packing);
  RUN_TEST(test_starter_pack_reads_pieces);
  RUN_TEST(test_starter_pack_refuses_damage);
  RUN_TEST(test_pattern_names_round_trip);
  RUN_TEST(test_pattern_dark_steps_climb);
  RUN_TEST(test_pattern_bars_and_ramps);
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
