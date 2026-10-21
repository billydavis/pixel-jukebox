#pragma once

// The pages in web/, embedded into the firmware at build time by the
// board_build.embed_txtfiles setting in platformio.ini, from the copies that
// tools/stage_web.py puts in .pio/embed/. Each is a
// null-terminated string. Nothing is fetched from outside the board.

extern const char WEB_INDEX_HTML[] asm("_binary__pio_embed_index_html_start");             // control page
extern const char WEB_SETUP_HTML[] asm("_binary__pio_embed_setup_html_start");             // Wi-Fi setup form
extern const char WEB_SETUP_CHECK_HTML[] asm("_binary__pio_embed_setup_check_html_start"); // "checking the password"
extern const char WEB_SETUP_DONE_HTML[] asm("_binary__pio_embed_setup_done_html_start");   // "connected"
extern const char WEB_SETUP_CSS[] asm("_binary__pio_embed_setup_css_start");               // shared by the setup pages

// The logo as a 64 x 64 favicon (web/favicon.png), embedded as it is rather
// than as a string, so it has an end instead of a terminating zero.
extern const uint8_t WEB_FAVICON_PNG[] asm("_binary__pio_embed_favicon_png_start");
extern const uint8_t WEB_FAVICON_PNG_END[] asm("_binary__pio_embed_favicon_png_end");

// Sends the favicon. It only changes with the firmware, so browsers may keep it a day.
template <typename Server> void sendFavicon(Server &server) {
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/png", (const char *)WEB_FAVICON_PNG, WEB_FAVICON_PNG_END - WEB_FAVICON_PNG);
}
