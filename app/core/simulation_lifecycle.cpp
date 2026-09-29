#include "app/core/simulation_lifecycle.h"

#include <QDebug>
#include <QString>

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace App::Core {

namespace {

constexpr auto k_tick_period = std::chrono::microseconds(16667);
constexpr float k_max_frame_seconds = 0.1F;

} // namespace

SimulationLifecycle::~SimulationLifecycle() {
  stop();
}

void SimulationLifecycle::start(TickBody body) {
  if (m_thread != nullptr) {
    return;
  }
  m_body = std::move(body);
  m_running.store(true, std::memory_order_release);
  m_thread.reset(QThread::create([this]() { run(); }));
  m_thread->setObjectName(QStringLiteral("SoISimulation"));
  m_thread->start();
  qInfo() << "GameEngine: simulation thread started";
}

void SimulationLifecycle::stop() {
  if (m_thread == nullptr) {
    return;
  }
  m_running.store(false, std::memory_order_release);
  m_thread->wait();
  m_thread.reset();
  qInfo() << "GameEngine: simulation thread stopped";
}

auto SimulationLifecycle::lock_frame() -> std::unique_lock<std::recursive_mutex> {
  std::unique_lock<std::recursive_mutex> lock(m_frame_mutex, std::try_to_lock);
  if (lock.owns_lock()) {
    m_stats.uncontended.fetch_add(1, std::memory_order_relaxed);
    return lock;
  }

  const auto started = std::chrono::steady_clock::now();
  m_lock_waiters.fetch_add(1, std::memory_order_release);
  lock.lock();
  m_lock_waiters.fetch_sub(1, std::memory_order_release);
  const auto waited_us =
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - started)
                                     .count());
  m_stats.contended.fetch_add(1, std::memory_order_relaxed);
  m_stats.waited_us.fetch_add(waited_us, std::memory_order_relaxed);
  std::uint64_t longest = m_stats.longest_wait_us.load(std::memory_order_relaxed);
  while (waited_us > longest && !m_stats.longest_wait_us.compare_exchange_weak(
                                    longest, waited_us, std::memory_order_relaxed)) {
  }
  return lock;
}

void SimulationLifecycle::run() {
  auto next_tick = std::chrono::steady_clock::now();
  auto last_tick = next_tick;
  while (m_running.load(std::memory_order_acquire)) {
    std::this_thread::sleep_until(next_tick);
    auto const now = std::chrono::steady_clock::now();
    next_tick += k_tick_period;
    if (next_tick < now) {
      next_tick = now + k_tick_period;
    }
    float const dt = std::min(std::chrono::duration<float>(now - last_tick).count(),
                              k_max_frame_seconds);
    last_tick = now;

    {
      const std::lock_guard<std::recursive_mutex> frame_lock(m_frame_mutex);
      if (!m_barrier.try_begin_simulation()) {
        continue;
      }
      auto const tick_start = std::chrono::steady_clock::now();
      m_body(dt);
      auto const tick_end = std::chrono::steady_clock::now();
      m_barrier.end_simulation();
      m_tick_us.fetch_add(static_cast<std::uint64_t>(
                              std::chrono::duration_cast<std::chrono::microseconds>(
                                  tick_end - tick_start)
                                  .count()),
                          std::memory_order_acq_rel);
    }

    for (int spin = 0;
         spin < k_handoff_yields && m_lock_waiters.load(std::memory_order_acquire) > 0;
         ++spin) {
      m_stats.simulation_handoff_yields.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
    }
  }
}

} // namespace App::Core
