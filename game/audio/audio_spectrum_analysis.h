#pragma once

#include <cstddef>

#include "audio_mastering.h"

namespace Game::Audio::Mastering {

void analyse_spectrum(Analysis& analysis,
                      const float* pcm,
                      std::size_t frames,
                      std::size_t channels,
                      std::size_t active_channels,
                      int sample_rate);

} // namespace Game::Audio::Mastering
