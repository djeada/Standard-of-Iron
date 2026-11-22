#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <functional>

namespace App::Session {

class LoadingOverlay : public QObject {
  Q_OBJECT
public:
  static constexpr auto k_first_frames_timeout = std::chrono::seconds(60);
  explicit LoadingOverlay(
      std::chrono::milliseconds first_frames_timeout = k_first_frames_timeout);

  struct Release {
    bool released = false;
    bool finalize_progress = false;
    bool show_objectives = false;
  };

  using PendingComponents = std::function<QStringList()>;

  [[nodiscard]] auto active() const -> bool {
    return m_active.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto armed_load() const -> quint64 {
    return m_armed_load.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto waiting_for_first_frame() const -> bool {
    return armed_load() != 0;
  }
  [[nodiscard]] auto frames_remaining() const -> int { return m_frames_remaining; }
  [[nodiscard]] auto elapsed_ms() const -> qint64 {
    return m_timer.isValid() ? m_timer.elapsed() : -1;
  }

  [[nodiscard]] auto describe(bool renderer_up, bool gpu_resources_up) const -> QString;

  // The GUI thread owns load preparation and the timer.
  auto begin() -> quint64;
  [[nodiscard]] auto current_load() const -> quint64 {
    return active() ? m_load_sequence : 0;
  }
  void abort();
  void arm_after_load();
  void set_window_minimized(bool minimized);
  void set_show_objectives_after_loading(bool show) { m_show_objectives = show; }

  [[nodiscard]] auto poll(quint64 rendered_load,
                          const PendingComponents& pending_components) -> Release;

signals:
  void first_frames_timed_out();

private:
  std::atomic_bool m_active{false};
  std::atomic<quint64> m_armed_load{0};
  quint64 m_load_sequence = 0;
  int m_frames_remaining = 0;
  QElapsedTimer m_timer;
  QTimer m_first_frames_timer;
  const std::chrono::milliseconds m_first_frames_timeout;
  std::chrono::milliseconds m_first_frames_remaining{0};
  bool m_window_minimized = false;
  bool m_finalize_progress = false;
  bool m_show_objectives = false;
};

} // namespace App::Session
