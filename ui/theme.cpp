#include "theme.h"

Theme *Theme::m_instance = nullptr;

Theme::Theme(QObject *parent) : QObject(parent) {}

Theme *Theme::instance() {
  if (!m_instance) {
    m_instance = new Theme();
  }
  return m_instance;
}

Theme *Theme::create(QQmlEngine *engine, QJSEngine *scriptEngine) {
  Q_UNUSED(engine)
  Q_UNUSED(scriptEngine)
  return instance();
}

QVariantList Theme::playerColors() const {
  QVariantList colors;
  colors.append(QVariantMap{{"name", "Red"}, {"hex", "#E74C3C"}});
  colors.append(QVariantMap{{"name", "Blue"}, {"hex", "#3498DB"}});
  colors.append(QVariantMap{{"name", "Green"}, {"hex", "#2ECC71"}});
  colors.append(QVariantMap{{"name", "Yellow"}, {"hex", "#F1C40F"}});
  colors.append(QVariantMap{{"name", "Orange"}, {"hex", "#E67E22"}});
  colors.append(QVariantMap{{"name", "Purple"}, {"hex", "#9B59B6"}});
  colors.append(QVariantMap{{"name", "Cyan"}, {"hex", "#1ABC9C"}});
  colors.append(QVariantMap{{"name", "Pink"}, {"hex", "#E91E63"}});
  return colors;
}

QVariantList Theme::teamIcons() const {
  QVariantList icons;
  icons << "⚪" << "①" << "②" << "③" << "④" << "⑤" << "⑥" << "⑦" << "⑧";
  return icons;
}

QVariantList Theme::factions() const {
  QVariantList factionsData;
  factionsData.append(QVariantMap{{"id", 0}, {"name", "Standard"}});
  factionsData.append(QVariantMap{{"id", 1}, {"name", "Romans"}});
  factionsData.append(QVariantMap{{"id", 2}, {"name", "Egyptians"}});
  factionsData.append(QVariantMap{{"id", 3}, {"name", "Barbarians"}});
  return factionsData;
}

QVariantMap Theme::unitIcons() const {
  QVariantMap icons;
  icons["archer"] = "🏹";
  icons["knight"] = "⚔️";
  icons["warrior"] = "⚔️";
  icons["spearman"] = "🗡️";
  icons["cavalry"] = "🐎";
  icons["default"] = "👤";
  return icons;
}
