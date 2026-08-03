#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace App::ViewModels {

// The slice of the engine the activity readout needs. Keeping it an interface
// stops GameEngine from growing yet another QML-facing member and lets the view
// model be exercised without a running match.
class ActivityHost {
public:
  ActivityHost() = default;
  ActivityHost(const ActivityHost&) = delete;
  ActivityHost(ActivityHost&&) = delete;
  auto operator=(const ActivityHost&) -> ActivityHost& = delete;
  auto operator=(ActivityHost&&) -> ActivityHost& = delete;
  virtual ~ActivityHost() = default;

  virtual void ensure_initialized() = 0;
  [[nodiscard]] virtual auto activity_markers() const -> QVariantList = 0;
  [[nodiscard]] virtual auto unit_activity(qulonglong unit_id) const -> QVariantMap = 0;
  [[nodiscard]] virtual auto selection_activity_summary() const -> QVariantMap = 0;
  virtual void toggle_repair_order() = 0;
  virtual void confirm_repair_at(qreal sx, qreal sy) = 0;
};

// What the HUD asks about unit activity: the overhead markers, one unit's
// current job, and what the selection as a whole is doing.
class ActivityViewModel : public QObject {
  Q_OBJECT

public:
  explicit ActivityViewModel(ActivityHost* host, QObject* parent = nullptr);

  // Grouped and already projected to screen space by the engine.
  Q_INVOKABLE [[nodiscard]] QVariantList markers() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap unit(qulonglong unit_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selection_summary() const;

  // Arms the repair cursor; the next click on a damaged friendly structure
  // sends every selected builder to mend it.
  Q_INVOKABLE void begin_repair_order();
  Q_INVOKABLE void confirm_repair_at(qreal sx, qreal sy);

private:
  ActivityHost* m_host = nullptr;
};

} // namespace App::ViewModels
