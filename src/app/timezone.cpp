#include "app/timezone.h"

#include <Preferences.h>
#include <time.h>

#include "timezones.h"

static Preferences prefs;
static volatile int wanted = 0; // an int is written in one go: safe across tasks

void timezoneBegin() {
  prefs.begin("clock", false);
  int index = timezones::find(prefs.getString("tz", "").c_str());
  wanted = index >= 0 ? index : timezones::defaultIndex();
  timezoneApply();
}

int timezoneIndex() { return wanted; }

const char *timezonePosix() { return timezones::at(wanted).posix; }

bool timezoneSave(int index) {
  if (index < 0 || (size_t)index >= timezones::count()) {
    return false;
  }
  wanted = index;
  prefs.putString("tz", timezones::at(index).name);
  return true;
}

void timezoneApply() {
  setenv("TZ", timezonePosix(), 1);
  tzset();
  Serial.printf("Time zone: %s\n", timezones::at(wanted).name);
}
