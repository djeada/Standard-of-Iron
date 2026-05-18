#include "language_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qtranslator.h>

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
    , m_current_language("en")
    , m_translator(new QTranslator(this)) {
  m_available_languages << "en" << "de" << "pt_br";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString const default_lang = QString(DEFAULT_LANG);
  if (m_available_languages.contains(default_lang)) {
    load_language(default_lang);
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

  QString const qm_file =
      QString(":/StandardOfIron/translations/app_%1.qm").arg(language);

  if (m_translator->load(qm_file)) {
    QCoreApplication::installTranslator(m_translator);
    m_current_language = language;
    qInfo() << "Language changed to:" << language;
    emit language_changed();
  } else {
    qWarning() << "Failed to load translation file:" << qm_file;
  }
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
