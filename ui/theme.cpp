#include "theme.h"
#include <QString>
#include <qglobal.h>
#include <qjsengine.h>
#include <qjsonarray.h>
#include <qobject.h>
#include <qqmlengine.h>

Theme *Theme::m_instance = nullptr;

Theme::Theme(QObject *parent) : QObject(parent) {}

auto Theme::instance() -> Theme * {
  if (m_instance == nullptr) {
    m_instance = new Theme();
  }
  return m_instance;
}

auto Theme::create(QQmlEngine *engine, QJSEngine *scriptEngine) -> Theme * {
  Q_UNUSED(engine)
  Q_UNUSED(scriptEngine)
  return instance();
}

QVariantList Theme::playerColors() {
  QVariantList colors;
  colors.append(QVariantMap{{"name", "Red"}, {"hex", "#E74C3C"}});
  colors.append(QVariantMap{{"name", "Blue"}, {"hex", "#3498DB"}});
  colors.append(QVariantMap{{"name", "Brown"}, {"hex", "#8B4513"}});
  colors.append(QVariantMap{{"name", "Green"}, {"hex", "#2ECC71"}});
  colors.append(QVariantMap{{"name", "Yellow"}, {"hex", "#F1C40F"}});
  colors.append(QVariantMap{{"name", "Orange"}, {"hex", "#E67E22"}});
  colors.append(QVariantMap{{"name", "Purple"}, {"hex", "#9B59B6"}});
  colors.append(QVariantMap{{"name", "Cyan"}, {"hex", "#1ABC9C"}});
  colors.append(QVariantMap{{"name", "Pink"}, {"hex", "#E91E63"}});
  return colors;
}

QVariantList Theme::teamIcons() {
  QVariantList icons;
  icons << "⚪" << "①" << "②" << "③" << "④" << "⑤" << "⑥" << "⑦" << "⑧";
  return icons;
}

QVariantList Theme::factions() {
  QVariantList factions_data;
  factions_data.append(QVariantMap{{"id", 0}, {"name", "Standard"}});
  factions_data.append(QVariantMap{{"id", 1}, {"name", "Romans"}});
  factions_data.append(QVariantMap{{"id", 2}, {"name", "Egyptians"}});
  factions_data.append(QVariantMap{{"id", 3}, {"name", "Barbarians"}});
  return factions_data;
}

QVariantMap Theme::unitIcons() {
  QVariantMap icons;
  icons["archer"] = "🏹";
  icons["swordsman"] = "⚔️";
  icons["warrior"] = "⚔️";
  icons["spearman"] = "🛡️";
  icons["cavalry"] = "🐎";
  icons["default"] = "👤";
  return icons;
}

QVariantMap Theme::nationEmblems() {
  QVariantMap emblems;
  constexpr auto k_resource_prefix =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      "qrc:/StandardOfIron/assets/visuals/emblems/";
#else
      "qrc:/assets/visuals/emblems/";
#endif
  emblems["roman_republic"] =
      QString::fromLatin1(k_resource_prefix) + "rome.png";
  emblems["carthage"] = QString::fromLatin1(k_resource_prefix) + "cartaghe.png";
  return emblems;
}
