#include "language_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qtranslator.h>

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_currentLanguage("en"),
      m_translator(new QTranslator(this)) {
  m_availableLanguages << "en" << "de";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString const default_lang = QString(DEFAULT_LANG);
  if (m_availableLanguages.contains(default_lang)) {
    loadLanguage(default_lang);
  } else {
    loadLanguage("en");
  }
}

LanguageManager::~LanguageManager() = default;

auto LanguageManager::currentLanguage() const -> QString {
  return m_currentLanguage;
}

auto LanguageManager::availableLanguages() const -> QStringList {
  return m_availableLanguages;
}

void LanguageManager::setLanguage(const QString &language) {
  if (language == m_currentLanguage ||
      !m_availableLanguages.contains(language)) {
    return;
  }

  loadLanguage(language);
}

void LanguageManager::loadLanguage(const QString &language) {
  QCoreApplication::removeTranslator(m_translator);

  QString const qm_file =
      QString(":/StandardOfIron/translations/app_%1.qm").arg(language);

  if (m_translator->load(qm_file)) {
    QCoreApplication::installTranslator(m_translator);
    m_currentLanguage = language;
    qInfo() << "Language changed to:" << language;
    emit languageChanged();
  } else {
    qWarning() << "Failed to load translation file:" << qm_file;
  }
}

QString LanguageManager::languageDisplayName(const QString &language) {
  if (language == "en") {
    return "English";
  }
  if (language == "de") {
    return "Deutsch (German)";
  }
  return language;
}
