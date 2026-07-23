#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <locale>
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

auto parse_glsl_float(const std::string& text) -> float {
  std::istringstream stream(text);
  stream.imbue(std::locale::classic());
  float value = 0.0F;
  stream >> value;
  return value;
}

void expect_literal_smoothstep_edges_are_ordered(const std::string& shader_source,
                                                 const char* shader_name) {
  const std::regex literal_call(
      R"(smoothstep\s*\(\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?))");
  for (auto it = std::sregex_iterator(
           shader_source.begin(), shader_source.end(), literal_call);
       it != std::sregex_iterator();
       ++it) {
    const float edge0 = parse_glsl_float((*it)[1].str());
    const float edge1 = parse_glsl_float((*it)[2].str());
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

TEST(ShaderSource, RiggedCharactersUseSceneLightingAndCameraAwareReadability) {
  const auto root = find_repo_root();
  const auto single = read_text(root / "assets" / "shaders" / "character_skinned.frag");
  const auto instanced =
      read_text(root / "assets" / "shaders" / "character_skinned_instanced.frag");
  ASSERT_FALSE(single.empty());
  ASSERT_FALSE(instanced.empty());

  for (const auto* source : {&single, &instanced}) {
    EXPECT_NE(source->find("uniform vec3 u_light_dir;"), std::string::npos);
    EXPECT_NE(source->find("uniform float u_ambient_strength;"), std::string::npos);
    EXPECT_NE(source->find("uniform vec3 u_camera_position;"), std::string::npos);
    EXPECT_NE(source->find("shade_readable_character"), std::string::npos);
    EXPECT_NE(source->find("float readable_ambient = max(scene_ambient, 0.18);"),
              std::string::npos);
    EXPECT_NE(source->find("float rim = pow("), std::string::npos);
    EXPECT_NE(source->find("if (material_id == 2)"), std::string::npos);
    EXPECT_EQ(source->find("normalize(vec3(0.65, 0.50, 0.40))"), std::string::npos);
  }
}

TEST(ShaderSource, RiverbankCarriesBiomeMaterialsToTheWaterEdge) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "riverbank.frag");
  ASSERT_FALSE(frag.empty());

  for (const auto* uniform : {"uniform vec3 u_grass_secondary;",
                              "uniform vec3 u_grass_dry;",
                              "uniform vec3 u_soil_color;",
                              "uniform vec3 u_rock_low;",
                              "uniform vec3 u_rock_high;",
                              "uniform vec3 u_snow_color;",
                              "uniform float u_moisture_level;",
                              "uniform float u_rock_exposure;",
                              "uniform float u_snow_coverage;"}) {
    EXPECT_NE(frag.find(uniform), std::string::npos);
  }

  EXPECT_NE(frag.find("vec3 wet_silt =\n      soil *"), std::string::npos);
  EXPECT_NE(frag.find("mix(earth, map_ground"), std::string::npos);
  EXPECT_EQ(frag.find("if (shore_t > edge_limit)"), std::string::npos);
  EXPECT_EQ(frag.find("float ground_blend"), std::string::npos);
}

TEST(ShaderSource, TerrainGroundUsesCoherentBiomeMaterialPatches) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());

  EXPECT_NE(frag.find("float meadow_field = clamp("), std::string::npos);
  EXPECT_NE(frag.find("float thatch_field = clamp("), std::string::npos);
  EXPECT_NE(frag.find("float lush_patch = smoothstep("), std::string::npos);
  EXPECT_NE(frag.find("float soil_mix = bare_patch * 0.52;"), std::string::npos);
  EXPECT_NE(frag.find("0.055 * soil_mix"), std::string::npos);
}
