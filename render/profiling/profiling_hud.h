

#pragma once

#include "frame_profile.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace Render::Profiling {

class ProfilingHud : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString overlay_text READ overlay_text NOTIFY overlay_changed)
  Q_PROPERTY(bool enabled READ enabled WRITE set_enabled NOTIFY enabled_changed)
  Q_PROPERTY(
      double budget_headroom_ms READ budget_headroom_ms NOTIFY overlay_changed)
  Q_PROPERTY(double total_ms READ total_ms NOTIFY overlay_changed)
  Q_PROPERTY(quint64 draw_calls READ draw_calls NOTIFY overlay_changed)
  Q_PROPERTY(quint64 frame_index READ frame_index NOTIFY overlay_changed)

public:
  explicit ProfilingHud(QObject *parent = nullptr);

  [[nodiscard]] auto overlay_text() const -> QString { return m_overlay; }
  [[nodiscard]] auto enabled() const -> bool {
    return global_profile().enabled;
  }
  [[nodiscard]] auto budget_headroom_ms() const -> double {
    return global_profile().budget_headroom_ms;
  }
  [[nodiscard]] auto total_ms() const -> double {
    return static_cast<double>(global_profile().total_us()) / 1000.0;
  }
  [[nodiscard]] auto draw_calls() const -> quint64 {
    return global_profile().draw_calls;
  }
  [[nodiscard]] auto frame_index() const -> quint64 {
    return global_profile().frame_index;
  }

  void set_enabled(bool on);

signals:
  void overlay_changed();
  void enabled_changed();

private:
  void refresh();

  QString m_overlay;
  QTimer m_timer;
};

} // namespace Render::Profiling
