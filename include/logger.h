#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace Log {

void init();
void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void println(const char* msg);

// Appends buffered log bytes written after `since` into `out` and returns the
// latest monotonic byte cursor. Sets `truncated` to true if `since > 0` and
// older unread bytes were overwritten by the ring buffer.
uint32_t readSince(uint32_t since, String& out, bool& truncated);

void clear();

} // namespace Log
