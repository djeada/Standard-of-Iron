#include "app/diagnostics/support_bundle_service.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <cstdint>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include "app/bootstrap/log_handler.h"
#include "app/core/user_settings.h"
#include "app/diagnostics/support_report.h"
#include "game/systems/persistence/save_load_service.h"
#include "render/frame/graphics_settings.h"
#include "render/scene/bootstrap.h"

namespace App::Diagnostics {

namespace {

constexpr int k_preview_characters = 6000;
constexpr qint64 k_bytes_per_megabyte = 1024LL * 1024LL;
constexpr int k_recent_log_lines_in_bundle = 800;
constexpr int k_startup_log_lines_in_bundle = 2000;

auto physical_memory_mb() -> qint64 {
#if defined(Q_OS_WIN)
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status) != 0) {
    return static_cast<qint64>(status.ullTotalPhys /
                               static_cast<unsigned long long>(k_bytes_per_megabyte));
  }
  return 0;
#elif defined(Q_OS_MACOS)
  std::uint64_t bytes = 0;
  std::size_t size = sizeof(bytes);
  if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0) {
    return static_cast<qint64>(bytes /
                               static_cast<std::uint64_t>(k_bytes_per_megabyte));
  }
  return 0;
#elif defined(Q_OS_LINUX)
  const long pages = sysconf(_SC_PHYS_PAGES);
  const long page_size = sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) {
    return static_cast<qint64>(static_cast<long long>(pages) * page_size /
                               static_cast<long long>(k_bytes_per_megabyte));
  }
  return 0;
#else
  return 0;
#endif
}

auto tail_lines(const QString& text, int max_lines) -> QString {
  const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  if (lines.size() <= max_lines) {
    return lines.join(QLatin1Char('\n'));
  }
  return lines.mid(lines.size() - max_lines).join(QLatin1Char('\n'));
}

auto read_text_file(const QString& path) -> QString {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

auto describe_directory(const QString& label, const QString& path) -> QString {
  const QDir directory(path);
  if (path.isEmpty() || !directory.exists()) {
    return QStringLiteral("%1: missing").arg(label);
  }
  const int files = directory.entryList(QDir::Files | QDir::NoDotAndDotDot).size();
  return QStringLiteral("%1: present (%2 files)").arg(label).arg(files);
}

auto startup_log_path() -> QString {
  const QString override_path = QString::fromLocal8Bit(qgetenv("SOI_STARTUP_LOG_PATH"));
  if (!override_path.isEmpty()) {
    return override_path;
  }
  const QString data_dir =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (data_dir.isEmpty()) {
    return {};
  }
  return QDir(data_dir).filePath(QStringLiteral("startup.log"));
}

// Paths whose presence in a log line would point at this machine or this
// person. The macOS temporary directory lives behind /private, and the app data
// directory can be reached through both spellings, so each one is registered in
// both forms rather than left to chance.
void append_path_variants(QStringList& out, const QString& path) {
  if (path.size() < 4 || out.contains(path)) {
    return;
  }
  out.append(path);
  out.append(QDir::toNativeSeparators(path));
}

auto graphics_tier() -> QString {
  const auto& adapter = Render::GL::RenderBootstrap::adapter();
  if (adapter.renderer.isEmpty()) {
    return {};
  }
  if (Render::GL::GLCapabilities::is_software_renderer(adapter)) {
    return QStringLiteral("software (no GPU acceleration)");
  }
  return QStringLiteral("hardware");
}

auto graphics_quality_line() -> QString {
  const auto& settings = Render::GraphicsSettings::instance();
  const QString preset =
      QString::fromLatin1(Render::graphics_quality_key(settings.quality()));
  return QStringLiteral("%1 (chosen by the player: %2)")
      .arg(preset,
           settings.quality_chosen_by_user() ? QStringLiteral("yes")
                                             : QStringLiteral("no"));
}

auto facts_now(const QString& version,
               const QString& commit,
               const QString& build_type) -> EnvironmentFacts {
  EnvironmentFacts facts;
  facts.version = version;
  facts.commit = commit;
  facts.build_type = build_type;
  facts.qt_version = QString::fromLatin1(qVersion());

  facts.os_name = QSysInfo::prettyProductName();
  facts.kernel_type = QSysInfo::kernelType();
  facts.kernel_version = QSysInfo::kernelVersion();
  facts.cpu_architecture = QSysInfo::currentCpuArchitecture();
  facts.cpu_count = QThread::idealThreadCount();
  facts.memory_mb = physical_memory_mb();
  facts.locale_name = QLocale::system().name();

  const QSettings settings = App::Core::UserSettings::open();
  facts.language =
      settings
          .value(QString::fromLatin1(App::Core::UserSettings::kLanguageKey),
                 QLocale::system().name())
          .toString();

  const auto& adapter = Render::GL::RenderBootstrap::adapter();
  facts.graphics.available = !adapter.renderer.isEmpty();
  facts.graphics.vendor = adapter.vendor;
  facts.graphics.renderer = adapter.renderer;
  facts.graphics.version = adapter.version;
  facts.graphics.tier = graphics_tier();
  facts.graphics.quality_preset = graphics_quality_line();

  if (const auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
    const auto screens = app->screens();
    facts.screen_count = static_cast<int>(screens.size());
    if (!screens.isEmpty()) {
      const QScreen* primary =
          app->primaryScreen() != nullptr ? app->primaryScreen() : screens.first();
      facts.primary_screen = primary->geometry().size();
      facts.primary_device_pixel_ratio = primary->devicePixelRatio();
    }
  }

  facts.storage_lines << describe_directory(
      QStringLiteral("saves"), Game::Systems::SaveLoadService::saves_directory());
  facts.storage_lines << describe_directory(
      QStringLiteral("exports"), Game::Systems::SaveLoadService::exports_directory());
  facts.storage_lines << QStringLiteral("settings file: %1")
                             .arg(QFileInfo(settings.fileName()).exists()
                                      ? QStringLiteral("present")
                                      : QStringLiteral("missing"));
  return facts;
}

auto settings_values() -> QList<QPair<QString, QString>> {
  const QSettings settings = App::Core::UserSettings::open();
  const QStringList keys = {
      QString::fromLatin1(App::Core::UserSettings::kGraphicsQualityKey),
      QString::fromLatin1(App::Core::UserSettings::kAutoGraphicsQualityKey),
      QString::fromLatin1(App::Core::UserSettings::kLanguageKey),
      QString::fromLatin1(App::Core::UserSettings::kMasterVolumeKey),
      QString::fromLatin1(App::Core::UserSettings::kSoundVolumeKey),
      QString::fromLatin1(App::Core::UserSettings::kMusicVolumeKey),
      QString::fromLatin1(App::Core::UserSettings::kVoiceVolumeKey),
      QString::fromLatin1(App::Core::UserSettings::kAmbienceVolumeKey),
      QString::fromLatin1(App::Core::UserSettings::kAutosaveSlotCountKey),
      QString::fromLatin1(App::Core::UserSettings::kAutosaveIntervalKey),
      QString::fromLatin1(App::Core::UserSettings::kUiScaleKey),
      QString::fromLatin1(App::Core::UserSettings::kUiReducedMotionKey),
      QString::fromLatin1(App::Core::UserSettings::kUiHighContrastKey),
      QString::fromLatin1(App::Core::UserSettings::kUiColorVisionKey),
      QString::fromLatin1(App::Core::UserSettings::kUiShowFpsKey),
      QString::fromLatin1(App::Core::UserSettings::kDisplayWindowModeKey),
      QString::fromLatin1(App::Core::UserSettings::kDisplayVsyncKey),
      QString::fromLatin1(App::Core::UserSettings::kMatchDifficultyKey),
  };

  QList<QPair<QString, QString>> values;
  for (const QString& key : keys) {
    const QVariant value = settings.value(key);
    values.append(
        {key, value.isValid() ? value.toString() : QStringLiteral("(default)")});
  }
  return values;
}

} // namespace

SupportBundleService::SupportBundleService(QObject* parent)
    : QObject(parent) {
  const QString documents =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  // Documents, not the save folder: a support bundle is something the reporter
  // is about to attach to a message, not game state.
  const QString folder = QDir(documents.isEmpty() ? QDir::homePath() : documents)
                             .filePath(QStringLiteral("Standard of Iron/support"));
  m_default_path = QDir(folder).filePath(
      default_archive_name(QDateTime::currentDateTime(), m_version, m_commit));
  refresh();
}

void SupportBundleService::set_build_info(const QString& version,
                                          const QString& commit,
                                          const QString& build_type) {
  m_version = version;
  m_commit = commit;
  m_build_type = build_type;
  const auto details_known = !m_version.isEmpty() || !m_commit.isEmpty();
  if (details_known) {
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString folder = QDir(documents.isEmpty() ? QDir::homePath() : documents)
                               .filePath(QStringLiteral("Standard of Iron/support"));
    const QString path = QDir(folder).filePath(
        default_archive_name(QDateTime::currentDateTime(), m_version, m_commit));
    if (path != m_default_path) {
      m_default_path = path;
      emit default_path_changed();
    }
  }
  refresh();
}

void SupportBundleService::set_screenshot_provider(std::function<QImage()> provider) {
  m_screenshot_provider = std::move(provider);
}

auto SupportBundleService::entries() const -> QVariantList {
  QVariantList rows;
  rows.reserve(m_entries.size());
  for (const auto& entry : m_entries) {
    QVariantMap row;
    row.insert(QStringLiteral("name"), entry.name);
    row.insert(QStringLiteral("description"), entry.description);
    row.insert(QStringLiteral("bytes"), entry.content.size());
    row.insert(QStringLiteral("binary"), entry.name.endsWith(QStringLiteral(".png")));
    rows.append(row);
  }
  return rows;
}

auto SupportBundleService::default_path() const -> QString {
  return m_default_path;
}

auto SupportBundleService::include_screenshot() const -> bool {
  return m_include_screenshot;
}

void SupportBundleService::set_include_screenshot(bool include) {
  if (m_include_screenshot == include) {
    return;
  }
  m_include_screenshot = include;
  emit include_screenshot_changed();
  refresh();
}

auto SupportBundleService::message() const -> QString {
  return m_message;
}

auto SupportBundleService::export_succeeded() const -> bool {
  return m_export_succeeded;
}

auto SupportBundleService::redaction_targets() const -> RedactionTargets {
  RedactionTargets targets;
  append_path_variants(targets.home_paths, QDir::homePath());
  append_path_variants(
      targets.data_paths,
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
  append_path_variants(
      targets.data_paths,
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
  append_path_variants(targets.data_paths,
                       QStandardPaths::writableLocation(QStandardPaths::TempLocation));
  append_path_variants(targets.extra_paths,
                       Game::Systems::SaveLoadService::saves_directory());
  append_path_variants(targets.extra_paths,
                       Game::Systems::SaveLoadService::exports_directory());
  append_path_variants(targets.extra_paths, startup_log_path());

  // The same directory reached through /private, which is how macOS reports a
  // temporary path in one log line and the plain one in the next.
  QStringList mirrored;
  for (const QString& path : targets.data_paths) {
    if (path.startsWith(QLatin1Char('/')) &&
        !path.startsWith(QStringLiteral("/private"))) {
      append_path_variants(mirrored, QStringLiteral("/private") + path);
    }
  }
  for (const QString& path : mirrored) {
    append_path_variants(targets.data_paths, path);
  }

  targets.user_name = QFileInfo(QDir::homePath()).fileName();
  if (targets.user_name.isEmpty()) {
    targets.user_name = QString::fromLocal8Bit(qgetenv("USER"));
  }
  targets.host_name = QSysInfo::machineHostName();
  return targets;
}

auto SupportBundleService::collect() const -> QList<BundleEntry> {
  const QDateTime now = QDateTime::currentDateTime();
  const RedactionTargets targets = redaction_targets();

  QList<BundleEntry> entries;

  const auto text_entry =
      [&targets](const QString& name, const QString& description, const QString& text) {
        BundleEntry entry;
        entry.name = name;
        entry.description = description;
        entry.content = redact_text(text, targets).toUtf8();
        return entry;
      };

  entries.append(text_entry(QStringLiteral("system.txt"),
                            QStringLiteral("Build, operating system, graphics adapter, "
                                           "display and storage state"),
                            QString::fromUtf8(environment_report(
                                facts_now(m_version, m_commit, m_build_type), now))));

  entries.append(
      text_entry(QStringLiteral("settings.ini"),
                 QStringLiteral("The game's saved preferences, without paths"),
                 QString::fromUtf8(settings_snapshot(settings_values()))));

  const QStringList recent =
      App::Bootstrap::recent_log_lines(k_recent_log_lines_in_bundle);
  entries.append(text_entry(QStringLiteral("logs/recent.log"),
                            QStringLiteral("The last %1 lines this run wrote to the "
                                           "console")
                                .arg(recent.size()),
                            recent.join(QLatin1Char('\n'))));

  const QString startup_path = startup_log_path();
  const QString startup = read_text_file(startup_path);
  if (!startup.isEmpty()) {
    entries.append(text_entry(QStringLiteral("logs/startup.log"),
                              QStringLiteral("The startup supervisor's log, tail"),
                              tail_lines(startup, k_startup_log_lines_in_bundle)));
  }

  if (m_include_screenshot && m_screenshot_provider) {
    const QImage shot = m_screenshot_provider();
    if (!shot.isNull()) {
      BundleEntry entry;
      entry.name = QStringLiteral("screenshot.png");
      entry.description = QStringLiteral("The window as it looked when the bundle "
                                         "was written");
      QByteArray png;
      QBuffer buffer(&png);
      buffer.open(QIODevice::WriteOnly);
      if (shot.save(&buffer, "PNG")) {
        entry.content = png;
        entries.append(entry);
      }
    }
  }

  if (!entries.isEmpty()) {
    BundleEntry readme;
    readme.name = QStringLiteral("README.txt");
    readme.description = QStringLiteral("What this bundle is and what was left out");
    readme.content = bundle_readme(entries, now, m_version);
    entries.prepend(readme);
  }
  return entries;
}

void SupportBundleService::refresh() {
  m_entries = collect();
  emit entries_changed();
}

QString SupportBundleService::preview(const QString& name, int max_characters) const {
  const int limit = max_characters > 0 ? max_characters : k_preview_characters;
  for (const auto& entry : m_entries) {
    if (entry.name != name) {
      continue;
    }
    if (entry.name.endsWith(QStringLiteral(".png"))) {
      return QStringLiteral("[screenshot: %1 bytes of PNG]").arg(entry.content.size());
    }
    const QString text = QString::fromUtf8(entry.content);
    if (text.size() <= limit) {
      return text;
    }
    return text.left(limit) +
           QStringLiteral("\n[... %1 more characters]").arg(text.size() - limit);
  }
  return {};
}

bool SupportBundleService::export_to(const QString& path) {
  const QString destination =
      path.trimmed().isEmpty() ? m_default_path : path.trimmed();
  QString target = destination;
  if (!target.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
    target += QStringLiteral(".zip");
  }

  refresh();
  const BundleWriteResult result =
      write_archive(target, m_entries, BundleLimits{}, QDateTime::currentDateTime());
  if (!result.ok) {
    set_message(QStringLiteral("The bundle was not written: %1").arg(result.error),
                false);
    return false;
  }

  set_message(QStringLiteral("Wrote %1 (%2 files, %3 KiB)")
                  .arg(QDir::toNativeSeparators(result.path))
                  .arg(result.written.size())
                  .arg((result.bytes + 1023) / 1024),
              true);
  return true;
}

void SupportBundleService::set_message(const QString& text, bool succeeded) {
  m_message = text;
  m_export_succeeded = succeeded;
  emit message_changed();
}

} // namespace App::Diagnostics
