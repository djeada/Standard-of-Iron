#include "app/core/language_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qtranslator.h>

#include "app/core/user_settings.h"

namespace {

auto layout_direction_for(const QString& language) -> Qt::LayoutDirection {
  return language == QLatin1String("ar") ? Qt::RightToLeft : Qt::LeftToRight;
}

} // namespace

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_current_language("en")
    , m_translator(new QTranslator(this)) {
  m_available_languages << "en" << "de" << "es" << "pt_br" << "ar" << "tr";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString startup_language = QString(DEFAULT_LANG);
  if (const auto saved_language = App::Core::UserSettings::load_language();
      saved_language.has_value() && m_available_languages.contains(*saved_language)) {
    startup_language = *saved_language;
  }

  if (m_available_languages.contains(startup_language)) {
    load_language(startup_language);
  } else {
    load_language("en");
  }
}

LanguageManager::~LanguageManager() = default;

auto LanguageManager::current_language() const -> QString {
  return m_current_language;
}

auto LanguageManager::available_languages() const -> QStringList {
  return m_available_languages;
}

void LanguageManager::set_language(const QString& language) {
  if (language == m_current_language || !m_available_languages.contains(language)) {
    return;
  }

  load_language(language);
}

void LanguageManager::load_language(const QString& language) {
  QCoreApplication::removeTranslator(m_translator);

  const QString qm_file = QString(":/translations/app_%1.qm").arg(language);

  if (!m_translator->load(qm_file)) {
    qWarning() << "Failed to load translation catalogue" << qm_file
               << "- is the build's lrelease step wired up?";
    return;
  }

  QCoreApplication::installTranslator(m_translator);
  QGuiApplication::setLayoutDirection(layout_direction_for(language));
  m_current_language = language;
  App::Core::UserSettings::save_language(language);
  qInfo() << "Language changed to:" << language;
  emit language_changed();
}

auto LanguageManager::language_display_name(const QString& language) -> QString {
  if (language == "en") {
    return "English";
  }
  if (language == "de") {
    return "Deutsch (German)";
  }
  if (language == "es") {
    return "Español (Spanish)";
  }
  if (language == "pt_br") {
    return "Português (Brasil)";
  }
  if (language == "ar") {
    return "العربية (Arabic)";
  }
  if (language == "tr") {
    return "Türkçe (Turkish)";
  }
  return language;
}
