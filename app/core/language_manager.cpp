#include "language_manager.h"
#include <QCoreApplication>
#include <QDebug>

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_currentLanguage("en"), m_translator(new QTranslator(this)) {
  m_availableLanguages << "en"
                       << "de";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString defaultLang = QString(DEFAULT_LANG);
  if (m_availableLanguages.contains(defaultLang)) {
    loadLanguage(defaultLang);
  } else {
    loadLanguage("en");
  }
}

LanguageManager::~LanguageManager() {}

QString LanguageManager::currentLanguage() const { return m_currentLanguage; }

QStringList LanguageManager::availableLanguages() const {
  return m_availableLanguages;
}

void LanguageManager::setLanguage(const QString &language) {
  if (language == m_currentLanguage || !m_availableLanguages.contains(language)) {
    return;
  }

  loadLanguage(language);
}

void LanguageManager::loadLanguage(const QString &language) {
  QCoreApplication::removeTranslator(m_translator);

  QString qmFile = QString(":/StandardOfIron/translations/app_%1.qm").arg(language);

  if (m_translator->load(qmFile)) {
    QCoreApplication::installTranslator(m_translator);
    m_currentLanguage = language;
    qInfo() << "Language changed to:" << language;
    emit languageChanged();
  } else {
    qWarning() << "Failed to load translation file:" << qmFile;
  }
}

QString LanguageManager::languageDisplayName(const QString &language) const {
  if (language == "en") {
    return "English";
  } else if (language == "de") {
    return "Deutsch (German)";
  }
  return language;
}
