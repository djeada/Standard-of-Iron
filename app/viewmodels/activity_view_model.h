#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace App::ViewModels {

class ActivityHost {
public:
  ActivityHost() = default;
  ActivityHost(const ActivityHost&) = delete;
  ActivityHost(ActivityHost&&) = delete;
  auto operator=(const ActivityHost&) -> ActivityHost& = delete;
  auto operator=(ActivityHost&&) -> ActivityHost& = delete;
  virtual ~ActivityHost() = default;

  virtual void ensure_initialized() = 0;
  [[nodiscard]] virtual auto unit_activity(qulonglong unit_id) const -> QVariantMap = 0;
  [[nodiscard]] virtual auto selection_activity_summary() const -> QVariantMap = 0;
  virtual void toggle_repair_order() = 0;
  virtual void confirm_repair_at(qreal sx, qreal sy) = 0;
};

class ActivityViewModel : public QObject {
  Q_OBJECT

public:
  explicit ActivityViewModel(ActivityHost* host, QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] QVariantMap unit(qulonglong unit_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selection_summary() const;

  Q_INVOKABLE void begin_repair_order();
  Q_INVOKABLE void confirm_repair_at(qreal sx, qreal sy);

private:
  ActivityHost* m_host = nullptr;
};

} // namespace App::ViewModels
