#include "track_store.h"

#include <QByteArray>
#include <QDebug>
#include <QMutexLocker>

#include <utility>

namespace Game::Audio {

auto TrackStore::claim_slot(const QString& id) -> int {
  QMutexLocker const locker(&m_mutex);
  const auto existing = m_ids.constFind(id);
  if (existing != m_ids.constEnd()) {
    return existing.value();
  }
  for (int slot = 0; slot < MAX_TRACKS; ++slot) {
    if (!m_slot_taken[slot]) {
      m_slot_taken[slot] = true;
      m_ids.insert(id, slot);
      return slot;
    }
  }
  return -1;
}

auto TrackStore::find_slot(const QString& id) const -> int {
  QMutexLocker const locker(&m_mutex);
  const auto found = m_ids.constFind(id);
  return found == m_ids.constEnd() ? -1 : found.value();
}

auto TrackStore::is_ready(const QString& id) const -> bool {
  const int slot = find_slot(id);
  return slot >= 0 && track(static_cast<std::size_t>(slot)) != nullptr;
}

auto TrackStore::install(const QString& id,
                         int slot,
                         std::unique_ptr<DecodedTrack> track) -> bool {
  QMutexLocker const locker(&m_mutex);
  if (slot < 0 || slot >= MAX_TRACKS || m_ids.value(id, -1) != slot) {
    return false;
  }
  const std::size_t resident_bytes = track->pcm.size() * sizeof(std::int16_t);
  m_storage[slot] = std::move(track);
  m_table[slot].store(m_storage[slot].get(), std::memory_order_release);
  note_resident(resident_bytes, id);
  return true;
}

void TrackStore::drop_storage(int slot) {
  m_table[slot].store(nullptr, std::memory_order_release);
  if (m_storage[slot]) {
    note_released(m_storage[slot]->pcm.size() * sizeof(std::int16_t));
  }
  m_storage[slot].reset();
  m_slot_taken[slot] = false;
}

void TrackStore::release_slot(int slot) {
  if (slot < 0 || slot >= MAX_TRACKS) {
    return;
  }
  QMutexLocker const locker(&m_mutex);
  for (auto it = m_ids.begin(); it != m_ids.end(); ++it) {
    if (it.value() == slot) {
      m_ids.erase(it);
      break;
    }
  }
  drop_storage(slot);
}

auto TrackStore::detach(const QString& id) -> int {
  QMutexLocker const locker(&m_mutex);
  const auto found = m_ids.constFind(id);
  if (found == m_ids.constEnd()) {
    return -1;
  }
  const int slot = found.value();
  m_ids.erase(found);
  return slot;
}

void TrackStore::free_slot(int slot) {
  QMutexLocker const locker(&m_mutex);
  drop_storage(slot);
}

void TrackStore::clear() {
  QMutexLocker const locker(&m_mutex);
  m_ids.clear();
  for (int slot = 0; slot < MAX_TRACKS; ++slot) {
    m_table[slot].store(nullptr, std::memory_order_release);
    m_storage[slot].reset();
    m_slot_taken[slot] = false;
  }
}

auto TrackStore::default_budget_bytes() -> std::uint64_t {
  constexpr std::uint64_t k_default_megabytes = 320;
  const QByteArray configured = qgetenv("SOI_AUDIO_PCM_BUDGET_MB");
  bool ok = false;
  const qulonglong parsed = configured.toULongLong(&ok);
  const std::uint64_t megabytes =
      (ok && parsed > 0) ? static_cast<std::uint64_t>(parsed) : k_default_megabytes;
  return megabytes * 1024ULL * 1024ULL;
}

void TrackStore::note_resident(std::size_t bytes, const QString& id) {
  const std::uint64_t resident =
      m_resident_bytes.fetch_add(bytes, std::memory_order_acq_rel) + bytes;
  std::uint64_t peak = m_peak_bytes.load(std::memory_order_relaxed);
  while (resident > peak && !m_peak_bytes.compare_exchange_weak(
                                peak, resident, std::memory_order_acq_rel)) {
  }
  if (m_budget_bytes > 0 && resident > m_budget_bytes) {
    if (m_budget_overruns.fetch_add(1, std::memory_order_acq_rel) == 0) {
      qWarning() << "MiniaudioBackend: decoded audio is over its residency budget after"
                 << id << "-" << (resident / (1024 * 1024)) << "MB resident,"
                 << (m_budget_bytes / (1024 * 1024))
                 << "MB budgeted (SOI_AUDIO_PCM_BUDGET_MB)";
    }
  }
}

void TrackStore::note_released(std::size_t bytes) {
  if (bytes == 0) {
    return;
  }
  m_resident_bytes.fetch_sub(bytes, std::memory_order_acq_rel);
}

} // namespace Game::Audio
