#include "logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {

constexpr size_t kRingSize = 6144;
constexpr size_t kLineBufSize = 256;

char ringBuf[kRingSize];
size_t ringHead = 0; // Next write position
size_t ringUsed = 0; // Valid bytes currently in ringBuf (<= kRingSize)
uint32_t ringTotalBytes = 0;
SemaphoreHandle_t logMutex = nullptr;

void writeRawToRing(const char* data, size_t len) {
  if (len == 0) {
    return;
  }
  if (len >= kRingSize) {
    data += (len - kRingSize);
    len = kRingSize;
  }
  size_t firstPart = kRingSize - ringHead;
  if (firstPart > len) {
    firstPart = len;
  }
  memcpy(ringBuf + ringHead, data, firstPart);
  size_t secondPart = len - firstPart;
  if (secondPart > 0) {
    memcpy(ringBuf, data + firstPart, secondPart);
  }
  ringHead = (ringHead + len) % kRingSize;
  ringUsed += len;
  if (ringUsed > kRingSize) {
    ringUsed = kRingSize;
  }
  ringTotalBytes += static_cast<uint32_t>(len);
}

void formatTimestamp(char* out, size_t outSize) {
  time_t nowSec = time(nullptr);
  if (nowSec > 1600000000) {
    struct tm ti;
    localtime_r(&nowSec, &ti);
    snprintf(out, outSize, "[%02d:%02d:%02d] ", ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    unsigned long sec = millis() / 1000UL;
    snprintf(out, outSize, "[+%lus] ", sec);
  }
}

void appendMessage(const char* rawMsg) {
  if (!rawMsg) {
    return;
  }

  // Strip leading CR/LF so every ring entry starts cleanly with a timestamp
  while (*rawMsg == '\r' || *rawMsg == '\n') {
    ++rawMsg;
  }
  size_t msgLen = strlen(rawMsg);
  while (msgLen > 0 && (rawMsg[msgLen - 1] == '\r' || rawMsg[msgLen - 1] == '\n')) {
    --msgLen;
  }
  if (msgLen == 0) {
    return;
  }

  char ts[20];
  formatTimestamp(ts, sizeof(ts));
  size_t tsLen = strlen(ts);

  char line[kLineBufSize];
  size_t maxMsg = (sizeof(line) - 2 > tsLen) ? (sizeof(line) - 2 - tsLen) : 0;
  if (msgLen > maxMsg) {
    msgLen = maxMsg;
  }
  memcpy(line, ts, tsLen);
  memcpy(line + tsLen, rawMsg, msgLen);
  size_t totalLen = tsLen + msgLen;
  line[totalLen++] = '\n';
  line[totalLen] = '\0';

  if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    writeRawToRing(line, totalLen);
    xSemaphoreGive(logMutex);
  }

  Serial.write(reinterpret_cast<const uint8_t*>(line), totalLen);
}

} // namespace

namespace Log {

void init() {
  if (!logMutex) {
    logMutex = xSemaphoreCreateMutex();
  }
}

void printf(const char* fmt, ...) {
  char buf[kLineBufSize];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  appendMessage(buf);
}

void println(const char* msg) {
  appendMessage(msg);
}

uint32_t readSince(uint32_t since, String& out, bool& truncated) {
  out = "";
  truncated = false;

  if (!logMutex || xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return ringTotalBytes;
  }

  uint32_t latest = ringTotalBytes;
  uint32_t oldest = latest - static_cast<uint32_t>(ringUsed);

  if (since > latest) {
    // Client has a stale cursor (e.g., ESP32 rebooted)
    since = oldest;
    truncated = true;
  } else if (since < oldest) {
    if (since > 0) {
      truncated = true;
    }
    since = oldest;
  }

  size_t avail = static_cast<size_t>(latest - since);
  if (avail > 0) {
    size_t startIdx = (ringHead + kRingSize - avail) % kRingSize;

    // If we are starting from the oldest byte of a wrapped buffer, skip any partial first line
    if (since == oldest && latest > kRingSize) {
      size_t skip = 0;
      while (skip < avail) {
        char c = ringBuf[(startIdx + skip) % kRingSize];
        ++skip;
        if (c == '\n') {
          break;
        }
      }
      if (skip < avail) {
        startIdx = (startIdx + skip) % kRingSize;
        avail -= skip;
      }
    }

    out.reserve(avail);
    size_t firstPart = kRingSize - startIdx;
    if (firstPart > avail) {
      firstPart = avail;
    }
    out.concat(ringBuf + startIdx, firstPart);
    size_t secondPart = avail - firstPart;
    if (secondPart > 0) {
      out.concat(ringBuf, secondPart);
    }
  }

  xSemaphoreGive(logMutex);
  return latest;
}

void clear() {
  if (logMutex && xSemaphoreTake(logMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ringUsed = 0;
    xSemaphoreGive(logMutex);
  }
}

} // namespace Log
