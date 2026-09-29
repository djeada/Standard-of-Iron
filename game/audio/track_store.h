#pragma once

#include <QHash>
#include <QMutex>
#include <QString>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "decoded_track.h"

namespace Game::Audio {

class TrackStore {
public:
  static constexpr int MAX_TRACKS = 512;

  auto claim_slot(const QString& id) -> int;
  [[nodiscard]] auto find_slot(const QString& id) const -> int;

  [[nodiscard]] auto track(std::size_t slot) const noexcept -> const DecodedTrack* {
    return m_table[slot].load(std::memory_order_acquire);
  }
  [[nodiscard]] auto is_ready(const QString& id) const -> bool;

  auto
  install(const QString& id, int slot, std::unique_ptr<DecodedTrack> track) -> bool;
  void release_slot(int slot);
  auto detach(const QString& id) -> int;
  void free_slot(int slot);
  void clear();

  [[nodiscard]] auto resident_bytes() const -> std::uint64_t {
    return m_resident_bytes.load(std::memory_order_relaxed);
  }
  [[nodiscard]] auto peak_bytes() const -> std::uint64_t {
    return m_peak_bytes.load(std::memory_order_relaxed);
  }
  [[nodiscard]] auto budget_bytes() const -> std::uint64_t { return m_budget_bytes; }
  void set_budget_bytes(std::uint64_t budget) { m_budget_bytes = budget; }
  [[nodiscard]] auto budget_overruns() const -> std::uint64_t {
    return m_budget_overruns.load(std::memory_order_relaxed);
  }

private:
  static auto default_budget_bytes() -> std::uint64_t;
  void note_resident(std::size_t bytes, const QString& id);
  void note_released(std::size_t bytes);
  void drop_storage(int slot);

  mutable QMutex m_mutex;
  QHash<QString, int> m_ids;
  std::array<bool, MAX_TRACKS> m_slot_taken{};
  std::array<std::unique_ptr<DecodedTrack>, MAX_TRACKS> m_storage;
  std::array<std::atomic<const DecodedTrack*>, MAX_TRACKS> m_table{};

  std::atomic<std::uint64_t> m_resident_bytes{0};
  std::atomic<std::uint64_t> m_peak_bytes{0};
  std::atomic<std::uint64_t> m_budget_overruns{0};
  std::uint64_t m_budget_bytes = default_budget_bytes();
};

} // namespace Game::Audio
