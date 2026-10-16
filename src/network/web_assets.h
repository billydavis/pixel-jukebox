#pragma once

// The pages in web/, embedded into the firmware at build time by the
// board_build.embed_txtfiles setting in platformio.ini, from the copies that
// tools/stage_web.py puts in .pio/embed/. Each is a
// null-terminated string. Nothing is fetched from outside the board.

extern const char WEB_INDEX_HTML[] asm("_binary__pio_embed_index_html_start");             // control page
extern const char WEB_GALLERY_HTML[] asm("_binary__pio_embed_gallery_html_start");         // saved art on the card
extern const char WEB_SETUP_HTML[] asm("_binary__pio_embed_setup_html_start");             // Wi-Fi setup form
extern const char WEB_SETUP_CHECK_HTML[] asm("_binary__pio_embed_setup_check_html_start"); // "checking the password"
extern const char WEB_SETUP_DONE_HTML[] asm("_binary__pio_embed_setup_done_html_start");   // "connected"
extern const char WEB_SETUP_CSS[] asm("_binary__pio_embed_setup_css_start");               // shared by the setup pages
