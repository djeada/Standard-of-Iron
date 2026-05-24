#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <sstream>
#include <string>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "assets" / "shaders" / "combat_dust.frag") &&
           std::filesystem::exists(path / "render" / "gl" / "backend" /
                                   "combat_dust_pipeline.cpp");
  };

  auto path = std::filesystem::path(__FILE__).parent_path();
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

void expect_literal_smoothstep_edges_are_ordered(const std::string& shader_source,
                                                 const char* shader_name) {
  const std::regex literal_call(
      R"(smoothstep\s*\(\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?))");
  for (auto it = std::sregex_iterator(
           shader_source.begin(), shader_source.end(), literal_call);
       it != std::sregex_iterator();
       ++it) {
    const float edge0 = std::stof((*it)[1].str());
    const float edge1 = std::stof((*it)[2].str());
    EXPECT_LT(edge0, edge1) << shader_name
                            << " contains reversed smoothstep edges: " << it->str();
  }
}

} // namespace

TEST(ShaderSource, CombatDustAvoidsUndefinedSmoothstepEdges) {
  const auto root = find_repo_root();
  const auto vert = read_text(root / "assets" / "shaders" / "combat_dust.vert");
  const auto frag = read_text(root / "assets" / "shaders" / "combat_dust.frag");
  ASSERT_FALSE(vert.empty());
  ASSERT_FALSE(frag.empty());

  expect_literal_smoothstep_edges_are_ordered(vert, "combat_dust.vert");
  expect_literal_smoothstep_edges_are_ordered(frag, "combat_dust.frag");

  EXPECT_NE(vert.find("float inv_smoothstep("), std::string::npos);
  EXPECT_NE(frag.find("float inv_smoothstep("), std::string::npos);
}

TEST(ShaderSource, InstancedRiggedShaderGuardsRoleColorFetches) {
  const auto root = find_repo_root();
  const auto vert =
      read_text(root / "assets" / "shaders" / "character_skinned_instanced.vert");
  const auto frag =
      read_text(root / "assets" / "shaders" / "character_skinned_instanced.frag");
  ASSERT_FALSE(vert.empty());
  ASSERT_FALSE(frag.empty());

  EXPECT_NE(vert.find("layout(location = 13) in vec4 i_role_meta;"), std::string::npos);
  EXPECT_NE(vert.find("flat out int v_role_color_count;"), std::string::npos);
  EXPECT_NE(frag.find("flat in int v_role_color_count;"), std::string::npos);
  EXPECT_NE(frag.find("v_color_role > 0 && v_color_role <= v_role_color_count"),
            std::string::npos);
}
