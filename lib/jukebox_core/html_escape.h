#pragma once

#include <string>

// Makes text safe to place in HTML, both between tags and inside a quoted
// attribute. Used for Wi-Fi network names, which anyone nearby can choose.
std::string htmlEscape(const std::string &text);
