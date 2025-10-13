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