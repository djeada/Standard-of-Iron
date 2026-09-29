#pragma once

#include <QHash>
#include <QMutex>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "audio_mastering.h"
#include "decoded_track.h"

namespace Game::Audio {

class TrackDecoder {
public:
  static constexpr int DECODE_BUFFER_FRAMES = 4096;

  void set_sample_rate(int hz) { m_sample_rate = hz; }

  [[nodiscard]] auto
  decode(const QString& id,
         const QString& path,
         Mastering::Material material) -> std::unique_ptr<DecodedTrack>;

  [[nodiscard]] auto analyses_computed() const -> std::uint64_t {
    return m_analyses_computed.load(std::memory_order_relaxed);
  }

private:
  struct CachedAnalysis {
    std::size_t frames = 0;
    Mastering::Analysis analysis;
  };

  auto analysis_for(const QString& id,
                    const float* pcm,
                    std::size_t frames) -> Mastering::Analysis;

  int m_sample_rate{48000};
  mutable QMutex m_analysis_cache_mutex;
  QHash<QString, CachedAnalysis> m_analysis_cache;
  std::atomic<std::uint64_t> m_analyses_computed{0};
};

} // namespace Game::Audio
