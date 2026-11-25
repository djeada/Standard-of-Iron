#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

#include "utils/resource_utils.h"

namespace {

using Utils::Resources::format_resolution_failure;
using Utils::Resources::resolve_resource_path;
using Utils::Resources::resolve_resource_path_with_diagnostics;
using Utils::Resources::ResourceResolution;

const QString k_alias_request =
    QStringLiteral(":/soi_resource_utils_test/alias_only.txt");
const QString k_missing_request =
    QStringLiteral(":/soi_resource_utils_test/does_not_exist.json");

QStringList g_messages;

void capture_message(QtMsgType, const QMessageLogContext&, const QString& message) {
  g_messages.append(message);
}

class LogProbe {
public:
  explicit LogProbe(bool diagnostics)
      : m_previous_handler(qInstallMessageHandler(capture_message))
      , m_previous_diagnostics(Utils::Resources::resource_diagnostics_enabled()) {
    g_messages.clear();
    Utils::Resources::set_resource_diagnostics_enabled(diagnostics);
  }
  ~LogProbe() {
    Utils::Resources::set_resource_diagnostics_enabled(m_previous_diagnostics);
    qInstallMessageHandler(m_previous_handler);
  }
  LogProbe(const LogProbe&) = delete;
  auto operator=(const LogProbe&) -> LogProbe& = delete;
  LogProbe(LogProbe&&) = delete;
  auto operator=(LogProbe&&) -> LogProbe& = delete;

private:
  QtMessageHandler m_previous_handler;
  bool m_previous_diagnostics;
};

class ScopedCurrentDir {
public:
  explicit ScopedCurrentDir(const QString& dir)
      : m_previous(QDir::currentPath()) {
    QDir::setCurrent(dir);
  }
  ~ScopedCurrentDir() { QDir::setCurrent(m_previous); }
  ScopedCurrentDir(const ScopedCurrentDir&) = delete;
  auto operator=(const ScopedCurrentDir&) -> ScopedCurrentDir& = delete;
  ScopedCurrentDir(ScopedCurrentDir&&) = delete;
  auto operator=(ScopedCurrentDir&&) -> ScopedCurrentDir& = delete;

private:
  QString m_previous;
};

auto upward_candidates(const QString& start_dir,
                       const QString& relative) -> QStringList {
  QStringList out;
  QDir dir(start_dir);
  for (int i = 0; i < 5; ++i) {
    out.append(dir.filePath(relative));
    if (!dir.cdUp()) {
      break;
    }
  }
  return out;
}

TEST(ResourceUtilsTest, DirectlyExistingPathResolvesToItselfWithoutLogging) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  const QString file_path = temp.filePath(QStringLiteral("present.json"));
  QFile file(file_path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write("{}");
  file.close();

  const LogProbe probe(true);
  EXPECT_EQ(resolve_resource_path(file_path), file_path);

  const ResourceResolution resolution =
      resolve_resource_path_with_diagnostics(file_path);
  EXPECT_TRUE(resolution.found);
  EXPECT_EQ(resolution.path, file_path);
  EXPECT_EQ(resolution.checked, QStringList{file_path});
  EXPECT_TRUE(format_resolution_failure(resolution).isEmpty());

  EXPECT_TRUE(g_messages.isEmpty()) << g_messages.join('\n').toStdString();
}

TEST(ResourceUtilsTest, ResourceAliasResolvesUnderAlternateRootWithoutLogging) {
  const QString expected =
      QStringLiteral(":/qt/qml/StandardOfIron/soi_resource_utils_test/alias_only.txt");
  ASSERT_TRUE(QFile::exists(expected));
  ASSERT_FALSE(QFile::exists(k_alias_request));

  const LogProbe probe(true);
  EXPECT_EQ(resolve_resource_path(k_alias_request), expected);

  const ResourceResolution resolution =
      resolve_resource_path_with_diagnostics(k_alias_request);
  EXPECT_TRUE(resolution.found);
  EXPECT_EQ(resolution.path, expected);

  ASSERT_GE(resolution.checked.size(), 3);
  EXPECT_EQ(resolution.checked.last(), expected);
  EXPECT_EQ(resolution.checked.at(resolution.checked.size() - 2),
            QStringLiteral(":/StandardOfIron/soi_resource_utils_test/alias_only.txt"));
  EXPECT_EQ(resolution.checked.at(resolution.checked.size() - 3), k_alias_request);
  EXPECT_TRUE(format_resolution_failure(resolution).isEmpty());

  EXPECT_TRUE(g_messages.isEmpty()) << g_messages.join('\n').toStdString();
}

TEST(ResourceUtilsTest, MissingResourceReportsEveryCandidateOnce) {

  const QString app_dir = QCoreApplication::applicationDirPath();
  const ScopedCurrentDir in_app_dir(app_dir);
  ASSERT_EQ(QDir::currentPath(), app_dir);

  const QString relative = k_missing_request.mid(2);
  QStringList expected = upward_candidates(app_dir, relative);
  expected.append(k_missing_request);
  expected.append(QStringLiteral(":/StandardOfIron/") + relative);
  expected.append(QStringLiteral(":/qt/qml/StandardOfIron/") + relative);
  expected.append(QStringLiteral(":/qt/qml/default/") + relative);

  const ResourceResolution resolution =
      resolve_resource_path_with_diagnostics(k_missing_request);
  EXPECT_FALSE(resolution.found);
  EXPECT_EQ(resolution.path, k_missing_request);
  EXPECT_EQ(resolution.checked, expected);
  EXPECT_EQ(QSet<QString>(resolution.checked.begin(), resolution.checked.end()).size(),
            resolution.checked.size());

  QString expected_message =
      QStringLiteral("Could not resolve resource: %1\nChecked:").arg(k_missing_request);
  for (const auto& candidate : expected) {
    expected_message += QStringLiteral("\n- ") + candidate;
  }
  EXPECT_EQ(format_resolution_failure(resolution), expected_message);

  {
    const LogProbe probe(true);
    EXPECT_EQ(resolve_resource_path(k_missing_request), k_missing_request);
    ASSERT_EQ(g_messages.size(), 1);
    EXPECT_EQ(g_messages.front(), expected_message);
  }
}

TEST(ResourceUtilsTest, MissingResourceIsSilentWhenDiagnosticsAreOff) {
  const LogProbe probe(false);
  EXPECT_EQ(resolve_resource_path(k_missing_request), k_missing_request);
  EXPECT_TRUE(g_messages.isEmpty()) << g_messages.join('\n').toStdString();
}

TEST(ResourceUtilsTest, MissingPlainPathChecksOnlyItself) {
  const QString missing = QStringLiteral("soi_resource_utils_test/not_here.json");
  const ResourceResolution resolution = resolve_resource_path_with_diagnostics(missing);
  EXPECT_FALSE(resolution.found);
  EXPECT_EQ(resolution.path, missing);
  EXPECT_EQ(resolution.checked, QStringList{missing});
}

TEST(ResourceUtilsTest, EmptyPathChecksNothingAndReportsNothing) {
  const LogProbe probe(true);
  EXPECT_TRUE(resolve_resource_path(QString{}).isEmpty());
  const ResourceResolution resolution =
      resolve_resource_path_with_diagnostics(QString{});
  EXPECT_FALSE(resolution.found);
  EXPECT_TRUE(resolution.checked.isEmpty());
  EXPECT_TRUE(format_resolution_failure(resolution).isEmpty());
  EXPECT_TRUE(g_messages.isEmpty());
}

TEST(ResourceUtilsTest, DiagnosticVariantReturnsTheSamePathAsThePlainLookup) {
  const QStringList requests = {
      QStringLiteral(":/assets/maps"),
      QStringLiteral(":/assets/visuals/unit_equipment_loadouts.json"),
      QStringLiteral("assets/data"),
      k_alias_request,
      k_missing_request,
      QStringLiteral("soi_resource_utils_test/not_here.json"),
  };
  const LogProbe probe(false);
  for (const auto& request : requests) {
    EXPECT_EQ(resolve_resource_path_with_diagnostics(request).path,
              resolve_resource_path(request))
        << request.toStdString();
  }
}

} // namespace
