#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "render" / "scene_walk.cpp");
  };

  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_repo_markers(path)) {
        return path;
      }
      const auto parent = path.parent_path();
      if (parent == path) {
        break;
      }
      path = parent;
    }
    return {};
  };

  if (const auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
      !from_file.empty()) {
    return from_file;
  }
  return walk_up(std::filesystem::current_path());
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

} // namespace

TEST(SceneWalkEntityHandleTest, WorldWalkIteratesFullWidthEntityHandles) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const auto source = read_text(root / "render" / "scene_walk.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_EQ(source.find("for (std::uint32_t const entity_id :"), std::string::npos)
      << "scene_walk.cpp truncates an EntityID to 32 bits while walking the world";
  EXPECT_EQ(source.find("[&](std::uint32_t entity_id,"), std::string::npos)
      << "scene_walk.cpp truncates an EntityID to 32 bits while collecting entries";

  for (const char* id_list : {"unit_ids", "building_ids", "other_ids"}) {
    const std::string loop =
        std::string("for (Engine::Core::EntityID const entity_id : ") + id_list + ")";
    EXPECT_NE(source.find(loop), std::string::npos)
        << "expected a full-width handle loop over " << id_list;
  }
}
