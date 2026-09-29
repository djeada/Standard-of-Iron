#include "app/session/loading_overlay.h"

#include <QDebug>

namespace App::Session {

namespace {

constexpr int k_frames_before_release = 5;
constexpr qint64 k_min_duration_after_load_ms = 1000;
constexpr qint64 k_max_wait_ms = 15000;

} // namespace

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

void LoadingOverlay::begin() {
  m_finalize_progress = false;
  m_active = true;
}

void LoadingOverlay::abort() {
  m_active = false;
  m_wait_for_first_frame.store(false, std::memory_order_release);
  m_finalize_progress = false;
  m_show_objectives = false;
}

void LoadingOverlay::arm_after_load() {
  m_wait_for_first_frame.store(true, std::memory_order_release);
  m_frames_remaining = k_frames_before_release;
  m_min_duration_ms = k_min_duration_after_load_ms;
  m_timer.restart();
  m_finalize_progress = true;
}

auto LoadingOverlay::poll(bool renderer_ready,
                          const PendingComponents& pending_components) -> Release {
  if (!renderer_ready) {
    m_frames_remaining = k_frames_before_release;
    m_timer.restart();
    return {};
  }

  if (m_frames_remaining > 0) {
    m_frames_remaining--;
  }

  const qint64 elapsed_ms = m_timer.isValid() ? m_timer.elapsed() : 0;
  const bool enough_time = m_timer.isValid() && (elapsed_ms >= m_min_duration_ms);
  const bool exceeded_max_wait = m_timer.isValid() && (elapsed_ms >= k_max_wait_ms);

  const QStringList pending = pending_components();
  const bool startup_ready = pending.isEmpty();

  if (!(enough_time && m_frames_remaining <= 0 &&
        (startup_ready || exceeded_max_wait))) {
    return {};
  }
  if (exceeded_max_wait && !startup_ready) {
    qWarning() << "Loading overlay timed out waiting for startup readiness"
               << pending.join(", ");
  }
  m_wait_for_first_frame.store(false, std::memory_order_release);
  m_active = false;
  Release release{.released = true,
                  .finalize_progress = m_finalize_progress,
                  .show_objectives = m_show_objectives};
  m_finalize_progress = false;
  m_show_objectives = false;
  return release;
}

} // namespace App::Session
