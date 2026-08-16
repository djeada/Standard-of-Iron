#include "activity_view_model.h"

namespace App::ViewModels {

ActivityViewModel::ActivityViewModel(ActivityHost* host, QObject* parent)
    : QObject(parent)
    , m_host(host) {
}

QVariantMap ActivityViewModel::unit(qulonglong unit_id) const {
  return m_host != nullptr ? m_host->unit_activity(unit_id) : QVariantMap{};
}

QVariantMap ActivityViewModel::selection_summary() const {
  return m_host != nullptr ? m_host->selection_activity_summary() : QVariantMap{};
}

void ActivityViewModel::begin_repair_order() {
  if (m_host == nullptr) {
    return;
  }
  m_host->ensure_initialized();
  m_host->toggle_repair_order();
}

void ActivityViewModel::confirm_repair_at(qreal sx, qreal sy) {
  if (m_host == nullptr) {
    return;
  }
  m_host->ensure_initialized();
  m_host->confirm_repair_at(sx, sy);
}

void ActivityViewModel::toggle_auto_gather(const QString& priority_product_type) {
  if (m_host == nullptr) {
    return;
  }
  m_host->ensure_initialized();
  m_host->toggle_auto_gather(priority_product_type);
}

void ActivityViewModel::clear_inspect_target() {
  if (m_host == nullptr) {
    return;
  }
  m_host->clear_inspect_target();
}

QVariantList ActivityViewModel::pop_combat_damage_events() {
  return m_host != nullptr ? m_host->pop_combat_damage_events() : QVariantList{};
}

void ActivityViewModel::set_focus_targets(const QVariantMap& inspect,
                                          const QVariantMap& target) {
  if (m_inspect_target == inspect && m_selection_target == target) {
    return;
  }
  m_inspect_target = inspect;
  m_selection_target = target;
  emit focus_targets_changed();
}

void ActivityViewModel::set_attack_target_hint(const QVariantMap& hint) {
  if (m_attack_target_hint == hint) {
    return;
  }
  m_attack_target_hint = hint;
  emit attack_target_hint_changed();
}

} // namespace App::ViewModels
