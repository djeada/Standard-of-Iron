#include "app/core/language_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QLatin1Char>
#include <QLatin1String>
#include <QStringLiteral>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qnamespace.h>
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
    : LanguageManager(QString(), parent) {
}

LanguageManager::LanguageManager(const QString& language_override, QObject* parent)
    : QObject(parent)
    , m_current_language("en")
    , m_translator(new QTranslator(this)) {
  m_available_languages << "en" << "de" << "es" << "pt_br" << "ar" << "tr" << "pl"
                        << "ru";

  if (!language_override.isEmpty()) {
    apply_language_override(language_override);
    return;
  }

  load_language(resolve_startup_language(), false);
}

LanguageManager::~LanguageManager() = default;

auto LanguageManager::current_language() const -> QString {
  return m_current_language;
}

auto LanguageManager::available_languages() const -> QStringList {
  return m_available_languages;
}

auto LanguageManager::normalize_language_code(const QString& language) -> QString {
  return language.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
}

auto LanguageManager::resolve_startup_language() const -> QString {
#ifndef DEFAULT_LANG
#define DEFAULT_LANG "en"
#endif

  if (const auto saved_language = App::Core::UserSettings::load_language();
      saved_language.has_value()) {
    const QString normalized = normalize_language_code(*saved_language);
    if (m_available_languages.contains(normalized)) {
      return normalized;
    }
  }

  const QString default_language = normalize_language_code(QString(DEFAULT_LANG));
  if (m_available_languages.contains(default_language)) {
    return default_language;
  }
  return QStringLiteral("en");
}

auto LanguageManager::apply_language_override(const QString& language) -> bool {
  const QString normalized = normalize_language_code(language);
  if (!normalized.isEmpty() && m_available_languages.contains(normalized)) {
    load_language(normalized, false);
    return true;
  }

  const QString fallback = resolve_startup_language();
  qWarning() << "Unsupported --language locale:" << language
             << "(supported:" << m_available_languages.join(", ")
             << "); falling back to" << fallback;
  load_language(fallback, false);
  return false;
}

void LanguageManager::set_language(const QString& language) {
  const QString normalized = normalize_language_code(language);
  if (normalized == m_current_language || !m_available_languages.contains(normalized)) {
    return;
  }

  load_language(normalized, true);
}

void LanguageManager::load_language(const QString& language, bool persist) {
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
  if (persist) {
    App::Core::UserSettings::save_language(language);
  }
  qInfo() << "Language changed to:" << language;
  emit language_changed();
}

auto LanguageManager::language_display_name(const QString& language) -> QString {
  const QString normalized = normalize_language_code(language);
  if (normalized == "en") {
    return "English";
  }
  if (normalized == "de") {
    return "Deutsch (German)";
  }
  if (normalized == "es") {
    return "Español (Spanish)";
  }
  if (normalized == "pt_br") {
    return "Português (Brasil)";
  }
  if (normalized == "ar") {
    return "العربية (Arabic)";
  }
  if (normalized == "tr") {
    return "Türkçe (Turkish)";
  }
  if (normalized == "pl") {
    return "Polski (Polish)";
  }
  if (normalized == "ru") {
    return "Русский (Russian)";
  }
  return language;
}
