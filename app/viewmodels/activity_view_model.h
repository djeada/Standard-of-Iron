#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "app/world/combat_feedback.h"

namespace Engine::Core {
struct CombatHitEvent;
}

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class ActivityViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantMap attack_target_hint READ attack_target_hint NOTIFY
                 attack_target_hint_changed)
  Q_PROPERTY(
      QVariantMap inspect_target READ inspect_target NOTIFY focus_targets_changed)
  Q_PROPERTY(
      QVariantMap selection_target READ selection_target NOTIFY focus_targets_changed)

public:
  ActivityViewModel(const App::Core::ClientContext& context,
                    App::Core::ClientHost& host,
                    QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] QVariantMap unit(qulonglong unit_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selection_summary() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap unit_profile(const QString& unit_type,
                                                     const QString& nation_id) const;

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

  void record_hit(const Engine::Core::CombatHitEvent& event);
  void advance_feedback(float dt) { m_feedback.update(dt); }

signals:
  void attack_target_hint_changed();
  void focus_targets_changed();

  void inspect_target_cleared();

private:
  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;

  App::Core::CombatFeedbackStore m_feedback;
  QVariantMap m_attack_target_hint;
  QVariantMap m_inspect_target;
  QVariantMap m_selection_target;
};

} // namespace App::ViewModels
