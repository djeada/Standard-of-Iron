#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <regex>
#include <set>
#include <string>

namespace {

auto read_qml(const char* relative) -> std::string {
  for (auto root = std::filesystem::current_path(); !root.empty();
       root = root.parent_path()) {
    if (std::filesystem::exists(root / relative)) {
      std::ifstream file(root / relative);
      return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    if (root == root.root_path()) {
      break;
    }
  }
  return {};
}

auto matches(const std::string& text,
             const std::regex& pattern) -> std::set<std::string> {
  std::set<std::string> found;
  for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end;
       ++it) {
    found.insert((*it)[1].str());
  }
  return found;
}

auto item_z(const std::string& qml, const std::string& id) -> double {
  std::smatch found;
  std::regex const pattern("id: " + id + "\\n(?:[^\\n{}]*\\n)*?\\s*z: ([0-9.]+)\\n");
  return std::regex_search(qml, found, pattern) ? std::stod(found[1].str()) : -1.0;
}

// An undeclared property reads as undefined in QML, so `!view.ready` stays true
// and a screen waiting on it never goes away.
TEST(StartupScreens, MainReadsOnlyMembersTheGameViewDeclares) {
  auto const main_qml = read_qml("ui/qml/Main.qml");
  auto const view_qml = read_qml("ui/qml/GameView.qml");
  ASSERT_FALSE(main_qml.empty());
  ASSERT_FALSE(view_qml.empty());

  auto const used = matches(main_qml, std::regex(R"(\bgameViewItem\.(\w+))"));
  auto declared = matches(
      view_qml,
      std::regex(R"(\n    (?:readonly )?property [\w<>]+ (\w+)|\n    function \w+)"));
  auto const functions = matches(view_qml, std::regex(R"(\n    function (\w+)\()"));
  declared.insert(functions.begin(), functions.end());
  std::set<std::string> const from_item{"forceActiveFocus"};

  ASSERT_TRUE(used.count("renderer_ready") == 1U);
  for (auto const& name : used) {
    EXPECT_TRUE(declared.count(name) == 1U || from_item.count(name) == 1U)
        << "Main.qml reads gameViewItem." << name
        << ", which GameView.qml does not declare";
  }
}

TEST(StartupScreens, LoadAndBootScreensCoverEveryMenuScreen) {
  auto const main_qml = read_qml("ui/qml/Main.qml");
  double const load = item_z(main_qml, "load_screen");
  ASSERT_GT(load, 0.0);
  for (auto const* screen : {"mainMenu",
                             "mapSelect",
                             "missions_screen",
                             "campaign_screen",
                             "save_game_panel",
                             "load_game_panel",
                             "settingsPanel",
                             "objectivesPanel",
                             "help_panel"}) {
    double const z = item_z(main_qml, screen);
    ASSERT_GE(z, 0.0) << screen;
    EXPECT_GT(load, z) << screen << " would stay on top of the load screen";
  }
}

} // namespace
