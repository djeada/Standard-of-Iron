#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "app" / "core" / "game_engine.cpp");
  };
  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_markers(path)) {
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
  if (auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
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

auto simulate_body(const std::string& source) -> std::string {
  const std::size_t begin = source.find("void GameEngine::simulate(float dt) {");
  if (begin == std::string::npos) {
    return {};
  }
  const std::size_t end = source.find("\nvoid GameEngine::", begin + 1);
  return source.substr(begin,
                       end == std::string::npos ? std::string::npos : end - begin);
}

} // namespace

TEST(MissionTickOwnershipTest, WavesAndStagesAdvanceOnTheConsumedSimulationStep) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const std::string body =
      simulate_body(read_text(root / "app" / "core" / "game_engine.cpp"));
  ASSERT_FALSE(body.empty());

  const std::size_t step_lambda = body.find("[this](float step_dt) {");
  ASSERT_NE(step_lambda, std::string::npos);

  const std::size_t waves = body.find("update_mission_waves(step_dt);");
  const std::size_t stages = body.find("update_mission_stages(step_dt);");

  EXPECT_NE(waves, std::string::npos)
      << "mission waves must be advanced by the fixed simulation step, not by the "
         "variable wall delta times the time scale";
  EXPECT_NE(stages, std::string::npos);
  EXPECT_GT(waves, step_lambda);
  EXPECT_GT(stages, step_lambda);
}

TEST(MissionTickOwnershipTest, NoMissionProgressionRunsOnTheScaledWallDelta) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const std::string body =
      simulate_body(read_text(root / "app" / "core" / "game_engine.cpp"));
  ASSERT_FALSE(body.empty());

  EXPECT_EQ(body.find("update_mission_waves(dt *"), std::string::npos);
  EXPECT_EQ(body.find("update_mission_stages(dt *"), std::string::npos);
}
