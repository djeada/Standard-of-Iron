

#include <QFont>
#include <QFontMetrics>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRawFont>
#include <QSGRendererInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include "app/core/user_settings.h"
#include "ui/edge_scroll.h"
#include "ui/game_speeds.h"
#include "ui/hints.h"
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

class WarningProbe : public QObject {
  Q_OBJECT

public:
  explicit WarningProbe(QObject* parent = nullptr)
      : QObject(parent) {}

  Q_INVOKABLE void start() {
    QMutexLocker const lock(&s_mutex);
    s_messages.clear();
    if (!s_recording) {
      s_previous = qInstallMessageHandler(&WarningProbe::handle);
      s_recording = true;
    }
  }

  Q_INVOKABLE void stop() {
    QMutexLocker const lock(&s_mutex);
    if (!s_recording) {
      return;
    }
    qInstallMessageHandler(s_previous);
    s_previous = nullptr;
    s_recording = false;
  }

  Q_INVOKABLE static QStringList messages() {
    QMutexLocker const lock(&s_mutex);
    return s_messages;
  }

  Q_INVOKABLE static int count() {
    QMutexLocker const lock(&s_mutex);
    return static_cast<int>(s_messages.size());
  }

  static void
  handle(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    {
      QMutexLocker const lock(&s_mutex);
      if (s_recording && (type == QtWarningMsg || type == QtCriticalMsg)) {
        s_messages.append(message);
      }
    }
    if (s_previous != nullptr) {
      s_previous(type, context, message);
    }
  }

  static WarningProbe* create(QQmlEngine* engine, QJSEngine* scriptEngine) {
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    static WarningProbe probe;
    QQmlEngine::setObjectOwnership(&probe, QQmlEngine::CppOwnership);
    return &probe;
  }

private:
  static QMutex s_mutex;
  static QStringList s_messages;
  static bool s_recording;
  static QtMessageHandler s_previous;
};

QMutex WarningProbe::s_mutex;
QStringList WarningProbe::s_messages;
bool WarningProbe::s_recording = false;
QtMessageHandler WarningProbe::s_previous = nullptr;

class CommanderPortraitStub : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QString troopType READ troop_type WRITE set_troop_type NOTIFY changed)
  Q_PROPERTY(QString nation READ nation WRITE set_nation NOTIFY changed)
  Q_PROPERTY(QString pose READ pose WRITE set_pose NOTIFY changed)
  Q_PROPERTY(bool speaking READ speaking WRITE set_speaking NOTIFY changed)
  Q_PROPERTY(bool talking READ talking WRITE set_talking NOTIFY changed)

public:
  explicit CommanderPortraitStub(QQuickItem* parent = nullptr)
      : QQuickItem(parent) {}

  [[nodiscard]] auto troop_type() const -> QString { return m_troop_type; }
  void set_troop_type(const QString& value) {
    m_troop_type = value;
    emit changed();
  }
  [[nodiscard]] auto nation() const -> QString { return m_nation; }
  void set_nation(const QString& value) {
    m_nation = value;
    emit changed();
  }
  [[nodiscard]] auto pose() const -> QString { return m_pose; }
  void set_pose(const QString& value) {
    m_pose = value;
    emit changed();
  }
  [[nodiscard]] auto speaking() const -> bool { return m_speaking; }
  void set_speaking(bool value) {
    m_speaking = value;
    emit changed();
  }
  [[nodiscard]] auto talking() const -> bool { return m_talking; }
  void set_talking(bool value) {
    m_talking = value;
    emit changed();
  }

signals:
  void changed();

private:
  QString m_troop_type;
  QString m_nation;
  QString m_pose;
  bool m_speaking = false;
  bool m_talking = false;
};

class DesignSystemTestSetup : public QObject {
  Q_OBJECT

public:
  DesignSystemTestSetup() = default;

public slots:
  void applicationAvailable() {

    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND") &&
        qEnvironmentVariableIsEmpty("QMLSCENE_DEVICE")) {
      QQuickWindow::setSceneGraphBackend(QStringLiteral("software"));
    }

    QStandardPaths::setTestModeEnabled(true);
    g_settings_dir = new QTemporaryDir();
    if (g_settings_dir->isValid()) {
      QSettings::setPath(
          QSettings::IniFormat, QSettings::UserScope, g_settings_dir->path());
    }
    App::Core::UserSettings::clear();

    qmlRegisterModule("StandardOfIron", 1, 0);
    qmlRegisterType<CommanderPortraitStub>(
        "StandardOfIron", 1, 0, "CommanderPortraitViewStub");
    qmlRegisterSingletonType<GlyphProbe>(
        "StandardOfIron.TestSupport", 1, 0, "GlyphProbe", &GlyphProbe::create);
    qmlRegisterSingletonType<WarningProbe>(
        "StandardOfIron.TestSupport", 1, 0, "WarningProbe", &WarningProbe::create);
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

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QTEST_SET_MAIN_SOURCE_PATH
  DesignSystemTestSetup setup;

  return quick_test_main_with_setup(argc, argv, "design_system", nullptr, &setup);
}

#include "design_system_qml_test.moc"
