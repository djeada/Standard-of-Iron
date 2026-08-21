

#include <QFont>
#include <QFontMetrics>
#include <QQmlEngine>
#include <QRawFont>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include "app/core/user_settings.h"
#include "ui/edge_scroll.h"
#include "ui/game_speeds.h"
#include "ui/icon_art.h"
#include "ui/input_bindings.h"
#include "ui/preferences.h"
#include "ui/theme.h"

namespace {

QTemporaryDir* g_settings_dir = nullptr;

}

class GlyphProbe : public QObject {
  Q_OBJECT

public:
  explicit GlyphProbe(QObject* parent = nullptr)
      : QObject(parent) {}

  // Answers through the font engine, so a character supplied by a fallback
  // counts as present. That is the right question for the interface glyphs:
  // what matters there is that the player sees the mark, not which family
  // drew it.
  Q_INVOKABLE static QStringList missing(const QString& family, const QString& text) {
    QFont const font(family);
    const QFontMetrics metrics(font);
    QStringList absent;
    const QVector<uint> code_points = text.toUcs4();
    for (const uint code_point : code_points) {
      if (metrics.inFontUcs4(code_point)) {
        continue;
      }
      const auto glyph = static_cast<char32_t>(code_point);
      absent.append(QStringLiteral("%1 (U+%2)")
                        .arg(QString::fromUcs4(&glyph, 1),
                             QString::number(code_point, 16).toUpper()));
    }
    return absent;
  }

  // Asks one physical face with nothing behind it. This is the right question
  // for the brand face, where a fallback is the failure: it means half a word
  // renders in another typeface.
  Q_INVOKABLE static QStringList missingWithoutFallback(const QString& family,
                                                        const QString& text) {
    const QRawFont face = QRawFont::fromFont(QFont(family));
    QStringList absent;
    if (!face.isValid()) {
      absent.append(QStringLiteral("family %1 did not resolve").arg(family));
      return absent;
    }
    const QVector<uint> code_points = text.toUcs4();
    for (const uint code_point : code_points) {
      const auto glyph = static_cast<char32_t>(code_point);
      const QString character = QString::fromUcs4(&glyph, 1);
      if (face.supportsCharacter(code_point)) {
        continue;
      }
      absent.append(QStringLiteral("%1 (U+%2)")
                        .arg(character, QString::number(code_point, 16).toUpper()));
    }
    return absent;
  }

  static GlyphProbe* create(QQmlEngine* engine, QJSEngine* scriptEngine) {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    static GlyphProbe probe;
    QQmlEngine::setObjectOwnership(&probe, QQmlEngine::CppOwnership);
    return &probe;
  }
};

class DesignSystemTestSetup : public QObject {
  Q_OBJECT

public:
  DesignSystemTestSetup() = default;

public slots:
  void applicationAvailable() {

    QStandardPaths::setTestModeEnabled(true);
    g_settings_dir = new QTemporaryDir();
    if (g_settings_dir->isValid()) {
      QSettings::setPath(
          QSettings::IniFormat, QSettings::UserScope, g_settings_dir->path());
    }
    App::Core::UserSettings::clear();

    for (const char* uri : {"StandardOfIron", "StandardOfIron.Core"}) {
      qmlRegisterSingletonType<Theme>(uri, 1, 0, "Theme", &Theme::create);
      qmlRegisterSingletonType<UiPreferences>(
          uri, 1, 0, "UiPreferences", &UiPreferences::create);
      qmlRegisterSingletonType<IconArtLibrary>(
          uri, 1, 0, "IconArt", &IconArtLibrary::create);
      qmlRegisterSingletonType<GameSpeeds>(
          uri, 1, 0, "GameSpeeds", &GameSpeeds::create);
      qmlRegisterSingletonType<EdgeScroll>(
          uri, 1, 0, "EdgeScroll", &EdgeScroll::create);
      qmlRegisterSingletonType<InputBindings>(
          uri, 1, 0, "InputBindings", &InputBindings::create);
    }
    qmlRegisterSingletonType<GlyphProbe>(
        "StandardOfIron.TestSupport", 1, 0, "GlyphProbe", &GlyphProbe::create);
  }

  void qmlEngineAvailable(QQmlEngine* engine) {

    engine->addImportPath(QStringLiteral("qrc:/"));
  }

  void cleanupTestCase() {
    App::Core::UserSettings::clear();
    delete g_settings_dir;
    g_settings_dir = nullptr;
  }
};

QUICK_TEST_MAIN_WITH_SETUP(design_system, DesignSystemTestSetup)

#include "design_system_qml_test.moc"
