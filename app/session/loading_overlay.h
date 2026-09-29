#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <atomic>
#include <functional>

namespace App::Session {

class LoadingOverlay {
public:
  struct Release {
    bool released = false;
    bool finalize_progress = false;
    bool show_objectives = false;
  };

  using PendingComponents = std::function<QStringList()>;

  [[nodiscard]] auto active() const -> bool { return m_active; }
  [[nodiscard]] auto waiting_for_first_frame() const -> bool {
    return m_wait_for_first_frame.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto frames_remaining() const -> int { return m_frames_remaining; }
  [[nodiscard]] auto elapsed_ms() const -> qint64 {
    return m_timer.isValid() ? m_timer.elapsed() : -1;
  }

  [[nodiscard]] auto describe(bool renderer_up, bool gpu_resources_up) const -> QString;

  void begin();
  void abort();
  void arm_after_load();
  void set_show_objectives_after_loading(bool show) { m_show_objectives = show; }

  [[nodiscard]] auto poll(bool renderer_ready,
                          const PendingComponents& pending_components) -> Release;

private:
  bool m_active = false;
  std::atomic_bool m_wait_for_first_frame{false};
  int m_frames_remaining = 0;
  qint64 m_min_duration_ms = 0;
  QElapsedTimer m_timer;
  bool m_finalize_progress = false;
  bool m_show_objectives = false;
};

} // namespace App::Session
