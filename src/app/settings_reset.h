#pragma once

// Reset all settings (backlog #41): every setting back to its default, for a
// fresh start or to try what a new panel does. Clears the rotation, the clock
// and time zone, the weather (its key too), the night schedule, the display
// options and brightness, the transition, view reports and the live switch,
// the PIN, the Makapix link (the panel registers again, with a new code) and
// the likes and views still waiting to be sent.
//
// Kept: the Wi-Fi details, so the panel comes back on the same network (its
// own reset is the setup hotspot: hold BOOT); everything on the card; and the
// crash record, which is a diagnostic rather than a setting.
//
// Restart straight after: the running firmware still holds the old settings.
void settingsResetAll();
