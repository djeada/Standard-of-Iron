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
  colors.append(QVariantMap{{"name", "Red"}, {"hex", "#FF4040"}});
  colors.append(QVariantMap{{"name", "Blue"}, {"hex", "#4080FF"}});
  colors.append(QVariantMap{{"name", "Green"}, {"hex", "#40FF60"}});
  colors.append(QVariantMap{{"name", "Yellow"}, {"hex", "#FFFF40"}});
  colors.append(QVariantMap{{"name", "Orange"}, {"hex", "#FF9040"}});
  colors.append(QVariantMap{{"name", "Purple"}, {"hex", "#B040FF"}});
  colors.append(QVariantMap{{"name", "Cyan"}, {"hex", "#40FFFF"}});
  colors.append(QVariantMap{{"name", "Pink"}, {"hex", "#FF40B0"}});
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
