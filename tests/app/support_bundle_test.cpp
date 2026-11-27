#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

#include "app/bootstrap/log_handler.h"
#include "app/diagnostics/support_bundle.h"
#include "app/diagnostics/support_bundle_service.h"
#include "app/diagnostics/support_report.h"

namespace {

using namespace App::Diagnostics;

auto read_u16(const QByteArray& bytes, int offset) -> quint16 {
  return static_cast<quint16>(static_cast<quint8>(bytes.at(offset)) |
                              (static_cast<quint8>(bytes.at(offset + 1)) << 8U));
}

auto read_u32(const QByteArray& bytes, int offset) -> quint32 {
  return static_cast<quint32>(read_u16(bytes, offset)) |
         (static_cast<quint32>(read_u16(bytes, offset + 2)) << 16U);
}

struct ArchiveEntry {
  QString name;
  quint16 method = 0;
  quint32 crc = 0;
  QByteArray data;
};

// A second reader of the format the writer produces, so a mistake in either one
// has to be made twice to go unnoticed. It walks the central directory rather
// than the local headers, which is how an unzipper finds what is in the file.
auto read_archive(const QByteArray& bytes, QString& error) -> QList<ArchiveEntry> {
  QList<ArchiveEntry> entries;
  if (bytes.size() < 22) {
    error = QStringLiteral("too short to hold an end of central directory record");
    return entries;
  }
  int eocd = -1;
  for (int offset = bytes.size() - 22; offset >= 0; --offset) {
    if (read_u32(bytes, offset) == 0x06054b50U) {
      eocd = offset;
      break;
    }
  }
  if (eocd < 0) {
    error = QStringLiteral("no end of central directory record");
    return entries;
  }

  const int count = read_u16(bytes, eocd + 10);
  int cursor = static_cast<int>(read_u32(bytes, eocd + 16));
  for (int index = 0; index < count; ++index) {
    if (cursor + 46 > bytes.size() || read_u32(bytes, cursor) != 0x02014b50U) {
      error = QStringLiteral("central directory entry %1 is malformed").arg(index);
      return {};
    }
    ArchiveEntry entry;
    entry.method = read_u16(bytes, cursor + 10);
    entry.crc = read_u32(bytes, cursor + 16);
    const auto compressed = static_cast<int>(read_u32(bytes, cursor + 20));
    const int name_length = read_u16(bytes, cursor + 28);
    const int extra_length = read_u16(bytes, cursor + 30);
    const int comment_length = read_u16(bytes, cursor + 32);
    const auto local_offset = static_cast<int>(read_u32(bytes, cursor + 42));
    entry.name = QString::fromUtf8(bytes.mid(cursor + 46, name_length));

    if (local_offset + 30 > bytes.size() ||
        read_u32(bytes, local_offset) != 0x04034b50U) {
      error = QStringLiteral("local header of %1 is malformed").arg(entry.name);
      return {};
    }
    const int local_name_length = read_u16(bytes, local_offset + 26);
    const int local_extra_length = read_u16(bytes, local_offset + 28);
    const int data_offset = local_offset + 30 + local_name_length + local_extra_length;
    if (data_offset + compressed > bytes.size()) {
      error = QStringLiteral("data of %1 runs past the archive").arg(entry.name);
      return {};
    }
    entry.data = bytes.mid(data_offset, compressed);
    entries.append(entry);
    cursor += 46 + name_length + extra_length + comment_length;
  }
  return entries;
}

auto sample_targets() -> RedactionTargets {
  RedactionTargets targets;
  targets.home_paths << QStringLiteral("/Users/andy");
  targets.data_paths << QStringLiteral(
      "/Users/andy/Library/Application Support/djeada");
  targets.extra_paths << QStringLiteral(
      "/Users/andy/Library/Application Support/djeada/"
      "StandardOfIron/saves");
  targets.user_name = QStringLiteral("andy");
  targets.host_name = QStringLiteral("Studio-Mac.local");
  return targets;
}

auto entry(const QString& name, const QByteArray& content) -> BundleEntry {
  BundleEntry built;
  built.name = name;
  built.content = content;
  return built;
}

TEST(SupportRedaction, HomeAndDataPathsBecomePlaceholders) {
  const QString line = QStringLiteral(
      "open /Users/andy/Games/Standard of Iron/save.db failed; config at "
      "/Users/andy/Library/Application Support/djeada/StandardOfIron/settings.ini");

  const QString redacted = redact_text(line, sample_targets());

  EXPECT_FALSE(redacted.contains(QStringLiteral("/Users/andy")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<home>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<path>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("save.db")))
      << "the file name is what a report is about and has to survive";
}

TEST(SupportRedaction, NamesAndKnownSecretsAreScrubbed) {
  const QString line =
      QStringLiteral("user andy (andy@example.com) on Studio-Mac.local used "
                     "ghp_1234567890abcdefghijABCDEFG and "
                     "authorization: Bearer abcdef123456");

  const QString redacted = redact_text(line, sample_targets());

  EXPECT_FALSE(redacted.contains(QStringLiteral("ghp_")));
  EXPECT_FALSE(redacted.contains(QStringLiteral("abcdef123456")));
  EXPECT_FALSE(redacted.contains(QStringLiteral("andy@example.com")));
  EXPECT_FALSE(redacted.contains(QStringLiteral("Studio-Mac.local")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<secret>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<email>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<host>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<user>")));
}

TEST(SupportRedaction, TheWholeAuthorizationValueIsScrubbedForAnyScheme) {
  // "Bearer" is not the only scheme a log carries: a Basic credential is a
  // base64 blurb that matches none of the token patterns, so a rule that stops
  // at the scheme name would export it verbatim.
  const QString line = QStringLiteral(
      "request failed: authorization: Basic dXNlcjpwYXNzd29yZA== (retrying)");

  const QString redacted = redact_text(line, sample_targets());

  EXPECT_FALSE(redacted.contains(QStringLiteral("dXNlcjpwYXNzd29yZA==")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("authorization=<secret>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("request failed")))
      << "the rule owns the header value, not the line around it";
}

TEST(SupportRedaction, AddressesGoButLoopbackStays) {
  const QString line = QStringLiteral(
      "connected to 10.2.3.4:7777 from 127.0.0.1:51000 over fe80::1c2d:3e4f:5a6b:7c8d");

  const QString redacted = redact_text(line, sample_targets());

  EXPECT_FALSE(redacted.contains(QStringLiteral("10.2.3.4")));
  EXPECT_FALSE(redacted.contains(QStringLiteral("fe80::")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("<ip>")));
  EXPECT_TRUE(redacted.contains(QStringLiteral("127.0.0.1")))
      << "loopback says nothing about the reporter and explains local servers";
}

TEST(SupportRedaction, OrdinaryNumbersSurvive) {
  const QString line = QStringLiteral("OpenGL 4.1 core, Qt 6.8.2, frame 2026-11-26 "
                                      "12:34:56 took 16 ms (size 1920x1080)");

  EXPECT_EQ(redact_text(line, sample_targets()), line);
}

TEST(SupportRedaction, EmptyByteArrayStaysEmpty) {
  EXPECT_TRUE(redact_bytes(QByteArray{}, sample_targets()).isEmpty());
}

TEST(SupportLimits, OversizedEntryKeepsItsTailAndSaysSo) {
  QByteArray content;
  for (int index = 0; index < 500; ++index) {
    content += QStringLiteral("line %1 of the log\n").arg(index).toUtf8();
  }
  BundleLimits limits;
  limits.max_entry_bytes = 200;
  limits.max_total_bytes = 4096;

  QList<BundleEntry> limited;
  const bool everything_fit = apply_limits(
      {entry(QStringLiteral("logs/recent.log"), content)}, limits, limited);

  ASSERT_EQ(limited.size(), 1);
  EXPECT_FALSE(everything_fit);
  const QString text = QString::fromUtf8(limited.first().content);
  EXPECT_TRUE(text.startsWith(QStringLiteral("[bundle: dropped the first")));
  EXPECT_TRUE(text.contains(QStringLiteral("line 499 of the log")));
  EXPECT_FALSE(text.contains(QStringLiteral("line 0 of the log")));
  EXPECT_LE(limited.first().content.size(), limits.max_entry_bytes + 100);
}

TEST(SupportLimits, EntryCountAndTotalBudgetAreEnforced) {
  BundleLimits limits;
  limits.max_entries = 3;
  limits.max_total_bytes = 10;

  QList<BundleEntry> limited;
  apply_limits({entry(QStringLiteral("a"), QByteArray("123456")),
                entry(QStringLiteral("b"), QByteArray("123456")),
                entry(QStringLiteral("c"), QByteArray("123456")),
                entry(QStringLiteral("d"), QByteArray("123456"))},
               limits,
               limited);

  ASSERT_EQ(limited.size(), 2);
  EXPECT_EQ(limited.at(0).content.size() + limited.at(1).content.size(), 10);
}

TEST(SupportLimits, AnOversizedScreenshotIsDroppedWholeInsteadOfCut) {
  BundleLimits limits;
  limits.max_entry_bytes = 100;
  limits.max_total_bytes = 4096;

  QList<BundleEntry> limited;
  const bool everything_fit = apply_limits(
      {entry(QStringLiteral("screenshot.png"), QByteArray(400, 'p'))}, limits, limited);

  EXPECT_TRUE(limited.isEmpty())
      << "a PNG cut on a byte boundary does not decode, so it cannot be kept";
  EXPECT_FALSE(everything_fit);
}

TEST(SupportArchive, TheReadmeNamesWhatTheSizeLimitLeftOut) {
  const QByteArray readme =
      bundle_readme({entry(QStringLiteral("system.txt"), QByteArray("build"))},
                    QDateTime(QDate(2026, 11, 26), QTime(12, 0, 0)),
                    QStringLiteral("0.3.0"),
                    {QStringLiteral("screenshot.png (over the per-file limit)")});
  const QString text = QString::fromUtf8(readme);
  EXPECT_TRUE(text.contains(QStringLiteral("Left out at the size limit")));
  EXPECT_TRUE(text.contains(QStringLiteral("screenshot.png")));
}

TEST(SupportArchive, RoundTripsThroughASecondReader) {
  const QDateTime stamp(QDate(2026, 11, 26), QTime(12, 0, 0));
  const QList<BundleEntry> entries = {
      entry(QStringLiteral("README.txt"), QByteArray("support bundle\n")),
      entry(QStringLiteral("logs/recent.log"), QByteArray("one\ntwo\n")),
  };

  QByteArray archive;
  const BundleWriteResult result =
      build_archive(entries, BundleLimits{}, stamp, archive);
  ASSERT_TRUE(result.ok) << result.error.toStdString();
  EXPECT_EQ(result.written.size(), 2);

  QString error;
  const auto parsed = read_archive(archive, error);
  ASSERT_TRUE(error.isEmpty()) << error.toStdString();
  ASSERT_EQ(parsed.size(), 2);
  EXPECT_EQ(parsed.at(0).name, QStringLiteral("README.txt"));
  EXPECT_EQ(parsed.at(1).name, QStringLiteral("logs/recent.log"));
  EXPECT_EQ(parsed.at(1).data, QByteArray("one\ntwo\n"));
  for (const auto& one : parsed) {
    EXPECT_EQ(one.method, 0) << "the bundle is stored, never compressed";
  }
}

TEST(SupportArchive, SameInputsProduceTheSameBytes) {
  const QDateTime stamp(QDate(2026, 11, 26), QTime(12, 0, 0));
  const QList<BundleEntry> entries = {
      entry(QStringLiteral("system.txt"), QByteArray("version: 0.3.0\n"))};

  QByteArray first;
  QByteArray second;
  ASSERT_TRUE(build_archive(entries, BundleLimits{}, stamp, first).ok);
  ASSERT_TRUE(build_archive(entries, BundleLimits{}, stamp, second).ok);
  EXPECT_EQ(first, second);

  QByteArray later;
  ASSERT_TRUE(build_archive(entries, BundleLimits{}, stamp.addSecs(2), later).ok);
  EXPECT_NE(first, later) << "the archive records when it was made";
}

TEST(SupportArchive, EmptyAndNamelessInputsAreRefused) {
  QByteArray archive;
  const auto empty =
      build_archive({}, BundleLimits{}, QDateTime::currentDateTime(), archive);
  EXPECT_FALSE(empty.ok);
  EXPECT_FALSE(empty.error.isEmpty());

  const QList<BundleEntry> repeated = {entry(QStringLiteral("a"), QByteArray("1")),
                                       entry(QStringLiteral("a"), QByteArray("2"))};
  const auto both =
      build_archive(repeated, BundleLimits{}, QDateTime::currentDateTime(), archive);
  ASSERT_TRUE(both.ok);
  EXPECT_EQ(both.written.size(), 1) << "a repeated name would shadow the first";
}

TEST(SupportArchive, WritesIntoANewFolder) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path =
      QDir(directory.path()).filePath(QStringLiteral("nested/support.zip"));

  const auto result =
      write_archive(path,
                    {entry(QStringLiteral("system.txt"), QByteArray("ok\n"))},
                    BundleLimits{},
                    QDateTime::currentDateTime());

  ASSERT_TRUE(result.ok) << result.error.toStdString();
  EXPECT_EQ(result.path, path);
  EXPECT_TRUE(QFile::exists(path));
  EXPECT_GT(result.bytes, 0);
}

TEST(SupportArchive, FailsWhenTheDestinationCannotBeWritten) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString blocker = QDir(directory.path()).filePath(QStringLiteral("blocker"));
  {
    QFile file(blocker);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not a folder");
  }

  const auto result =
      write_archive(QDir(blocker).filePath(QStringLiteral("support.zip")),
                    {entry(QStringLiteral("system.txt"), QByteArray("ok\n"))},
                    BundleLimits{},
                    QDateTime::currentDateTime());

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.isEmpty());
}

TEST(SupportArchive, NamesCarryTheBuildAndTheMoment) {
  const QDateTime stamp(QDate(2026, 11, 26), QTime(9, 5, 3));
  EXPECT_EQ(default_archive_name(
                stamp, QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d4e5f6")),
            QStringLiteral("standard-of-iron-support-20261126-090503-a1b2c3d.zip"));
  EXPECT_EQ(
      default_archive_name(stamp, QStringLiteral("0.3.0"), QStringLiteral("unknown")),
      QStringLiteral("standard-of-iron-support-20261126-090503-0.3.0.zip"));
}

TEST(SupportReport, ReportStatesTheBuildAndTheAdapter) {
  EnvironmentFacts facts;
  facts.version = QStringLiteral("0.3.0");
  facts.commit = QStringLiteral("a1b2c3d");
  facts.build_type = QStringLiteral("Debug");
  facts.qt_version = QStringLiteral("6.8.2");
  facts.os_name = QStringLiteral("macOS 27.0");
  facts.kernel_type = QStringLiteral("darwin");
  facts.kernel_version = QStringLiteral("25.0.0");
  facts.cpu_architecture = QStringLiteral("arm64");
  facts.cpu_count = 10;
  facts.memory_mb = 32768;
  facts.graphics.available = true;
  facts.graphics.vendor = QStringLiteral("Apple");
  facts.graphics.renderer = QStringLiteral("Apple M3 Pro");
  facts.graphics.version = QStringLiteral("4.1 Metal - 89.3");
  facts.graphics.tier = QStringLiteral("hardware");
  facts.graphics.quality_preset = QStringLiteral("high (chosen by the player: no)");
  facts.screen_count = 1;
  facts.primary_screen = QSize(1728, 1117);
  facts.primary_device_pixel_ratio = 2.0;
  facts.storage_lines << QStringLiteral("saves: present (4 files)");

  const QByteArray report =
      environment_report(facts, QDateTime(QDate(2026, 11, 26), QTime(12, 0, 0)));

  const QString text = QString::fromUtf8(report);
  EXPECT_TRUE(text.contains(QStringLiteral("version: 0.3.0")));
  EXPECT_TRUE(text.contains(QStringLiteral("renderer: Apple M3 Pro")));
  EXPECT_TRUE(text.contains(QStringLiteral("primary screen: 1728x1117 @ 2.00")));
  EXPECT_TRUE(text.contains(QStringLiteral("saves: present (4 files)")));
  EXPECT_TRUE(text.contains(QStringLiteral("generated: %1")
                                .arg(QDateTime(QDate(2026, 11, 26), QTime(12, 0, 0))
                                         .toUTC()
                                         .toString(Qt::ISODate))));
}

TEST(SupportReport, AMissingAdapterSaysSoInsteadOfGuessing) {
  EnvironmentFacts facts;
  facts.version = QStringLiteral("0.3.0");

  const QString text = QString::fromUtf8(
      environment_report(facts, QDateTime(QDate(2026, 11, 26), QTime(12, 0, 0))));

  EXPECT_TRUE(text.contains(QStringLiteral("adapter: unavailable")));
}

TEST(SupportReport, SettingsAreSortedAndKeepTheirKeys) {
  const QByteArray snapshot = settings_snapshot({
      {QStringLiteral("ui/scale"), QStringLiteral("1.25")},
      {QStringLiteral("audio/master_volume"), QStringLiteral("0.8")},
  });

  EXPECT_EQ(QString::fromUtf8(snapshot),
            QStringLiteral("# Standard of Iron preferences as they were when this "
                           "bundle was written.\n"
                           "# Only the values that describe how the game behaves; no "
                           "paths, no account identifiers.\n"
                           "audio/master_volume=0.8\n"
                           "ui/scale=1.25\n"));
}

TEST(SupportLog, RecentLinesAreTheTailOfWhatWasLogged) {
  App::Bootstrap::install_log_message_handler();
  App::Bootstrap::clear_recent_log_lines();

  qInfo() << "support-bundle-first-line";
  qWarning() << "support-bundle-second-line";

  const QStringList recent = App::Bootstrap::recent_log_lines(2);
  qInstallMessageHandler(nullptr);
  App::Bootstrap::clear_recent_log_lines();

  ASSERT_EQ(recent.size(), 2);
  EXPECT_TRUE(recent.at(0).startsWith(QStringLiteral("[INFO] ")));
  EXPECT_TRUE(recent.at(0).contains(QStringLiteral("support-bundle-first-line")));
  EXPECT_TRUE(recent.at(1).startsWith(QStringLiteral("[WARNING] ")));
}

TEST(SupportService, BundleCarriesTheReportTheSettingsAndTheReadme) {
  SupportBundleService service;
  service.set_build_info(
      QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d"), QStringLiteral("Debug"));

  const QVariantList rows = service.entries();
  QStringList names;
  for (const auto& row : rows) {
    names << row.toMap().value(QStringLiteral("name")).toString();
  }

  EXPECT_EQ(names.value(0), QStringLiteral("README.txt"))
      << "the readme is what a maintainer opens first";
  EXPECT_TRUE(names.contains(QStringLiteral("system.txt")));
  EXPECT_TRUE(names.contains(QStringLiteral("settings.ini")));
  EXPECT_TRUE(names.contains(QStringLiteral("logs/recent.log")));
  EXPECT_FALSE(names.contains(QStringLiteral("screenshot.png")))
      << "a screenshot is only ever taken when the reporter asked for one";
}

TEST(SupportService, PreviewShowsTheTextAndSkipsTheScreenshot) {
  SupportBundleService service;
  service.set_build_info(
      QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d"), QStringLiteral("Debug"));
  service.set_screenshot_provider([] {
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
  });
  service.set_include_screenshot(true);

  const QString preview = service.preview(QStringLiteral("system.txt"), 40);
  EXPECT_TRUE(preview.contains(QStringLiteral("Standard of Iron")));
  EXPECT_TRUE(preview.contains(QStringLiteral("more characters")));

  const QString shot = service.preview(QStringLiteral("screenshot.png"), 40);
  EXPECT_TRUE(shot.startsWith(QStringLiteral("[screenshot:")));
  EXPECT_FALSE(shot.contains(QStringLiteral("PNG\n")));
}

TEST(SupportService, ExportWritesAZipTheReaderCanOpen) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  SupportBundleService service;
  service.set_build_info(
      QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d"), QStringLiteral("Debug"));

  const QString target = QDir(directory.path()).filePath(QStringLiteral("bundle"));
  ASSERT_TRUE(service.export_to(target));
  EXPECT_TRUE(service.export_succeeded());
  EXPECT_TRUE(service.message().contains(QStringLiteral("bundle.zip")));

  QFile file(QDir(directory.path()).filePath(QStringLiteral("bundle.zip")));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  QString error;
  const auto parsed = read_archive(file.readAll(), error);
  ASSERT_TRUE(error.isEmpty()) << error.toStdString();
  EXPECT_GE(parsed.size(), 3);
  EXPECT_EQ(parsed.first().name, QStringLiteral("README.txt"));
}

TEST(SupportService, ALargeScreenshotIsScaledToFitAndStillDecodes) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  SupportBundleService service;
  service.set_build_info(
      QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d"), QStringLiteral("Debug"));
  service.set_screenshot_provider([] {
    QImage image(900, 700, QImage::Format_RGB32);
    quint32 seed = 2463534242U;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        seed ^= seed << 13U;
        seed ^= seed >> 17U;
        seed ^= seed << 5U;
        image.setPixel(
            x, y, qRgb(seed & 0xff, (seed >> 8) & 0xff, (seed >> 16) & 0xff));
      }
    }
    return image;
  });
  service.set_include_screenshot(true);

  const QString target = QDir(directory.path()).filePath(QStringLiteral("bundle"));
  ASSERT_TRUE(service.export_to(target));

  QFile file(QDir(directory.path()).filePath(QStringLiteral("bundle.zip")));
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  QString error;
  const auto parsed = read_archive(file.readAll(), error);
  ASSERT_TRUE(error.isEmpty()) << error.toStdString();

  int shot = -1;
  for (int index = 0; index < parsed.size(); ++index) {
    if (parsed.at(index).name == QStringLiteral("screenshot.png")) {
      shot = index;
    }
  }
  ASSERT_GE(shot, 0) << "the image fits once scaled, so it has to be in the bundle";
  const QByteArray png = parsed.at(shot).data;
  EXPECT_LE(png.size(), BundleLimits{}.max_entry_bytes);
  QImage decoded;
  ASSERT_TRUE(decoded.loadFromData(png, "PNG"))
      << "a PNG in a bundle has to decode; a byte cut would not";
  EXPECT_GT(decoded.width(), 16);
}

TEST(SupportService, ExportToAnUnwritablePlaceReportsWhyItFailed) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString blocker = QDir(directory.path()).filePath(QStringLiteral("blocker"));
  {
    QFile file(blocker);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not a folder");
  }

  SupportBundleService service;
  EXPECT_FALSE(service.export_to(QDir(blocker).filePath(QStringLiteral("bundle.zip"))));
  EXPECT_FALSE(service.export_succeeded());
  EXPECT_TRUE(service.message().contains(QStringLiteral("not written")));
}

TEST(SupportService, ImportedPathsAreRedactedInTheReadme) {
  SupportBundleService service;
  service.set_build_info(
      QStringLiteral("0.3.0"), QStringLiteral("a1b2c3d"), QStringLiteral("Debug"));

  const QString readme = service.preview(QStringLiteral("README.txt"), 8000);
  EXPECT_TRUE(readme.contains(QStringLiteral("saved games")))
      << "the readme has to say what was left out";
}

} // namespace
