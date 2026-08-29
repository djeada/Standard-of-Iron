#include "ui/hints.h"

#include "app/core/user_settings.h"

namespace {

using App::Core::UserSettings::Detail::load_bool;
using App::Core::UserSettings::Detail::save_bool;

} // namespace

UiHints* UiHints::m_instance = nullptr;

auto UiHints::definitions() -> const QVector<Definition>& {

  static const QVector<Definition> registry{
      {QStringLiteral("camera_legend"),
       App::Core::UserSettings::kUiCameraLegendSeenKey,
       true,
       false},
      {QStringLiteral("economy_coach"),
       App::Core::UserSettings::kUiEconomyCoachKey,
       false,
       false},
      {QStringLiteral("formation_readout"),
       App::Core::UserSettings::kUiFormationHintsKey,
       false,
       true},
  };
  return registry;
}

UiHints::UiHints(QObject* parent)
    : QObject(parent) {
  for (const auto& definition : definitions()) {
    State state;
    if (definition.stores_dismissal) {
      state.enabled = !load_bool(definition.settings_key, false);
    } else {
      state.enabled = load_bool(definition.settings_key, true);
    }
    m_state.insert(definition.id, state);
  }
}

auto UiHints::instance() -> UiHints* {
  if (m_instance == nullptr) {
    m_instance = new UiHints();
  }
  return m_instance;
}

auto UiHints::create(QQmlEngine* engine, QJSEngine* script_engine) -> UiHints* {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)
  auto* hints = instance();
  QQmlEngine::setObjectOwnership(hints, QQmlEngine::CppOwnership);
  return hints;
}

auto UiHints::definition(const QString& id) const -> const Definition* {
  for (const auto& definition : definitions()) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

void UiHints::store(const Definition& definition, bool enabled) {
  save_bool(definition.settings_key, definition.stores_dismissal ? !enabled : enabled);
}

auto UiHints::showing_map() const -> QVariantMap {
  QVariantMap map;
  for (auto it = m_state.constBegin(); it != m_state.constEnd(); ++it) {
    map.insert(it.key(), it->armed);
  }
  return map;
}

auto UiHints::enabled_map() const -> QVariantMap {
  QVariantMap map;
  for (auto it = m_state.constBegin(); it != m_state.constEnd(); ++it) {
    map.insert(it.key(), it->enabled);
  }
  return map;
}

auto UiHints::catalog() const -> QVariantList {
  QVariantList list;
  for (const auto& definition : definitions()) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), definition.id);
    entry.insert(QStringLiteral("enabled"), is_enabled(definition.id));
    list.append(entry);
  }
  return list;
}

auto UiHints::is_showing(const QString& id) const -> bool {
  const auto it = m_state.constFind(id);
  return it != m_state.constEnd() && it->armed;
}

auto UiHints::is_enabled(const QString& id) const -> bool {
  const auto it = m_state.constFind(id);
  return it != m_state.constEnd() && it->enabled;
}

void UiHints::show(const QString& id) {
  const auto it = m_state.find(id);
  if (it == m_state.end() || !it->enabled || it->armed) {
    return;
  }
  it->armed = true;
  emit changed();
}

void UiHints::reveal(const QString& id) {
  const auto it = m_state.find(id);
  if (it == m_state.end() || it->armed) {
    return;
  }
  it->armed = true;
  emit changed();
}

void UiHints::show_once(const QString& id) {
  const auto it = m_state.find(id);
  if (it == m_state.end() || !it->enabled) {
    return;
  }
  it->armed = true;
  it->enabled = false;
  const auto* definition = this->definition(id);
  if (definition != nullptr) {
    store(*definition, false);
  }
  emit changed();
}

void UiHints::dismiss(const QString& id) {
  const auto it = m_state.find(id);
  if (it == m_state.end() || !it->armed) {
    return;
  }
  it->armed = false;
  emit changed();
}

void UiHints::dismiss_all() {
  bool any = false;
  for (auto it = m_state.begin(); it != m_state.end(); ++it) {
    if (it->armed) {
      it->armed = false;
      any = true;
    }
  }
  if (any) {
    emit changed();
  }
}

void UiHints::on_selection_changed() {
  bool any = false;
  for (const auto& definition : definitions()) {
    if (!definition.selection_scoped) {
      continue;
    }
    const auto it = m_state.find(definition.id);
    if (it != m_state.end() && it->armed) {
      it->armed = false;
      any = true;
    }
  }
  if (any) {
    emit changed();
  }
}

void UiHints::suppress(const QString& id) {
  set_enabled(id, false);
}

void UiHints::set_enabled(const QString& id, bool enabled) {
  const auto it = m_state.find(id);
  if (it == m_state.end() || it->enabled == enabled) {
    return;
  }
  it->enabled = enabled;
  if (!enabled) {
    it->armed = false;
  }
  const auto* definition = this->definition(id);
  if (definition != nullptr) {
    store(*definition, enabled);
  }
  emit changed();
}

void UiHints::restore_all() {
  bool any = false;
  for (const auto& definition : definitions()) {
    const auto it = m_state.find(definition.id);
    if (it == m_state.end() || it->enabled) {
      continue;
    }
    it->enabled = true;
    store(definition, true);
    any = true;
  }
  if (any) {
    emit changed();
  }
}
