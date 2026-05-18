#ifndef LANGUAGE_MANAGER_H
#define LANGUAGE_MANAGER_H

#include <QObject>
#include <QTranslator>

class LanguageManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString current_language READ current_language WRITE set_language NOTIFY
                 language_changed)
  Q_PROPERTY(QStringList available_languages READ available_languages CONSTANT)

public:
  explicit LanguageManager(QObject* parent = nullptr);
  ~LanguageManager() override;

  [[nodiscard]] QString current_language() const;
  [[nodiscard]] QStringList available_languages() const;

  Q_INVOKABLE void set_language(const QString& language);
  Q_INVOKABLE [[nodiscard]] static QString
  language_display_name(const QString& language);

signals:
  void language_changed();

private:
  QString m_current_language;
  QTranslator* m_translator;
  QStringList m_available_languages;

  void load_language(const QString& language);
};

#endif
