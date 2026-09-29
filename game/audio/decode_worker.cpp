#include "decode_worker.h"

#include <QDebug>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "track_decoder.h"
#include "track_store.h"

namespace Game::Audio {

void DecodeWorker::start() {
  QMutexLocker locker(&m_mutex);
  if (m_running) {
    return;
  }
  m_running = true;
  locker.unlock();
  m_thread = std::thread([this] { run(); });
}

void DecodeWorker::stop() {
  {
    QMutexLocker const locker(&m_mutex);
    if (!m_running) {
      return;
    }
    m_running = false;
    m_jobs.clear();
    m_bulk_jobs.clear();
    m_ready.wakeAll();
  }
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

auto DecodeWorker::take_next_job(DecodeJob& job) -> bool {
  if (!m_jobs.empty()) {
    job = m_jobs.front();
    m_jobs.pop_front();
    return true;
  }
  if (!m_bulk_jobs.empty()) {
    job = m_bulk_jobs.front();
    m_bulk_jobs.pop_front();
    return true;
  }
  return false;
}

void DecodeWorker::run() {
  for (;;) {
    DecodeJob job;
    {
      QMutexLocker locker(&m_mutex);
      while (m_running && m_jobs.empty() && m_bulk_jobs.empty()) {
        m_ready.wait(&m_mutex);
      }
      if (!m_running) {
        return;
      }
      if (!take_next_job(job)) {
        continue;
      }
      ++m_in_flight;
    }

    finish_job(job, decode_and_install(job));
  }
}

auto DecodeWorker::decode_and_install(const DecodeJob& job) -> bool {
  auto track = m_decoder.decode(job.id, job.path, job.material);
  if (!track) {
    return false;
  }
  return m_store.install(job.id, job.track, std::move(track));
}

void DecodeWorker::finish_job(const DecodeJob& job, bool decoded) {
  if (!decoded) {
    qWarning() << "MiniaudioBackend: dropping" << job.id
               << "because its audio could not be decoded";
    AudioCommand command;
    command.type = AudioCommand::Type::ReleaseTrack;
    command.track = static_cast<std::int16_t>(job.track);
    m_submit(command);
    m_store.release_slot(job.track);
  }

  std::optional<DeferredLoop> deferred;
  {
    QMutexLocker const locker(&m_mutex);
    m_pending_slots.remove(job.track);
    if (const auto held = m_deferred_loops.constFind(job.track);
        held != m_deferred_loops.constEnd()) {
      if (decoded) {
        deferred = held.value();
      }
      m_deferred_loops.remove(job.track);
    }
  }

  if (deferred.has_value()) {
    AudioCommand command;
    command.type = AudioCommand::Type::PlaySound;
    command.track = static_cast<std::int16_t>(job.track);
    command.volume = deferred->volume;
    command.pan =
        std::isfinite(deferred->pan) ? std::clamp(deferred->pan, -1.0F, 1.0F) : 0.0F;
    command.mix_bus = deferred->mix_bus;
    command.priority = deferred->priority;
    command.loop = true;
    m_submit(command);
  }

  QMutexLocker const locker(&m_mutex);
  --m_in_flight;
  m_idle.wakeAll();
}

auto DecodeWorker::request(DecodeJob job) -> bool {
  QMutexLocker locker(&m_mutex);
  if (!m_running) {
    locker.unlock();
    return decode_and_install(job);
  }
  m_pending_slots.insert(job.track);
  const bool is_bed = job.material == Mastering::Material::Music ||
                      job.material == Mastering::Material::Ambience;
  if (is_bed) {
    m_bulk_jobs.push_back(std::move(job));
  } else {
    m_jobs.push_back(std::move(job));
  }
  m_ready.wakeOne();
  return true;
}

void DecodeWorker::cancel(int slot) {
  QMutexLocker const locker(&m_mutex);
  const auto for_slot = [slot](const DecodeJob& job) {
    return job.track == slot;
  };
  const auto erase_from = [&for_slot](auto& jobs) {
    const auto before = jobs.size();
    jobs.erase(std::remove_if(jobs.begin(), jobs.end(), for_slot), jobs.end());
    return before != jobs.size();
  };
  const bool erased_urgent = erase_from(m_jobs);
  const bool erased_bulk = erase_from(m_bulk_jobs);
  if (!erased_urgent && !erased_bulk) {
    return;
  }
  m_pending_slots.remove(slot);
  m_deferred_loops.remove(slot);
  m_idle.wakeAll();
}

void DecodeWorker::wait_for_slot(int slot) {
  QMutexLocker locker(&m_mutex);
  while (m_running && m_pending_slots.contains(slot)) {
    m_idle.wait(&m_mutex);
  }
}

void DecodeWorker::wait_for_all() {
  QMutexLocker locker(&m_mutex);
  while (m_running && (!m_jobs.empty() || !m_bulk_jobs.empty() || m_in_flight > 0)) {
    m_idle.wait(&m_mutex);
  }
}

auto DecodeWorker::is_pending(int slot) const -> bool {
  QMutexLocker const locker(&m_mutex);
  return m_pending_slots.contains(slot);
}

auto DecodeWorker::has_pending() const -> bool {
  QMutexLocker const locker(&m_mutex);
  return !m_pending_slots.isEmpty();
}

auto DecodeWorker::defer_if_pending(int slot,
                                    bool loop,
                                    const DeferredLoop& deferred) -> bool {
  QMutexLocker const locker(&m_mutex);
  if (!m_pending_slots.contains(slot)) {
    return false;
  }
  if (loop) {
    m_deferred_loops.insert(slot, deferred);
  }
  return true;
}

void DecodeWorker::discard_deferred(int slot) {
  QMutexLocker const locker(&m_mutex);
  m_deferred_loops.remove(slot);
}

} // namespace Game::Audio
