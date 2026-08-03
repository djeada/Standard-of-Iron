#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DEVICE_IO
#define MA_ENABLE_MP3
#define MA_ENABLE_FLAC
#define MA_ENABLE_VORBIS
#include <stb_vorbis.h>
#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <miniaudio.h>
#include <string>
#include <vector>

#include "game/audio/audio_mastering.h"

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

} // namespace

auto main(int argc, char** argv) -> int {
  fs::path output_dir = "artifacts/audio_preview";
  fs::path render_target;
  bool write_audio = true;
  std::vector<fs::path> inputs;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--out" && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (argument == "--render" && i + 1 < argc) {
      render_target = argv[++i];
    } else if (argument == "--report-only") {
      write_audio = false;
    } else if (argument == "--help" || argument == "-h") {
      std::printf("usage: audio_master_preview [--out DIR] [--render FILE.wav] "
                  "[--report-only] [files or directories]\n");
      return 0;
    } else {
      inputs.emplace_back(argument);
    }
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
