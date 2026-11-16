#pragma once

#include "config.h"

namespace Storage {
  void load(AppConfig& cfg);
  void save(const AppConfig& cfg);
  void reset();
}
