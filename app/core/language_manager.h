#ifndef LANGUAGE_MANAGER_H
#define LANGUAGE_MANAGER_H

#include <QObject>
#include <QTranslator>

class LanguageManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage
                 NOTIFY languageChanged)
  Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)

public:
  explicit LanguageManager(QObject *parent = nullptr);
  ~LanguageManager();

  QString currentLanguage() const;
  QStringList availableLanguages() const;

  Q_INVOKABLE void setLanguage(const QString &language);
  Q_INVOKABLE QString languageDisplayName(const QString &language) const;

signals:
  void languageChanged();

private:
  QString m_currentLanguage;
  QTranslator *m_translator;
  QStringList m_availableLanguages;

  void loadLanguage(const QString &language);
};

#endif
