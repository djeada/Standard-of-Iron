#pragma once

#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QWaitCondition>

#include <deque>
#include <functional>
#include <thread>
#include <utility>

#include "audio_commands.h"
#include "audio_mastering.h"

namespace Game::Audio {

class TrackStore;
class TrackDecoder;

struct DecodeJob {
  QString id;
  QString path;
  int track = -1;
  Mastering::Material material = Mastering::Material::Effect;
};

struct DeferredLoop {
  float volume = 1.0F;
  float pan = 0.0F;
  MixBus mix_bus = MixBus::Unmixed;
  int priority = 0;
};

class DecodeWorker {
public:
  using CommandSink = std::function<void(const AudioCommand&)>;

  DecodeWorker(TrackStore& store, TrackDecoder& decoder, CommandSink submit)
      : m_store(store)
      , m_decoder(decoder)
      , m_submit(std::move(submit)) {}
  ~DecodeWorker() { stop(); }
  DecodeWorker(const DecodeWorker&) = delete;
  auto operator=(const DecodeWorker&) -> DecodeWorker& = delete;

  void start();
  void stop();

  auto request(DecodeJob job) -> bool;
  void cancel(int slot);
  void wait_for_slot(int slot);
  void wait_for_all();

  [[nodiscard]] auto is_pending(int slot) const -> bool;
  [[nodiscard]] auto has_pending() const -> bool;

  auto defer_if_pending(int slot, bool loop, const DeferredLoop& deferred) -> bool;
  void discard_deferred(int slot);

private:
  [[nodiscard]] auto take_next_job(DecodeJob& job) -> bool;
  void run();
  auto decode_and_install(const DecodeJob& job) -> bool;
  void finish_job(const DecodeJob& job, bool decoded);

  TrackStore& m_store;
  TrackDecoder& m_decoder;
  CommandSink m_submit;

  mutable QMutex m_mutex;
  QWaitCondition m_ready;
  QWaitCondition m_idle;
  std::deque<DecodeJob> m_jobs;
  std::deque<DecodeJob> m_bulk_jobs;
  QSet<int> m_pending_slots;
  QHash<int, DeferredLoop> m_deferred_loops;
  std::thread m_thread;
  bool m_running{false};
  int m_in_flight{0};
};

} // namespace Game::Audio
