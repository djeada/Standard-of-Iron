#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Game::Audio::Mastering {

enum class Material {
  Music,
  Ambience,
  Voice,
  Interface,
  Effect
};

inline constexpr std::size_t MAX_NOTCHES = 4;

struct Notch {
  float frequency_hz = 0.0F;
  float q = 1.0F;
  float gain_db = 0.0F;
};

struct Analysis {
  float peak = 0.0F;
  float loudness_lufs = -70.0F;
  float presence_band_db = -120.0F;
  float air_band_db = -120.0F;
  float presence_peak_db = -120.0F;
  float air_peak_db = -120.0F;
  float body_spectrum_db = -120.0F;
  float presence_spectrum_db = -120.0F;
  float air_spectrum_db = -120.0F;
  int spectral_frames = 0;
  bool channels_identical = false;
  std::size_t notch_count = 0;
  Notch notches[MAX_NOTCHES]{};
};

struct Profile {
  bool normalise_loudness = false;
  bool detect_resonances = false;
  float target_lufs = -15.0F;
  float loudness_authority_db = 0.0F;
  float presence_target_db = -8.0F;
  float air_target_db = -16.0F;
  float tilt_cut_db = 2.5F;
  float tilt_boost_db = 0.0F;
  float harshness_depth_db = 4.0F;
  float harshness_threshold_db = 7.0F;
  float ceiling_db = -1.0F;
};

struct Report {
  float input_peak_db = 0.0F;
  float output_peak_db = 0.0F;
  float input_lufs = -70.0F;
  float output_lufs = -70.0F;
  float loudness_gain_db = 0.0F;
  float presence_tilt_db = 0.0F;
  float air_tilt_db = 0.0F;
  float limiter_reduction_db = 0.0F;
  std::size_t notch_count = 0;
};

auto profile_for(Material material) -> Profile;

auto effect_material(const std::string& resource_id) -> Material;

auto analyse(const float* pcm,
             std::size_t frames,
             int channels,
             int sample_rate) -> Analysis;

auto apply(float* pcm,
           std::size_t frames,
           int channels,
           int sample_rate,
           const Profile& profile,
           const Analysis& analysis) -> Report;

auto restore(std::vector<float>& interleaved_pcm,
             int channels,
             int sample_rate,
             Material material) -> Report;

} // namespace Game::Audio::Mastering
