#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DEVICE_IO
#define MA_ENABLE_MP3
#define MA_ENABLE_FLAC
#define MA_ENABLE_VORBIS
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#pragma push_macro("L")
#pragma push_macro("C")
#pragma push_macro("R")
#include <stb_vorbis.h>
#pragma pop_macro("R")
#pragma pop_macro("C")
#pragma pop_macro("L")
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <miniaudio.h>
#include <string>
#include <vector>

#include "game/audio/audio_mastering.h"
#include "game/audio/bus_limiter.h"
#include "game/audio/gameplay_mix.h"

namespace {

namespace Mastering = Game::Audio::Mastering;
namespace fs = std::filesystem;

constexpr int SAMPLE_RATE = 48000;
constexpr int CHANNELS = 2;

auto classify(const fs::path& path) -> Mastering::Material {
  const std::string text = path.generic_string();
  if (text.find("/music/") != std::string::npos) {
    return Mastering::Material::Music;
  }
  if (text.find("/ambience/") != std::string::npos) {
    return Mastering::Material::Ambience;
  }
  if (text.find("/voices/") != std::string::npos) {
    return Mastering::Material::Voice;
  }
  if (text.find("/ui/") != std::string::npos) {
    return Mastering::Material::Interface;
  }
  return Mastering::Material::Effect;
}

auto material_name(Mastering::Material material) -> const char* {
  switch (material) {
  case Mastering::Material::Music:
    return "music";
  case Mastering::Material::Ambience:
    return "ambience";
  case Mastering::Material::Voice:
    return "voice";
  case Mastering::Material::Interface:
    return "interface";
  case Mastering::Material::Effect:
    return "effect";
  }
  return "effect";
}

auto decode(const fs::path& path, std::vector<float>& pcm) -> bool {
  ma_decoder_config config =
      ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);
  ma_decoder decoder;
  if (ma_decoder_init_file(path.string().c_str(), &config, &decoder) != MA_SUCCESS) {
    return false;
  }
  ma_uint64 expected = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &expected) == MA_SUCCESS) {
    pcm.reserve(static_cast<std::size_t>(expected) * CHANNELS);
  }
  std::vector<float> buffer(4096 * CHANNELS);
  for (;;) {
    ma_uint64 read = 0;
    const ma_result result =
        ma_decoder_read_pcm_frames(&decoder, buffer.data(), 4096, &read);
    if (read > 0) {
      pcm.insert(pcm.end(),
                 buffer.begin(),
                 buffer.begin() + static_cast<std::ptrdiff_t>(read * CHANNELS));
    }
    if (result != MA_SUCCESS) {
      break;
    }
  }
  ma_decoder_uninit(&decoder);
  return !pcm.empty();
}

void write_le(std::FILE* file, std::uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) {
    const auto byte = static_cast<unsigned char>((value >> (8 * i)) & 0xFFU);
    std::fputc(byte, file);
  }
}

auto write_wav(const fs::path& path, const std::vector<float>& pcm) -> bool {
  std::FILE* file = std::fopen(path.string().c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  const auto samples = static_cast<std::uint32_t>(pcm.size());
  const std::uint32_t data_bytes = samples * 2;
  std::fwrite("RIFF", 1, 4, file);
  write_le(file, 36 + data_bytes, 4);
  std::fwrite("WAVEfmt ", 1, 8, file);
  write_le(file, 16, 4);
  write_le(file, 1, 2);
  write_le(file, CHANNELS, 2);
  write_le(file, SAMPLE_RATE, 4);
  write_le(file, SAMPLE_RATE * CHANNELS * 2, 4);
  write_le(file, CHANNELS * 2, 2);
  write_le(file, 16, 2);
  std::fwrite("data", 1, 4, file);
  write_le(file, data_bytes, 4);
  for (const float sample : pcm) {
    const float clamped = std::clamp(sample, -1.0F, 1.0F);
    const auto value = static_cast<std::int16_t>(clamped * 32767.0F);
    write_le(file, static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)), 2);
  }
  std::fclose(file);
  return true;
}

auto process(const fs::path& source,
             const fs::path& output_dir,
             bool write_audio,
             const fs::path& render_target) -> bool {
  std::vector<float> pcm;
  if (!decode(source, pcm)) {
    std::fprintf(stderr, "audio_master: cannot decode %s\n", source.string().c_str());
    return false;
  }
  const std::vector<float> original = pcm;
  const std::size_t frames = pcm.size() / CHANNELS;
  const Mastering::Material material = classify(source);

  const auto started = std::chrono::steady_clock::now();
  const Mastering::Analysis analysis =
      Mastering::analyse(pcm.data(), frames, CHANNELS, SAMPLE_RATE);
  const auto analysed = std::chrono::steady_clock::now();
  const Mastering::Report report = Mastering::apply(pcm.data(),
                                                    frames,
                                                    CHANNELS,
                                                    SAMPLE_RATE,
                                                    Mastering::profile_for(material),
                                                    analysis);
  const auto applied = std::chrono::steady_clock::now();

  const auto milliseconds = [](auto from, auto to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
  };

  std::size_t clipped = 0;
  for (const float sample : original) {
    if (std::abs(sample) >= 1.0F) {
      ++clipped;
    }
  }

  std::printf(
      "%s  [%s]\n", source.filename().string().c_str(), material_name(material));
  std::printf("  %6.1f s  %s  peak %+6.2f -> %+6.2f dBFS  clipped in %zu\n",
              double(frames) / double(SAMPLE_RATE),
              analysis.channels_identical ? "mono " : "true ",
              report.input_peak_db,
              report.output_peak_db,
              clipped);
  std::printf("  loudness %+6.2f -> %+6.2f LUFS (gain %+5.2f dB)  limiter %+5.2f dB\n",
              report.input_lufs,
              report.output_lufs,
              report.loudness_gain_db,
              report.limiter_reduction_db);
  std::printf("  tilt presence %+5.2f dB  air %+5.2f dB  notches %zu\n",
              report.presence_tilt_db,
              report.air_tilt_db,
              report.notch_count);
  for (std::size_t i = 0; i < analysis.notch_count; ++i) {
    std::printf("    notch %8.0f Hz  Q %5.1f  %+6.2f dB\n",
                analysis.notches[i].frequency_hz,
                analysis.notches[i].q,
                analysis.notches[i].gain_db);
  }
  std::printf("  analyse %7.2f ms  apply %7.2f ms\n",
              milliseconds(started, analysed),
              milliseconds(analysed, applied));

  if (!render_target.empty()) {
    std::error_code error;
    if (render_target.has_parent_path()) {
      fs::create_directories(render_target.parent_path(), error);
    }
    return write_wav(render_target, pcm);
  }
  if (!write_audio) {
    return true;
  }
  std::error_code error;
  fs::create_directories(output_dir, error);
  const std::string stem = source.stem().string();
  return write_wav(output_dir / (stem + ".before.wav"), original) &&
         write_wav(output_dir / (stem + ".after.wav"), pcm);
}

void collect(const fs::path& root, std::vector<fs::path>& out) {
  if (fs::is_regular_file(root)) {
    out.push_back(root);
    return;
  }
  if (!fs::is_directory(root)) {
    return;
  }
  for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string extension = entry.path().extension().string();
    if (extension == ".ogg" || extension == ".wav" || extension == ".mp3" ||
        extension == ".flac") {
      out.push_back(entry.path());
    }
  }
}

auto mix_review(const fs::path& output_dir, bool write_audio) -> bool {
  using namespace Game::Audio;
  struct Layer {
    const char* path;
    MixBus bus;
    float slider;
    bool bed;
    std::vector<float> pcm;
  };
  std::array<Layer, 9> layers{{
      {"music/menu/main_theme_standard_of_iron.ogg", MixBus::Music, 0.45F, true, {}},
      {"ambience/mediterranean_plains.ogg", MixBus::Ambience, 0.30F, true, {}},
      {"sfx/combat/sword_hit_01.ogg", MixBus::Combat, 1.0F, false, {}},
      {"voices/roman/archer.ogg", MixBus::Voice, 1.0F, false, {}},
      {"sfx/ui/click_confirm.ogg", MixBus::Interface, 1.0F, false, {}},
      {"sfx/build/construction_started.ogg", MixBus::Economy, 1.0F, false, {}},
      {"ambience/weather_rain.ogg", MixBus::Weather, 0.30F, true, {}},
      {"sfx/wildlife/wolf_howl_distant.ogg", MixBus::Environment, 1.0F, false, {}},
      {"sfx/alerts/enemy_spotted_horn.ogg", MixBus::Alert, 1.0F, false, {}},
  }};
  for (auto& layer : layers) {
    const fs::path path = fs::path("assets/audio") / layer.path;
    if (!decode(path, layer.pcm)) {
      std::fprintf(stderr, "mix review: cannot decode %s\n", path.string().c_str());
      return false;
    }
    Mastering::restore(layer.pcm, CHANNELS, SAMPLE_RATE, classify(path));
  }
  fs::create_directories(output_dir);
  const fs::path report_path = output_dir / "mix-review.csv";
  std::FILE* report = std::fopen(report_path.string().c_str(), "w");
  if (report == nullptr) {
    return false;
  }
  const char* header = "preset,scene,pre_peak_dbfs,post_peak_dbfs,rms_dbfs,estimated_"
                       "lufs,min_limiter_gain\n";
  std::fputs(header, report);
  std::fputs(header, stdout);
  bool passed = true;
  for (int preset = 0; preset < 3; ++preset) {
    for (int scene = 0; scene < 3; ++scene) {
      GameplayMix mix;
      mix.prepare(SAMPLE_RATE);
      BusLimiter limiter;
      limiter.prepare(SAMPLE_RATE, CHANNELS);
      limiter.set_listening_preset(preset_from_int(preset));
      constexpr unsigned frames = SAMPLE_RATE * 8;
      std::vector<float> pcm(frames * CHANNELS, 0.0F);
      float pre_peak = 0.0F;
      float min_gain = 1.0F;
      for (unsigned frame = 0; frame < frames; ++frame) {
        MixCounts counts{};
        std::array<unsigned, 9> positions{};
        std::array<unsigned, 9> instances{};
        for (std::size_t index = 0; index < layers.size(); ++index) {
          const auto& layer = layers[index];
          const auto length = static_cast<unsigned>(layer.pcm.size() / CHANNELS);
          unsigned count = 1;
          unsigned position = frame % length;
          if (!layer.bed) {
            if (layer.bus == MixBus::Combat) {
              count = scene == 0 ? 0U : (scene == 1 ? 4U : 16U);
              position = frame % (SAMPLE_RATE / 5);
            } else if (layer.bus == MixBus::Voice || layer.bus == MixBus::Alert) {
              count = scene == 0 || frame < SAMPLE_RATE * 3U ? 0U : 1U;
              position = frame >= SAMPLE_RATE * 3U ? frame - SAMPLE_RATE * 3U : 0U;
            } else {
              count = scene == 0 ? 0U : 1U;
              position = frame % (SAMPLE_RATE * 2U);
            }
            if (position >= length) {
              count = 0;
            }
          }
          if (layer.bus == MixBus::Weather && scene == 0) {
            count = 0;
          }
          counts[mix_index(layer.bus)] += count;
          instances[index] = count;
          positions[index] = position;
        }
        const bool speech = counts[mix_index(MixBus::Voice)] > 0;
        const bool critical = speech || counts[mix_index(MixBus::Alert)] > 0;
        mix.target(counts, speech, critical, preset_from_int(preset));
        const auto& gains = mix.next();
        for (std::size_t index = 0; index < layers.size(); ++index) {
          if (instances[index] == 0) {
            continue;
          }
          const auto& layer = layers[index];
          // Worst case deliberately aligns 16 impacts and uses maximum sliders;
          // normal/quiet use first-install sliders. This bypasses cue admission
          // to stress the final bus even when all admitted sources correlate.
          const float volume = (scene == 2 ? 1.0F : 0.70F * layer.slider) *
                               float(instances[index]) * gains[mix_index(layer.bus)];
          for (unsigned channel = 0; channel < CHANNELS; ++channel) {
            pcm[frame * CHANNELS + channel] +=
                layer.pcm[positions[index] * CHANNELS + channel] * volume;
          }
        }
        for (unsigned channel = 0; channel < CHANNELS; ++channel) {
          pre_peak = std::max(pre_peak, std::abs(pcm[frame * CHANNELS + channel]));
        }
        limiter.process(pcm.data() + frame * CHANNELS, 1);
        min_gain = std::min(min_gain, limiter.gain());
      }
      const auto analysis =
          Mastering::analyse(pcm.data(), frames, CHANNELS, SAMPLE_RATE);
      double energy = 0.0;
      for (const float sample : pcm) {
        passed = passed && std::isfinite(sample) &&
                 std::abs(sample) <= BusLimiter::GAMEPLAY_CEILING + 0.00001F;
        energy += double(sample) * double(sample);
      }
      const auto db = [](double value) {
        return 20.0 * std::log10(std::max(1e-12, value));
      };
      const char* preset_name =
          preset == 0 ? "headphones" : (preset == 1 ? "speakers" : "night");
      const char* scene_name =
          scene == 0 ? "quiet" : (scene == 1 ? "normal" : "worst-case");
      for (std::FILE* destination : {report, stdout}) {
        std::fprintf(destination,
                     "%s,%s,%.3f,%.3f,%.3f,%.3f,%.5f\n",
                     preset_name,
                     scene_name,
                     db(pre_peak),
                     db(analysis.peak),
                     db(std::sqrt(energy / double(pcm.size()))),
                     analysis.loudness_lufs,
                     min_gain);
      }
      if (write_audio) {
        passed = write_wav(output_dir /
                               (std::string(preset_name) + "-" + scene_name + ".wav"),
                           pcm) &&
                 passed;
      }
    }
  }
  std::fclose(report);
  return passed;
}

} // namespace

auto main(int argc, char** argv) -> int {
  fs::path output_dir = "artifacts/audio_preview";
  fs::path render_target;
  bool write_audio = true;
  bool review_mix = false;
  std::vector<fs::path> inputs;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--mix-review") {
      review_mix = true;
    } else if (argument == "--out" && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (argument == "--render" && i + 1 < argc) {
      render_target = argv[++i];
    } else if (argument == "--report-only") {
      write_audio = false;
    } else if (argument == "--help" || argument == "-h") {
      std::printf("usage: audio_master_preview [--out DIR] [--render FILE.wav] "
                  "[--report-only] [--mix-review] [files or directories]\n");
      return 0;
    } else {
      inputs.emplace_back(argument);
    }
  }
  if (review_mix) {
    return mix_review(output_dir, write_audio) ? 0 : 1;
  }
  if (inputs.empty()) {
    inputs.emplace_back("assets/audio");
  }

  std::vector<fs::path> files;
  for (const fs::path& input : inputs) {
    collect(input, files);
  }
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    std::fprintf(stderr, "audio_master: no audio files found\n");
    return 1;
  }
  if (!render_target.empty() && files.size() != 1) {
    std::fprintf(stderr, "audio_master: --render takes exactly one input file\n");
    return 1;
  }

  int failures = 0;
  for (const fs::path& file : files) {
    if (!process(file, output_dir, write_audio, render_target)) {
      ++failures;
    }
  }
  std::printf("\n%zu file(s), %d failed\n", files.size(), failures);
  return failures == 0 ? 0 : 1;
}
