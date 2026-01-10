#pragma once

#include <cstddef>

namespace AudioConstants {

constexpr float DEFAULT_VOLUME = 1.0F;
constexpr float MIN_VOLUME = 0.0F;
constexpr float MAX_VOLUME = 1.0F;

constexpr int DEFAULT_PRIORITY = 0;

constexpr size_t DEFAULT_MAX_CHANNELS = 32;
constexpr size_t MIN_CHANNELS = 1;

constexpr int DEFAULT_MUSIC_CHANNELS = 4;
constexpr int DEFAULT_SAMPLE_RATE = 48000;
constexpr int DEFAULT_OUTPUT_CHANNELS = 2;
constexpr int DEFAULT_FADE_IN_MS = 250;
constexpr int DEFAULT_FADE_OUT_MS = 150;
constexpr int NO_FADE_MS = 0;
} // namespace AudioConstants
