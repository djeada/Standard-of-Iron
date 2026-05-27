#include "language_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qtranslator.h>

#include "user_settings.h"

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_current_language("en")
    , m_translator(new QTranslator(this)) {
  m_available_languages << "en" << "de" << "pt_br";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString startup_language = QString(DEFAULT_LANG);
  if (const auto saved_language = App::Core::UserSettings::load_language();
      saved_language.has_value() &&
      m_available_languages.contains(*saved_language)) {
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

  const QStringList qm_files{
      QString(":/StandardOfIron/translations/app_%1.qm").arg(language),
      QString(":/translations/app_%1.qm").arg(language),
      QString(":/translations/translations/app_%1.qm").arg(language),
  };

  for (const QString& qm_file : qm_files) {
    if (!m_translator->load(qm_file)) {
      continue;
    }

    QCoreApplication::installTranslator(m_translator);
    m_current_language = language;
    App::Core::UserSettings::save_language(language);
    qInfo() << "Language changed to:" << language;
    emit language_changed();
    return;
  }

  qWarning() << "Failed to load translation file for language:" << language
             << "candidates:" << qm_files;
}

auto LanguageManager::language_display_name(const QString& language) -> QString {
  if (language == "en") {
    return "English";
  }
  if (language == "de") {
    return "Deutsch (German)";
  }
  if (language == "pt_br") {
    return "Português (Brasil)";
  }
  return language;
}
