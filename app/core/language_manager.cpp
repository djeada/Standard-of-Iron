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
  m_available_languages << "en" << "de" << "es" << "pt_br" << "ar" << "tr" << "pl"
                        << "ru";

#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  QString startup_language = QString(DEFAULT_LANG);
  if (const auto saved_language = App::Core::UserSettings::load_language();
      saved_language.has_value() && m_available_languages.contains(*saved_language)) {
    startup_language = *saved_language;
  }

  if (m_available_languages.contains(startup_language)) {
    load_language(startup_language, false);
  } else {
    load_language("en", false);
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
  if (!m_available_languages.contains(language)) {
    return;
  }
  if (language == m_current_language) {
    if (m_language_is_temporary) {
      App::Core::UserSettings::save_language(language);
      m_language_is_temporary = false;
    }
    return;
  }

  load_language(language, true);
}

auto LanguageManager::apply_temporary_language(const QString& language) -> bool {
  const QString code = normalize_language_code(language);
  if (!m_available_languages.contains(code)) {
    qWarning().noquote()
        << QStringLiteral("Ignoring unsupported language override \"%1\" (available: "
                          "%2); keeping \"%3\".")
               .arg(language,
                    m_available_languages.join(QStringLiteral(", ")),
                    m_current_language);
    return false;
  }
  if (code != m_current_language && !load_language(code, false)) {
    return false;
  }
  m_language_is_temporary = true;
  qInfo().noquote() << "Language" << code
                    << "applied for this session only; the saved choice is kept.";
  return true;
}

auto LanguageManager::is_language_temporary() const -> bool {
  return m_language_is_temporary;
}

auto LanguageManager::normalize_language_code(const QString& language) -> QString {
  QString code = language.trimmed().toLower();
  code.replace(QLatin1Char('-'), QLatin1Char('_'));
  return code;
}

auto LanguageManager::load_language(const QString& language, bool persist) -> bool {
  QCoreApplication::removeTranslator(m_translator);

  const QString qm_file = QString(":/translations/app_%1.qm").arg(language);

  if (!m_translator->load(qm_file)) {
    qWarning() << "Failed to load translation catalogue" << qm_file
               << "- is the build's lrelease step wired up?";
    return false;
  }

  QCoreApplication::installTranslator(m_translator);
  QGuiApplication::setLayoutDirection(layout_direction_for(language));
  m_current_language = language;
  m_language_is_temporary = false;
  if (persist) {
    App::Core::UserSettings::save_language(language);
  }
  qInfo() << "Language changed to:" << language;
  emit language_changed();
  return true;
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
  if (language == "pl") {
    return "Polski (Polish)";
  }
  if (language == "ru") {
    return "Русский (Russian)";
  }
  return language;
}
