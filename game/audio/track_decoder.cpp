#include "track_decoder.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>

#include <algorithm>
#include <array>
#include <cmath>
#include <miniaudio.h>
#include <optional>
#include <utility>
#include <vector>

#include "loop_seam.h"
#include "miniaudio_config.h"
#include "resampler.h"

namespace Game::Audio {

namespace {

constexpr int k_channels = k_decoded_output_channels;
constexpr float k_pcm_scale_up = 32767.0F;

struct RawPcm {
  std::vector<float> samples;
  ma_uint32 source_rate = 0;
};

auto read_pcm(const QString& path) -> std::optional<RawPcm> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "miniaudio: QFile open failed for" << path;
    return std::nullopt;
  }

  QByteArray data = file.readAll();
  file.close();
  if (data.isEmpty()) {
    qWarning() << "miniaudio: empty track data" << path;
    return std::nullopt;
  }

  const ma_decoder_config decoder_config =
      ma_decoder_config_init(ma_format_f32, k_channels, 0);
  ma_decoder decoder;
  if (ma_decoder_init_memory(data.constData(),
                             static_cast<size_t>(data.size()),
                             &decoder_config,
                             &decoder) != MA_SUCCESS) {
    qWarning() << "miniaudio: decoder init failed for" << path;
    return std::nullopt;
  }

  RawPcm raw;
  std::vector<float>& pcm = raw.samples;
  ma_uint64 expected_frames = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &expected_frames) == MA_SUCCESS &&
      expected_frames > 0) {
    pcm.reserve(static_cast<std::size_t>(expected_frames) * k_channels);
  }

  std::array<float, TrackDecoder::DECODE_BUFFER_FRAMES * k_channels> buffer{};
  for (;;) {
    ma_uint64 frames_read = 0;
    ma_result const result = ma_decoder_read_pcm_frames(
        &decoder, buffer.data(), TrackDecoder::DECODE_BUFFER_FRAMES, &frames_read);
    if (frames_read > 0) {
      const auto samples = static_cast<std::size_t>(frames_read) * k_channels;
      pcm.insert(pcm.end(),
                 buffer.begin(),
                 buffer.begin() + static_cast<std::ptrdiff_t>(samples));
    }
    if (result == MA_AT_END) {
      break;
    }
    if (result != MA_SUCCESS) {
      ma_decoder_uninit(&decoder);
      qWarning() << "miniaudio: decode failed for" << path << "result:" << result;
      return std::nullopt;
    }
  }
  raw.source_rate = decoder.outputSampleRate;
  ma_decoder_uninit(&decoder);
  data.clear();
  data.squeeze();

  if (pcm.empty()) {
    qWarning() << "miniaudio: decode produced no PCM for" << path;
    return std::nullopt;
  }
  return raw;
}

auto quantize(std::vector<float>& pcm,
              std::size_t frame_count,
              bool channels_identical) -> std::unique_ptr<DecodedTrack> {
  auto track = std::make_unique<DecodedTrack>();
  track->frames = static_cast<unsigned>(frame_count);
  if (channels_identical) {
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      pcm[frame] = pcm[frame * k_channels];
    }
    pcm.resize(frame_count);
    pcm.shrink_to_fit();
    track->channels = 1;
  } else {
    track->channels = k_channels;
  }
  track->pcm.resize(pcm.size());
  for (std::size_t i = 0; i < pcm.size(); ++i) {
    const float clamped = std::clamp(pcm[i], -1.0F, 1.0F);
    track->pcm[i] = static_cast<std::int16_t>(std::lrintf(clamped * k_pcm_scale_up));
  }
  pcm.clear();
  pcm.shrink_to_fit();
  return track;
}

} // namespace

auto TrackDecoder::analysis_for(const QString& id,
                                const float* pcm,
                                std::size_t frames) -> Mastering::Analysis {
  {
    QMutexLocker const locker(&m_analysis_cache_mutex);
    if (const auto cached = m_analysis_cache.constFind(id);
        cached != m_analysis_cache.constEnd() && cached->frames == frames) {
      return cached->analysis;
    }
  }

  const Mastering::Analysis analysis =
      Mastering::analyse(pcm, frames, k_channels, m_sample_rate);
  m_analyses_computed.fetch_add(1, std::memory_order_relaxed);

  QMutexLocker const locker(&m_analysis_cache_mutex);
  m_analysis_cache.insert(id, CachedAnalysis{frames, analysis});
  return analysis;
}

auto TrackDecoder::decode(const QString& id,
                          const QString& path,
                          Mastering::Material material)
    -> std::unique_ptr<DecodedTrack> {
  std::optional<RawPcm> raw = read_pcm(path);
  if (!raw.has_value()) {
    return nullptr;
  }
  std::vector<float>& pcm = raw->samples;

  resample_to(pcm, k_channels, raw->source_rate, m_sample_rate);

  auto frame_count = pcm.size() / k_channels;
  const Mastering::Analysis analysis = analysis_for(id, pcm.data(), frame_count);
  Mastering::apply(pcm.data(),
                   frame_count,
                   k_channels,
                   m_sample_rate,
                   Mastering::profile_for(material),
                   analysis);

  const bool loops = material == Mastering::Material::Music ||
                     material == Mastering::Material::Ambience;
  if (loops) {
    const LoopSeamReport seam =
        seal_loop(pcm.data(), frame_count, k_channels, m_sample_rate);
    if (seam.loop_frames > 0) {
      frame_count = seam.loop_frames;
      pcm.resize(frame_count * k_channels);
    }
  }
  return quantize(pcm, frame_count, analysis.channels_identical);
}

} // namespace Game::Audio
