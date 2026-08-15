

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

auto find_repo_root() -> fs::path {
  fs::path current = fs::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(current / "CMakeLists.txt") && fs::exists(current / "scripts") &&
        fs::exists(current / "game")) {
      return current;
    }
    if (!current.has_parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return fs::current_path();
}

auto read_text(const fs::path& path) -> QString {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

auto module_rules(const fs::path& root) -> QJsonObject {
  QFile file(QString::fromStdString((root / "scripts" / "module_rules.json").string()));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  QJsonParseError error{};
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  EXPECT_EQ(error.error, QJsonParseError::NoError)
      << "scripts/module_rules.json is not valid JSON: "
      << error.errorString().toStdString();
  return document.object();
}

auto declared_libraries(const QString& lists) -> std::set<QString> {
  std::set<QString> names;
  static const QRegularExpression pattern(
      R"(add_library\(\s*(\w+)\s+(?:STATIC|OBJECT|SHARED)\b)");
  auto it = pattern.globalMatch(lists);
  while (it.hasNext()) {
    names.insert(it.next().captured(1));
  }
  return names;
}

auto shell_suite_list(const QString& script) -> QStringList {
  const auto start = script.indexOf("suites=(");
  if (start < 0) {
    return {};
  }
  const auto end = script.indexOf(')', start);
  if (end < 0) {
    return {};
  }
  const auto body = script.mid(start + 8, end - start - 8);
  QStringList suites;
  for (const auto& token : body.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
    suites.push_back(token.trimmed());
  }
  suites.sort();
  return suites;
}

auto cmake_suite_list(const QString& lists) -> QStringList {
  const auto start = lists.indexOf("soi_test_binaries\n");
  if (start < 0) {
    return {};
  }
  const auto end = lists.indexOf(")", start);
  if (end < 0) {
    return {};
  }
  const auto body = lists.mid(start, end - start);
  QStringList suites;
  for (const auto& token : body.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
    if (token.endsWith("_tests")) {
      suites.push_back(token);
    }
  }
  suites.sort();
  return suites;
}

} // namespace

TEST(ModuleBoundaries, EveryModuleNamesNeighboursThatExist) {
  const auto root = find_repo_root();
  const auto rules = module_rules(root);
  ASSERT_FALSE(rules.isEmpty()) << "scripts/module_rules.json not found from " << root;

  const auto modules = rules.value("modules").toObject();
  ASSERT_FALSE(modules.isEmpty());

  const auto names = modules.keys();
  for (const auto& name : names) {
    const auto spec = modules.value(name).toObject();
    const auto claims =
        spec.value("paths").toArray().size() + spec.value("files").toArray().size();
    EXPECT_GT(claims, 0) << "module " << name.toStdString() << " claims no files";

    for (const auto& entry : spec.value("may_use").toArray()) {
      const auto neighbour = entry.toString();
      EXPECT_TRUE(names.contains(neighbour))
          << "module " << name.toStdString() << " may_use names "
          << neighbour.toStdString() << ", which is not a module";
      EXPECT_NE(neighbour, name)
          << "module " << name.toStdString() << " lists itself in may_use";
    }
  }
}

TEST(ModuleBoundaries, EveryModuleIsBackedByACmakeTargetTheLinkerEnforces) {
  const auto root = find_repo_root();
  const auto rules = module_rules(root);
  ASSERT_FALSE(rules.isEmpty());

  const auto lists = read_text(root / "game" / "CMakeLists.txt") +
                     read_text(root / "game" / "audio" / "CMakeLists.txt");
  ASSERT_FALSE(lists.isEmpty());
  const auto libraries = declared_libraries(lists);
  ASSERT_FALSE(libraries.empty())
      << "could not parse add_library() in game/CMakeLists.txt";

  const auto modules = rules.value("modules").toObject();
  for (const auto& name : modules.keys()) {
    const auto targets = modules.value(name).toObject().value("targets").toArray();
    EXPECT_FALSE(targets.isEmpty())
        << "module " << name.toStdString()
        << " names no CMake target. Every module is shipped by a static library so "
           "that a wrong-way edge is a link error, not just a script finding.";
    for (const auto& target : targets) {
      EXPECT_TRUE(libraries.count(target.toString()) > 0)
          << "module " << name.toStdString() << " says it is built into "
          << target.toString().toStdString()
          << ", which neither game/CMakeLists.txt nor game/audio/CMakeLists.txt "
             "declares.";
    }
  }
}

TEST(ModuleBoundaries, NoWrongWayEdgeIsToleratedAnyMore) {
  const auto root = find_repo_root();
  const auto rules = module_rules(root);
  ASSERT_FALSE(rules.isEmpty());

  EXPECT_FALSE(rules.contains("baseline"))
      << "scripts/module_rules.json has grown a baseline again. Fix the edge.";
}

TEST(ModuleBoundaries, TestBinariesLinkProductionCodeRatherThanRecompilingIt) {
  const auto root = find_repo_root();
  const auto lists = read_text(root / "tests" / "CMakeLists.txt");
  ASSERT_FALSE(lists.isEmpty());

  std::vector<std::string> offenders;
  const auto lines = lists.split('\n');
  for (int index = 0; index < lines.size(); ++index) {
    auto line = lines.at(index).trimmed();
    if (line.startsWith('#') || !line.endsWith(".cpp")) {
      continue;
    }

    if (line.contains("${CMAKE_SOURCE_DIR}") || line.startsWith("../")) {
      offenders.push_back(QString("tests/CMakeLists.txt:%1: %2")
                              .arg(index + 1)
                              .arg(line)
                              .toStdString());
    }
  }

  EXPECT_TRUE(offenders.empty())
      << "a test binary compiles a production source instead of linking the "
         "target that ships it. A separately compiled copy can pass while the "
         "object the game ships is broken:\n  "
      << [&] {
           std::string joined;
           for (const auto& entry : offenders) {
             joined += entry + "\n  ";
           }
           return joined;
         }();
}

TEST(ModuleBoundaries, TheSuiteListsCiRunsAndCmakeBuildsAgree) {
  const auto root = find_repo_root();
  const auto script = read_text(root / "scripts" / "run-tests.sh");
  const auto lists = read_text(root / "tests" / "CMakeLists.txt");
  ASSERT_FALSE(script.isEmpty());
  ASSERT_FALSE(lists.isEmpty());

  const auto from_shell = shell_suite_list(script);
  const auto from_cmake = cmake_suite_list(lists);

  ASSERT_FALSE(from_shell.isEmpty())
      << "could not parse the suites=() array in run-tests.sh";
  ASSERT_FALSE(from_cmake.isEmpty())
      << "could not parse soi_test_binaries in tests/CMakeLists.txt";

  EXPECT_EQ(from_shell, from_cmake)
      << "scripts/run-tests.sh and soi_test_binaries disagree about which suites "
         "exist. A binary the build produces but the script never runs is a suite "
         "nobody is checking.\n  run-tests.sh: "
      << from_shell.join(", ").toStdString()
      << "\n  CMake:       " << from_cmake.join(", ").toStdString();
}
