#pragma once

#include <QObject>
#include <QString>
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
  virtual void toggle_auto_gather(const QString& priority_product_type) = 0;
  virtual void clear_inspect_target() = 0;
  [[nodiscard]] virtual auto pop_combat_damage_events() -> QVariantList = 0;
};

class ActivityViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantMap attack_target_hint READ attack_target_hint NOTIFY
                 attack_target_hint_changed)
  Q_PROPERTY(
      QVariantMap inspect_target READ inspect_target NOTIFY focus_targets_changed)
  Q_PROPERTY(
      QVariantMap selection_target READ selection_target NOTIFY focus_targets_changed)

public:
  explicit ActivityViewModel(ActivityHost* host, QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] QVariantMap unit(qulonglong unit_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selection_summary() const;

  Q_INVOKABLE void begin_repair_order();
  Q_INVOKABLE void confirm_repair_at(qreal sx, qreal sy);

  Q_INVOKABLE void toggle_auto_gather(const QString& priority_product_type = {});

  Q_INVOKABLE void clear_inspect_target();
  Q_INVOKABLE [[nodiscard]] QVariantList pop_combat_damage_events();

  [[nodiscard]] auto inspect_target() const -> QVariantMap { return m_inspect_target; }
  [[nodiscard]] auto selection_target() const -> QVariantMap {
    return m_selection_target;
  }
  void set_focus_targets(const QVariantMap& inspect, const QVariantMap& target);

  [[nodiscard]] auto attack_target_hint() const -> QVariantMap {
    return m_attack_target_hint;
  }

  void set_attack_target_hint(const QVariantMap& hint);

signals:
  void attack_target_hint_changed();
  void focus_targets_changed();

private:
  ActivityHost* m_host = nullptr;
  QVariantMap m_attack_target_hint;
  QVariantMap m_inspect_target;
  QVariantMap m_selection_target;
};

} // namespace App::ViewModels
