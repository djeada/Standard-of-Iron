

#pragma once

#include "frame_profile.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace Render::Profiling {

class ProfilingHud : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString overlayText READ overlay_text NOTIFY overlayChanged)
  Q_PROPERTY(bool enabled READ enabled WRITE set_enabled NOTIFY enabledChanged)
  Q_PROPERTY(
      double budgetHeadroomMs READ budget_headroom_ms NOTIFY overlayChanged)
  Q_PROPERTY(double totalMs READ total_ms NOTIFY overlayChanged)
  Q_PROPERTY(quint64 drawCalls READ draw_calls NOTIFY overlayChanged)
  Q_PROPERTY(quint64 frameIndex READ frame_index NOTIFY overlayChanged)

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
  void overlayChanged();
  void enabledChanged();

private:
  void refresh();

  QString m_overlay;
  QTimer m_timer;
};

} // namespace Render::Profiling
