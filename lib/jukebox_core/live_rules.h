#pragma once

#include <stdint.h>

// The live connection's timing rules (network/live.cpp), apart from the
// network so they can be tested.

namespace live {

// Seconds to wait after `failures` failed connections in a row: 1, 2, 4 ...
// up to 5 minutes, as Makapix's MQTT guide asks.
uint32_t retryDelaySeconds(int failures);

// Whole days from `nowUnix` (UTC) to midnight UTC at the start of a calendar
// date; negative once it has passed.
long daysUntil(int year, int month, int day, int64_t nowUnix);

// Renew the client certificate when it has 30 days or less left, or when the
// broker has turned the panel away three times in a row (it may have expired,
// or the CA changed), but not more than once a day: Makapix allows ten a day.
static const long RENEW_WITHIN_DAYS = 30;
static const int RENEW_AFTER_FAILURES = 3;
bool shouldRenew(long daysLeft, bool daysKnown, int handshakeFailures, bool renewedToday);

} // namespace live
