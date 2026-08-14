

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

const std::set<QString>& extracted_targets() {
  static const std::set<QString> entries{
      "soi_ai",
      "soi_campaign",
      "soi_missions",
      "soi_persistence",
      "soi_runtime",
      "game_view",
  };
  return entries;
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

TEST(ModuleBoundaries, NothingDependsOnAModuleThatLeftTheKernel) {
  const auto root = find_repo_root();
  const auto rules = module_rules(root);
  ASSERT_FALSE(rules.isEmpty());

  const auto modules = rules.value("modules").toObject();
  std::set<QString> extracted;
  for (const auto& name : modules.keys()) {
    for (const auto& target :
         modules.value(name).toObject().value("targets").toArray()) {
      if (extracted_targets().contains(target.toString())) {
        extracted.insert(name);
      }
    }
  }
  EXPECT_FALSE(extracted.empty())
      << "no module claims one of the extracted CMake targets";

  const auto baseline = rules.value("baseline").toObject();
  for (const auto& pair : baseline.keys()) {
    const auto parts = pair.split(" -> ");
    ASSERT_EQ(parts.size(), 2) << "malformed baseline key: " << pair.toStdString();
    EXPECT_EQ(extracted.count(parts.at(1)), 0U)
        << "the baseline records an edge into " << parts.at(1).toStdString()
        << ", which is its own CMake target. That edge cannot exist -- the kernel "
           "does not link it. Either the module map is wrong or the target was "
           "folded back in.";
  }
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
