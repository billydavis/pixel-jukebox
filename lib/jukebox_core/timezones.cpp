#include "timezones.h"

#include <string.h>

namespace timezones {

// West to east. Rules from posix_tz_db zones.csv (2026-10).
static const Zone ZONES[] = {
    {"Pacific/Honolulu", "Hawaii (Honolulu)", "HST10"},
    {"America/Anchorage", "Alaska (Anchorage)", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "US Pacific (Los Angeles)", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "Arizona (Phoenix)", "MST7"},
    {"America/Denver", "US Mountain (Denver)", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "US Central (Chicago)", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "Mexico City", "CST6"},
    {"America/New_York", "US Eastern (New York)", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Bogota", "Bogota, Lima", "<-05>5"},
    {"America/Halifax", "Atlantic (Halifax)", "AST4ADT,M3.2.0,M11.1.0"},
    {"America/St_Johns", "Newfoundland (St. John's)", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"America/Sao_Paulo", "Sao Paulo", "<-03>3"},
    {"America/Argentina/Buenos_Aires", "Buenos Aires", "<-03>3"},
    {"Atlantic/Azores", "Azores", "<-01>1<+00>,M3.5.0/0,M10.5.0/1"},
    {"Etc/UTC", "UTC", "UTC0"},
    {"Atlantic/Reykjavik", "Reykjavik", "GMT0"},
    {"Europe/London", "UK (London)", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Dublin", "Ireland (Dublin)", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"Europe/Lisbon", "Portugal (Lisbon)", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Africa/Lagos", "West Africa (Lagos)", "WAT-1"},
    {"Europe/Paris", "Central Europe (Paris, Berlin, Rome, Madrid)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Athens", "Eastern Europe (Athens, Helsinki, Kyiv)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Africa/Cairo", "Egypt (Cairo)", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"Africa/Johannesburg", "South Africa (Johannesburg)", "SAST-2"},
    {"Asia/Jerusalem", "Israel (Jerusalem)", "IST-2IDT,M3.4.4/26,M10.5.0"},
    {"Europe/Istanbul", "Turkey (Istanbul)", "<+03>-3"},
    {"Europe/Moscow", "Moscow", "MSK-3"},
    {"Africa/Nairobi", "East Africa (Nairobi)", "EAT-3"},
    {"Asia/Riyadh", "Arabia (Riyadh)", "<+03>-3"},
    {"Asia/Tehran", "Iran (Tehran)", "<+0330>-3:30"},
    {"Asia/Dubai", "Gulf (Dubai)", "<+04>-4"},
    {"Asia/Karachi", "Pakistan (Karachi)", "PKT-5"},
    {"Asia/Kolkata", "India (Kolkata)", "IST-5:30"},
    {"Asia/Kathmandu", "Nepal (Kathmandu)", "<+0545>-5:45"},
    {"Asia/Dhaka", "Bangladesh (Dhaka)", "<+06>-6"},
    {"Asia/Bangkok", "Indochina (Bangkok, Jakarta)", "<+07>-7"},
    {"Asia/Shanghai", "China (Shanghai)", "CST-8"},
    {"Asia/Hong_Kong", "Hong Kong", "HKT-8"},
    {"Asia/Singapore", "Singapore, Kuala Lumpur", "<+08>-8"},
    {"Asia/Manila", "Philippines (Manila)", "PST-8"},
    {"Australia/Perth", "Western Australia (Perth)", "AWST-8"},
    {"Asia/Tokyo", "Japan (Tokyo)", "JST-9"},
    {"Asia/Seoul", "Korea (Seoul)", "KST-9"},
    {"Australia/Darwin", "Northern Territory (Darwin)", "ACST-9:30"},
    {"Australia/Adelaide", "South Australia (Adelaide)", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane", "Queensland (Brisbane)", "AEST-10"},
    {"Australia/Sydney", "Eastern Australia (Sydney, Melbourne)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "New Zealand (Auckland)", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};

size_t count() { return sizeof(ZONES) / sizeof(ZONES[0]); }

const Zone &at(size_t index) { return ZONES[index < count() ? index : defaultIndex()]; }

int find(const char *name) {
  for (size_t i = 0; name && i < count(); i++) {
    if (strcmp(ZONES[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int defaultIndex() { return find("America/Chicago"); }

} // namespace timezones
