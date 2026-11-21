#include "app/session/loading_overlay.h"

#include <QDebug>

namespace App::Session {

namespace {

constexpr int k_frames_before_release = 5;
constexpr qint64 k_min_duration_after_load_ms = 1000;
constexpr qint64 k_pending_components_wait_ms = 15000;

} // namespace

LoadingOverlay::LoadingOverlay(std::chrono::milliseconds first_frames_timeout)
    : m_first_frames_timeout(first_frames_timeout) {
  m_first_frames_timer.setSingleShot(true);
  m_first_frames_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_first_frames_timer, &QTimer::timeout, this, [this]() {
    abort();
    emit first_frames_timed_out();
  });
}

auto LoadingOverlay::describe(bool renderer_up,
                              bool gpu_resources_up) const -> QString {
  return QStringLiteral("loading overlay still up (frames remaining %1, elapsed %2ms, "
                        "renderer %3, gpu resources %4, waiting for first frame %5)")
      .arg(m_frames_remaining)
      .arg(elapsed_ms())
      .arg(renderer_up ? QStringLiteral("up") : QStringLiteral("null"))
      .arg(gpu_resources_up ? QStringLiteral("up") : QStringLiteral("null"))
      .arg(waiting_for_first_frame() ? QStringLiteral("yes") : QStringLiteral("no"));
}

auto LoadingOverlay::begin() -> quint64 {
  abort();
  ++m_load_sequence;
  m_active.store(true, std::memory_order_release);
  return m_load_sequence;
}

void LoadingOverlay::abort() {
  m_first_frames_timer.stop();
  m_armed_load.store(0, std::memory_order_release);
  m_active.store(false, std::memory_order_release);
  m_finalize_progress = false;
  m_show_objectives = false;
}

void LoadingOverlay::arm_after_load() {
  m_frames_remaining = k_frames_before_release;
  m_timer.restart();
  m_finalize_progress = true;
  m_armed_load.store(m_load_sequence, std::memory_order_release);
  m_first_frames_remaining = m_first_frames_timeout;
  if (!m_window_minimized) {
    m_first_frames_timer.start(m_first_frames_remaining);
  }
}

void LoadingOverlay::set_window_minimized(bool minimized) {
  if (m_window_minimized == minimized) {
    return;
  }
  m_window_minimized = minimized;
  if (minimized && m_first_frames_timer.isActive()) {
    m_first_frames_remaining = m_first_frames_timer.remainingTimeAsDuration();
    m_first_frames_timer.stop();
  } else if (!minimized && waiting_for_first_frame()) {
    m_first_frames_timer.start(m_first_frames_remaining);
  }
}

auto LoadingOverlay::poll(quint64 rendered_load,
                          const PendingComponents& pending_components) -> Release {
  if (rendered_load == 0 || rendered_load != armed_load()) {
    return {};
  }

  if (m_frames_remaining > 0) {
    m_frames_remaining--;
  }

  const qint64 elapsed = m_timer.elapsed();
  if (elapsed < k_min_duration_after_load_ms || m_frames_remaining > 0) {
    return {};
  }

  const QStringList pending = pending_components();
  if (!pending.isEmpty() && elapsed < k_pending_components_wait_ms) {
    return {};
  }
  if (!pending.isEmpty()) {
    qWarning() << "Loading overlay timed out waiting for startup readiness"
               << pending.join(", ");
  }
  const Release release{.released = true,
                        .finalize_progress = m_finalize_progress,
                        .show_objectives = m_show_objectives};
  abort();
  return release;
}

} // namespace App::Session
