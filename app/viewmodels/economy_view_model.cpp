#include "app/viewmodels/economy_view_model.h"

#include "app/core/user_settings.h"

namespace App::ViewModels {

EconomyViewModel::EconomyViewModel(QObject* parent)
    : QObject(parent)
    , m_coach_enabled(App::Core::UserSettings::load_ui_economy_coach()) {
}

auto EconomyViewModel::coach_visible() const -> bool {
  return m_coach_visible;
}

auto EconomyViewModel::resource(const QString& key) const -> QVariantMap {
  for (const auto& entry : m_resources) {
    const auto map = entry.toMap();
    if (map.value(QStringLiteral("key")).toString() == key) {
      return map;
    }
  }
  return {};
}

void EconomyViewModel::dismiss_coach() {
  set_coach_enabled(false);
}

void EconomyViewModel::set_coach_enabled(bool enabled) {
  if (m_coach_enabled == enabled) {
    return;
  }
  m_coach_enabled = enabled;
  App::Core::UserSettings::save_ui_economy_coach(enabled);
  emit coach_enabled_changed();
  update_coach_visible();
}

void EconomyViewModel::set_resources(const QVariantList& resources) {
  if (m_resources == resources) {
    return;
  }
  m_resources = resources;
  emit resources_changed();
}

void EconomyViewModel::set_help(const QVariantMap& help) {
  if (m_help == help) {
    return;
  }
  m_help = help;
  emit help_changed();
}

void EconomyViewModel::set_coach(const QVariantMap& coach) {
  if (m_coach == coach) {
    return;
  }
  m_coach = coach;
  emit coach_changed();
  update_coach_visible();
}

void EconomyViewModel::set_coach_available(bool available) {
  if (m_coach_available == available) {
    return;
  }
  m_coach_available = available;
  update_coach_visible();
}

void EconomyViewModel::clear() {
  set_resources({});
  set_help({});
  set_coach({});
  set_coach_available(false);
}

void EconomyViewModel::update_coach_visible() {
  const bool visible = m_coach_enabled && m_coach_available && !m_coach.isEmpty();
  if (m_coach_visible == visible) {
    return;
  }
  m_coach_visible = visible;
  emit coach_visible_changed();
}

} // namespace App::ViewModels
