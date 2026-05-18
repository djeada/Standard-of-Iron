#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "ui" / "gl_view.cpp") &&
           std::filesystem::exists(path / "render" / "scene_renderer.cpp");
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
  if (const auto from_cwd = walk_up(std::filesystem::current_path());
      !from_cwd.empty()) {
    return from_cwd;
  }
  return std::filesystem::current_path();
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

auto contains(const std::string& text, const std::string& needle) -> bool {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST(RenderThreadBoundaryTest, GlRendererRunsSimulationBeforeRenderSubmit) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "gl_view.cpp");
  ASSERT_FALSE(source.empty());

  const auto update_pos = source.find("m_engine->update(dt);");
  const auto render_pos = source.find("m_engine->render(");

  ASSERT_NE(update_pos, std::string::npos);
  ASSERT_NE(render_pos, std::string::npos);
  EXPECT_LT(update_pos, render_pos);
}

TEST(RenderThreadBoundaryTest, RenderSubmitDoesNotCallCombatQuerySearches) {
  const auto root = find_repo_root();
  const std::vector<std::filesystem::path> render_sources = {
      root / "render" / "scene_renderer.cpp",
      root / "render" / "gl" / "backend.cpp",
  };
  const std::vector<std::string> forbidden_tokens = {
      "combat_system/combat_utils",
      "build_combat_query_context",
      "rebuild_combat_query_context",
      "find_nearest_enemy(",
      "process_attacks(",
  };

  for (const auto& path : render_sources) {
    const auto source = read_text(path);
    ASSERT_FALSE(source.empty()) << path;
    for (const auto& token : forbidden_tokens) {
      EXPECT_FALSE(contains(source, token)) << path << " contains " << token;
    }
  }
}

TEST(RenderThreadBoundaryTest, StageDocumentationNamesBoundariesAndLogging) {
  const auto root = find_repo_root();
  const auto docs = read_text(root / "docs" / "RENDERING_ARCHITECTURE.md");
  ASSERT_FALSE(docs.empty());

  EXPECT_TRUE(contains(docs, "QSG render-thread stages"));
  EXPECT_TRUE(contains(docs, "GameEngine::update"));
  EXPECT_TRUE(contains(docs, "GameEngine::render"));
  EXPECT_TRUE(contains(docs, "Renderer::render_world"));
  EXPECT_TRUE(contains(docs, "Backend::execute"));
  EXPECT_TRUE(contains(docs, "SOI_RENDER_STAGE_LOG"));
}
