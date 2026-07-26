#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

namespace {

auto repo_root() -> QDir {

  return QDir::current();
}

auto icons_dir() -> QDir {
  return QDir(repo_root().filePath(QStringLiteral("assets/visuals/icons")));
}

auto read_text(const QString& relative_path) -> QString {
  QFile file(repo_root().filePath(relative_path));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

auto registry_filenames() -> QStringList {
  const QString source = read_text(QStringLiteral("ui/qml/design/Icons.qml"));
  QStringList names;
  static const QRegularExpression pattern(QStringLiteral("\"([A-Za-z0-9_]+\\.png)\""));
  auto matches = pattern.globalMatch(source);
  while (matches.hasNext()) {
    const QString name = matches.next().captured(1);
    if (!names.contains(name)) {
      names.append(name);
    }
  }
  return names;
}

auto registry_unit_family_filenames() -> QStringList {
  const QString source = read_text(QStringLiteral("ui/qml/design/Icons.qml"));

  const auto capture_block = [&source](const QString& property) -> QString {
    const int start = source.indexOf(property + QStringLiteral(":"));
    if (start < 0) {
      return {};
    }
    const int open = source.indexOf(QLatin1Char('{'), start);
    const int close = source.indexOf(QStringLiteral("})"), open);
    if (open < 0 || close < 0) {
      return {};
    }
    return source.mid(open, close - open);
  };

  static const QRegularExpression entry(
      QStringLiteral("\"([A-Za-z0-9_]+)\"\\s*:\\s*\"([A-Za-z0-9_]+)\""));

  QStringList suffixes;
  auto suffix_matches =
      entry.globalMatch(capture_block(QStringLiteral("nationArtSuffix")));
  while (suffix_matches.hasNext()) {
    const QString suffix = suffix_matches.next().captured(2);
    if (!suffixes.contains(suffix)) {
      suffixes.append(suffix);
    }
  }

  QStringList bases;
  auto base_matches = entry.globalMatch(capture_block(QStringLiteral("unitArtBase")));
  while (base_matches.hasNext()) {
    const QString base = base_matches.next().captured(2);
    if (!bases.contains(base)) {
      bases.append(base);
    }
  }

  QStringList names;
  for (const QString& base : bases) {
    for (const QString& suffix : suffixes) {
      names.append(base + QLatin1Char('_') + suffix + QStringLiteral(".png"));
    }
  }
  return names;
}

TEST(IconResourcesTest, TheRegistryFileIsReadableFromTheSuiteWorkingDirectory) {
  ASSERT_FALSE(read_text(QStringLiteral("ui/qml/design/Icons.qml")).isEmpty())
      << "tests must run with the repository root as the working directory";
  ASSERT_TRUE(icons_dir().exists());
}

TEST(IconResourcesTest, EveryFilenameTheRegistryNamesExistsOnDisk) {
  const QStringList names = registry_filenames() + registry_unit_family_filenames();
  ASSERT_FALSE(names.isEmpty());

  QStringList missing;
  for (const QString& name : names) {
    if (!QFileInfo::exists(icons_dir().filePath(name))) {
      missing.append(name);
    }
  }

  EXPECT_TRUE(missing.isEmpty()) << "Icons.qml names artwork that does not exist: "
                                 << missing.join(", ").toStdString();
}

TEST(IconResourcesTest, EveryShippedIconIsEmbeddedInTheQmlModule) {

  const QString cmake = read_text(QStringLiteral("CMakeLists.txt"));
  ASSERT_FALSE(cmake.isEmpty());
  EXPECT_TRUE(cmake.contains(QStringLiteral("assets/visuals/icons/*.png")))
      << "the icon glob disappeared from CMakeLists.txt; icons may silently stop "
         "shipping";
  EXPECT_TRUE(cmake.contains(QStringLiteral("${SOI_UNIT_ICON_RESOURCES}")))
      << "the globbed icon list is no longer referenced by the QML module";
}

TEST(IconResourcesTest, NoIconOnDiskIsOrphanedFromTheRegistry) {
  const QStringList registered =
      registry_filenames() + registry_unit_family_filenames();
  const QSet<QString> known(registered.begin(), registered.end());

  QStringList orphans;
  const QStringList on_disk =
      icons_dir().entryList({QStringLiteral("*.png")}, QDir::Files);
  for (const QString& name : on_disk) {
    if (!known.contains(name)) {
      orphans.append(name);
    }
  }

  EXPECT_TRUE(orphans.isEmpty()) << "icons ship but nothing can request them: "
                                 << orphans.join(", ").toStdString();
}

} // namespace
