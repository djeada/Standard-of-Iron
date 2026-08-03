#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace {

namespace fs = std::filesystem;

auto find_repo_root() -> fs::path {
  fs::path current = fs::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(current / "app" / "core" / "game_engine.h")) {
      return current;
    }
    if (!current.has_parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return fs::current_path();
}

auto count_occurrences(const fs::path& file, const std::string& needle) -> int {
  std::ifstream stream(file);
  std::string line;
  int count = 0;
  while (std::getline(stream, line)) {
    if (line.find(needle) != std::string::npos) {
      ++count;
    }
  }
  return count;
}

constexpr int k_max_invokables = 103;

constexpr int k_max_properties = 41;

constexpr const char* k_guidance =
    "\nGameEngine is the composition root, not the UI API. New QML-facing "
    "members belong on a view model that owns that slice of the interface -- "
    "see app/viewmodels/save_slots_view_model.h for the shape -- exposed from "
    "GameEngine as a single CONSTANT property.\n"
    "If you removed members instead, lower the ceiling in this test so it keeps "
    "ratcheting down.";

TEST(QmlSurface, GameEngineDoesNotGrowNewInvokables) {
  const auto header = find_repo_root() / "app" / "core" / "game_engine.h";
  ASSERT_TRUE(fs::exists(header));

  const int invokables = count_occurrences(header, "Q_INVOKABLE");
  EXPECT_LE(invokables, k_max_invokables)
      << "GameEngine now exposes " << invokables << " Q_INVOKABLE members (ceiling "
      << k_max_invokables << ")." << k_guidance;
}

TEST(QmlSurface, GameEngineDoesNotGrowNewProperties) {
  const auto header = find_repo_root() / "app" / "core" / "game_engine.h";
  ASSERT_TRUE(fs::exists(header));

  const int properties = count_occurrences(header, "Q_PROPERTY");
  EXPECT_LE(properties, k_max_properties)
      << "GameEngine now exposes " << properties << " Q_PROPERTY members (ceiling "
      << k_max_properties << ")." << k_guidance;
}

TEST(QmlSurface, CeilingsStayTight) {

  const auto header = find_repo_root() / "app" / "core" / "game_engine.h";
  ASSERT_TRUE(fs::exists(header));

  EXPECT_GE(count_occurrences(header, "Q_INVOKABLE"), k_max_invokables - 10)
      << "members were removed; lower k_max_invokables to lock the win in";
  EXPECT_GE(count_occurrences(header, "Q_PROPERTY"), k_max_properties - 10)
      << "members were removed; lower k_max_properties to lock the win in";
}

TEST(QmlSurface, PlacementLivesOnItsOwnViewModel) {
  const auto root = find_repo_root();
  const auto engine = root / "app" / "core" / "game_engine.h";
  const auto view_model = root / "app" / "viewmodels" / "placement_view_model.h";
  ASSERT_TRUE(fs::exists(engine));
  ASSERT_TRUE(fs::exists(view_model));

  for (const char* member : {"on_formation_confirm",
                             "on_construction_confirm",
                             "construction_preview_valid",
                             "pending_builder_construction_type",
                             "start_builder_construction",
                             "start_building_placement",
                             "get_construction_info"}) {
    EXPECT_EQ(count_occurrences(engine, member), 0)
        << member << " is still declared on GameEngine";
    EXPECT_GT(count_occurrences(view_model, member), 0)
        << member << " is missing from PlacementViewModel";
  }
}

TEST(QmlSurface, SaveSlotBrowsingLivesOnItsOwnViewModel) {
  const auto root = find_repo_root();
  const auto engine = root / "app" / "core" / "game_engine.h";
  const auto view_model = root / "app" / "viewmodels" / "save_slots_view_model.h";
  ASSERT_TRUE(fs::exists(engine));
  ASSERT_TRUE(fs::exists(view_model));

  for (const char* member : {"get_save_slots",
                             "delete_save_slot",
                             "verify_save_slot",
                             "export_save_slot",
                             "import_save_file",
                             "autosave_slot_count"}) {
    EXPECT_EQ(count_occurrences(engine, member), 0)
        << member << " is still declared on GameEngine";
    EXPECT_GT(count_occurrences(view_model, member), 0)
        << member << " is missing from SaveSlotsViewModel";
  }
}

} // namespace
