#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <locale>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "render/gl/directional_shadow_block.h"
#include "render/local_lighting.h"

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

// Several of these shaders now keep their shared shading in
// assets/shaders/include/*.glsl, exactly as the engine resolves it before
// compiling. Reading a stage with its includes inlined keeps the assertions
// pointed at the code that actually runs.
auto inline_shader_includes(const std::filesystem::path& shader_dir,
                            const std::string& source,
                            std::vector<std::string>& already_included) -> std::string {
  std::istringstream input(source);
  std::string line;
  std::string resolved;
  while (std::getline(input, line)) {
    const auto include_at = line.find("#include \"");
    if (include_at != std::string::npos) {
      const auto start = line.find('"', include_at);
      const auto end = line.find('"', start + 1);
      if (start != std::string::npos && end != std::string::npos) {
        const std::string included = line.substr(start + 1, end - start - 1);
        if (std::find(already_included.begin(), already_included.end(), included) !=
            already_included.end()) {
          continue;
        }
        already_included.push_back(included);
        resolved += inline_shader_includes(
            shader_dir, read_text(shader_dir / "include" / included), already_included);
        resolved += '\n';
        continue;
      }
    }
    resolved += line;
    resolved += '\n';
  }
  return resolved;
}

auto read_shader_with_includes(const std::filesystem::path& repo_root,
                               const std::string& name) -> std::string {
  const auto shader_dir = repo_root / "assets" / "shaders";
  std::vector<std::string> already_included;
  return inline_shader_includes(
      shader_dir, read_text(shader_dir / name), already_included);
}

auto collapse_whitespace(const std::string& text) -> std::string {
  std::string collapsed;
  collapsed.reserve(text.size());
  bool in_space = false;
  for (const char character : text) {
    if (std::isspace(static_cast<unsigned char>(character)) != 0) {
      in_space = true;
      continue;
    }
    if (in_space && !collapsed.empty()) {
      collapsed.push_back(' ');
    }
    in_space = false;
    collapsed.push_back(character);
  }
  return collapsed;
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

struct Std140Block {
  std::vector<std::string> member_names;
  std::size_t float_count = 0;
};

auto parse_std140_block(const std::string& glsl,
                        const std::string& block_name,
                        const std::map<std::string, std::size_t>& array_extents)
    -> Std140Block {
  Std140Block parsed;
  const std::regex block(R"(layout\(std140\)\s+uniform\s+)" + block_name +
                         R"(\s*\{([^}]*)\})");
  std::smatch match;
  if (!std::regex_search(glsl, match, block)) {
    return parsed;
  }
  const std::string body = match[1].str();

  const std::regex member(
      R"((mat4|vec4|vec3|vec2|float|int)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[\s*([A-Za-z0-9_]+)\s*\])?\s*;)");
  for (auto it = std::sregex_iterator(body.begin(), body.end(), member);
       it != std::sregex_iterator();
       ++it) {
    const std::string type = (*it)[1].str();
    parsed.member_names.push_back((*it)[2].str());

    std::size_t elements = 1;
    if ((*it)[3].matched) {
      const std::string extent = (*it)[3].str();
      const auto named = array_extents.find(extent);
      elements = named != array_extents.end()
                     ? named->second
                     : static_cast<std::size_t>(std::stoul(extent));
    }

    const std::size_t slots_per_element = type == "mat4" ? 4U : 1U;
    parsed.float_count += elements * slots_per_element * 4U;
  }
  return parsed;
}

auto glsl_int_constant(const std::string& glsl, const std::string& name) -> int {
  std::smatch match;
  const std::regex declaration(R"(const\s+int\s+)" + name + R"(\s*=\s*([0-9]+)\s*;)");
  if (!std::regex_search(glsl, match, declaration)) {
    return -1;
  }
  return std::stoi(match[1].str());
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

TEST(ShaderSource, GpuInstancedRiggedShaderGuardsRoleColorFetches) {
  const auto root = find_repo_root();
  const auto vert =
      read_text(root / "assets" / "shaders" / "character_skinned_gpu_instanced.vert");
  const auto frag =
      read_text(root / "assets" / "shaders" / "character_skinned_gpu_instanced.frag");
  ASSERT_FALSE(vert.empty());
  ASSERT_FALSE(frag.empty());

  EXPECT_NE(vert.find("RiggedInstanceData data"), std::string::npos);
  EXPECT_NE(vert.find("v_instance_id = int(data.role_meta.y);"), std::string::npos);
  EXPECT_NE(vert.find("flat out int v_role_color_count;"), std::string::npos);
  EXPECT_NE(frag.find("flat in int v_role_color_count;"), std::string::npos);
  EXPECT_NE(frag.find("v_color_role > 0 && v_color_role <= v_role_color_count"),
            std::string::npos);
}

TEST(ShaderSource, HealerAuraKeepsTheUnitReadable) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "healing_aura.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("float perimeter_mask = smoothstep(0.56, 0.94"),
            std::string::npos);
  EXPECT_NE(flat.find("clamp(aura_alpha, 0.0, 0.12)"), std::string::npos);
  EXPECT_NE(flat.find("veil * (0.018 + filament * 0.082) * u_intensity"),
            std::string::npos);
  EXPECT_EQ(flat.find("shell_fade * u_intensity * 0.7"), std::string::npos);
  EXPECT_EQ(flat.find("clamp(alpha, 0.0, 0.85)"), std::string::npos);
}

TEST(ShaderSource, RiggedCharactersUseSceneLightingAndCameraAwareReadability) {
  const auto root = find_repo_root();
  const auto single = read_shader_with_includes(root, "character_skinned.frag");
  const auto instanced =
      read_shader_with_includes(root, "character_skinned_gpu_instanced.frag");
  ASSERT_FALSE(single.empty());
  ASSERT_FALSE(instanced.empty());

  for (const auto* source : {&single, &instanced}) {
    // Resolved sources no longer carry the directive, so check that the shared
    // environment block itself came through.
    EXPECT_NE(source->find("layout(std140) uniform EnvironmentLighting"),
              std::string::npos);
    EXPECT_NE(source->find("uniform vec3 u_camera_position;"), std::string::npos);
    EXPECT_NE(source->find("shade_readable_character"), std::string::npos);
    EXPECT_NE(source->find("environment_primary_direction()"), std::string::npos);
    EXPECT_NE(source->find("environment_ambient_intensity()"), std::string::npos);
    EXPECT_NE(source->find("float readable_ambient = max(scene_ambient, 0.22);"),
              std::string::npos);
    EXPECT_NE(source->find("k_readable_shadow_floor"), std::string::npos);
    EXPECT_NE(source->find("float rim = pow("), std::string::npos);

    EXPECT_NE(source->find("color_role == k_humanoid_role_metal"), std::string::npos);
    EXPECT_NE(source->find("color_role == k_humanoid_role_leather"), std::string::npos);
    EXPECT_NE(source->find("color_role == k_humanoid_role_skin"), std::string::npos);
    EXPECT_EQ(source->find("normalize(vec3(0.65, 0.50, 0.40))"), std::string::npos);
  }
}

TEST(ShaderSource, EveryWorldLightingVariantUsesSharedEnvironmentBlock) {
  const auto root = find_repo_root();
  const std::vector<std::string> shaders{
      "basic.frag",
      "basic_instanced.frag",
      "character_skinned.frag",
      "character_skinned_gpu_instanced.frag",
      "catapult.frag",
      "catapult_instanced.frag",
      "terrain_chunk.frag",
      "ground_plane.frag",
      "grass_instanced.vert",
      "stone_instanced.frag",
      "plant_instanced.frag",
      "pine_instanced.frag",
      "olive_instanced.frag",
      "tent_instanced.frag",
      "supply_cart_instanced.frag",
      "weapon_rack_instanced.frag",
      "ruins_instanced.frag",
      "dead_tree_instanced.frag",
      "iron_ore_instanced.frag",
      "magic_shrine_instanced.frag",
      "statue_instanced.frag",
      "primitive_instanced.frag",
      "cylinder_instanced.frag",
      "banner.frag",
      "bridge.frag",
      "road.frag",
      "river.frag",
      "riverbank.frag",
  };

  for (const auto& name : shaders) {
    const auto source = read_text(root / "assets" / "shaders" / name);
    ASSERT_FALSE(source.empty()) << name;
    EXPECT_NE(source.find("#include \"environment_lighting.glsl\""), std::string::npos)
        << name;
  }

  const auto common =
      read_text(root / "assets" / "shaders" / "include" / "environment_lighting.glsl");
  ASSERT_FALSE(common.empty());
  EXPECT_NE(common.find("layout(std140) uniform EnvironmentLighting"),
            std::string::npos);
  for (const auto* field : {"u_env_primary_direction_intensity",
                            "u_env_primary_color_ambient_intensity",
                            "u_env_sky_color_exposure",
                            "u_env_ground_bounce_color_fog_density",
                            "u_env_fog_color_cloud_cover",
                            "u_env_shadow_tint_strength",
                            "u_env_shadow_softness_wetness"}) {
    EXPECT_NE(common.find(field), std::string::npos) << field;
  }
}

TEST(ShaderSource, WorldShadersLeaveGradingToThePostProcessPass) {
  const auto root = find_repo_root();
  const std::vector<std::string> shaders{
      "basic.frag",
      "basic_instanced.frag",
      "character_skinned.frag",
      "character_skinned_gpu_instanced.frag",
      "terrain_chunk.frag",
      "ground_plane.frag",
      "stone_instanced.frag",
      "pine_instanced.frag",
      "olive_instanced.frag",
      "road.frag",
      "river.frag",
      "riverbank.frag",
      "bridge.frag",
  };

  for (const auto& name : shaders) {
    const auto source = read_text(root / "assets" / "shaders" / name);
    ASSERT_FALSE(source.empty()) << name;
    EXPECT_EQ(source.find("soi_finalize("), std::string::npos)
        << name << " must write linear radiance; the post pass owns the tone curve";
  }

  const auto grade =
      read_text(root / "assets" / "shaders" / "include" / "tonemap.glsl");
  ASSERT_FALSE(grade.empty());
  EXPECT_NE(grade.find("vec3 soi_tonemap(vec3 linear_color)"), std::string::npos);
  EXPECT_NE(grade.find("vec3 soi_grade(vec3 mapped_color)"), std::string::npos);
  EXPECT_NE(grade.find("vec3 soi_finalize(vec3 linear_color)"), std::string::npos);
  EXPECT_NE(grade.find("k_soi_grade_shadow_lift"), std::string::npos);

  const auto composite = read_text(root / "assets" / "shaders" / "post_composite.frag");
  ASSERT_FALSE(composite.empty());
  EXPECT_NE(composite.find("#include \"tonemap.glsl\""), std::string::npos);
  EXPECT_NE(composite.find("soi_finalize(combined)"), std::string::npos);
  EXPECT_NE(composite.find("u_bloom"), std::string::npos);
  EXPECT_NE(composite.find("u_vignette_strength"), std::string::npos);

  const auto fxaa = read_text(root / "assets" / "shaders" / "post_fxaa.frag");
  ASSERT_FALSE(fxaa.empty());
  EXPECT_NE(fxaa.find("u_inverse_resolution"), std::string::npos);
}

TEST(ShaderSource, WorldSurfacesTakeTheirKeyLightFromTheSharedModel) {
  const auto root = find_repo_root();
  const std::vector<std::string> shaders{
      "bridge.frag",
      "road.frag",
      "riverbank.frag",
      "ground_plane.frag",
      "banner.frag",
      "dead_tree_instanced.frag",
      "stone_instanced.frag",
      "tent_instanced.frag",
      "ruins_instanced.frag",
      "weapon_rack_instanced.frag",
      "supply_cart_instanced.frag",
      "statue_instanced.frag",
      "magic_shrine_instanced.frag",
      "iron_ore_instanced.frag",
      "pine_instanced.frag",
      "olive_instanced.frag",
  };

  for (const auto& name : shaders) {
    const auto source = read_text(root / "assets" / "shaders" / name);
    ASSERT_FALSE(source.empty()) << name;
    const bool routed = source.find("soi_key_light(") != std::string::npos ||
                        source.find("soi_surface_lighting") != std::string::npos;
    EXPECT_TRUE(routed)
        << name
        << " multiplies the sun in by hand, so its terminator no longer matches the "
           "wrapped-and-banded curve every other surface shades with";
  }

  const auto common =
      read_text(root / "assets" / "shaders" / "include" / "environment_lighting.glsl");
  ASSERT_FALSE(common.empty());
  EXPECT_LT(common.find("vec3 soi_key_light(vec3 normal)"),
            common.find("vec3 environment_direct_light("))
      << "GLSL has no forward declarations: the shared shading helpers must stay "
         "above their first caller in this include";
}

TEST(ShaderSource, GeneralWorldShadersDoNotHardCodeDaylightColors) {
  const auto root = find_repo_root();
  for (const auto* name : {"basic.frag",
                           "basic_instanced.frag",
                           "character_skinned.frag",
                           "character_skinned_gpu_instanced.frag",
                           "terrain_chunk.frag",
                           "ground_plane.frag"}) {
    const auto source = read_text(root / "assets" / "shaders" / name);
    ASSERT_FALSE(source.empty()) << name;
    EXPECT_EQ(source.find("vec3 sun_color = vec3("), std::string::npos) << name;
    EXPECT_EQ(source.find("vec3 sky_color = vec3("), std::string::npos) << name;
    EXPECT_EQ(source.find("vec3 sun_light = vec3("), std::string::npos) << name;
    EXPECT_EQ(source.find("vec3 sky_light = vec3("), std::string::npos) << name;
  }
}

TEST(ShaderSource, MainWorldReceiversUseDirectionalAndLocalLighting) {
  const auto root = find_repo_root();
  for (const auto* name : {"basic.frag",
                           "basic_instanced.frag",
                           "character_skinned.frag",
                           "character_skinned_gpu_instanced.frag",
                           "terrain_chunk.frag",
                           "ground_plane.frag"}) {
    const auto source = read_text(root / "assets" / "shaders" / name);
    ASSERT_FALSE(source.empty()) << name;
    EXPECT_NE(source.find("#include \"local_lighting.glsl\""), std::string::npos)
        << name;
    EXPECT_NE(source.find("#include \"directional_shadows.glsl\""), std::string::npos)
        << name;
    EXPECT_NE(source.find("apply_directional_shadow("), std::string::npos) << name;
  }

  const auto shadow =
      read_text(root / "assets" / "shaders" / "include" / "directional_shadows.glsl");
  EXPECT_NE(shadow.find("u_shadow_light_vp[SOI_MAX_SHADOW_CASCADES]"),
            std::string::npos);
  EXPECT_NE(shadow.find("texture(u_directional_shadow_map"), std::string::npos);
  EXPECT_NE(shadow.find("uniform sampler2DArrayShadow u_directional_shadow_map;"),
            std::string::npos)
      << "receivers rely on hardware depth-compare PCF; the backend sets "
         "GL_TEXTURE_COMPARE_MODE on the cascade array to match";
  EXPECT_NE(shadow.find("for (int y = -2; y <= 2; ++y)"), std::string::npos);
  EXPECT_NE(shadow.find("u_shadow_cascade_texel_world[cascade]"), std::string::npos)
      << "biases and the penumbra are authored in world metres per cascade";
}

TEST(ShaderSource, OnlyTheShaderClassTalksToGlUniform) {

  const auto root = find_repo_root();
  std::vector<std::string> offenders;
  for (const auto* subdir : {"render", "ui", "app", "tools"}) {
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root / subdir)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const auto ext = entry.path().extension().string();
      if (ext != ".cpp" && ext != ".h") {
        continue;
      }
      const auto relative =
          std::filesystem::relative(entry.path(), root).generic_string();
      if (relative == "render/gl/shader.cpp" || relative == "render/gl/shader.h") {
        continue;
      }
      const auto source = read_text(entry.path());
      if (source.find("glUniform") != std::string::npos) {
        offenders.push_back(relative);
      }
    }
  }
  EXPECT_TRUE(offenders.empty()) << offenders.front();
}

TEST(ShaderSource, EveryShaderFileIsEmbeddedInTheResourceBundle) {

  const auto root = find_repo_root();
  const auto qrc = read_text(root / "assets.qrc");
  ASSERT_FALSE(qrc.empty());
  std::vector<std::string> missing;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(root / "assets" / "shaders")) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext != ".frag" && ext != ".vert" && ext != ".glsl" && ext != ".comp") {
      continue;
    }
    const auto relative =
        std::filesystem::relative(entry.path(), root).generic_string();
    if (qrc.find("<file>" + relative + "</file>") == std::string::npos) {
      missing.push_back(relative);
    }
  }
  EXPECT_TRUE(missing.empty()) << "not in assets.qrc: " << missing.front();
}

TEST(ShaderSource, QualityTierIsCompiledNotBranched) {
  const auto root = find_repo_root();
  const auto quality =
      read_text(root / "assets" / "shaders" / "include" / "quality.glsl");
  ASSERT_FALSE(quality.empty());

  EXPECT_NE(quality.find("#ifndef SOI_QUALITY_TIER"), std::string::npos);
  EXPECT_NE(quality.find("#define SOI_TERRAIN_NOISE_OCTAVES"), std::string::npos);
  EXPECT_NE(quality.find("#define SOI_SURFACE_DETAIL"), std::string::npos);
  EXPECT_NE(quality.find("#define SOI_ULTRA_EFFECTS"), std::string::npos);

  const auto environment =
      read_text(root / "assets" / "shaders" / "include" / "environment_lighting.glsl");
  EXPECT_NE(environment.find("#include \"quality.glsl\""), std::string::npos)
      << "every lit shader reaches the tier macros through environment_lighting";

  for (const auto& entry :
       std::filesystem::directory_iterator(root / "assets" / "shaders")) {
    const auto ext = entry.path().extension().string();
    if (ext != ".frag" && ext != ".vert") {
      continue;
    }
    const auto source = read_text(entry.path());
    EXPECT_EQ(source.find("u_noise_octaves"), std::string::npos) << entry.path();
    EXPECT_EQ(source.find("u_shader_quality"), std::string::npos) << entry.path();
  }

  const auto terrain = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  EXPECT_NE(terrain.find("SOI_TERRAIN_NOISE_OCTAVES"), std::string::npos);
  EXPECT_NE(terrain.find("#if SOI_SURFACE_DETAIL"), std::string::npos);
  const auto shadows =
      read_text(root / "assets" / "shaders" / "include" / "directional_shadows.glsl");
  EXPECT_NE(shadows.find("#if SOI_SHADOW_PCSS"), std::string::npos);
  EXPECT_NE(shadows.find("SOI_SHADOW_PCF_RADIUS"), std::string::npos);
  const auto grass = read_text(root / "assets" / "shaders" / "grass_instanced.frag");
  EXPECT_NE(grass.find("#if SOI_ULTRA_EFFECTS"), std::string::npos);
}

TEST(ShaderSource, RiverbankCarriesBiomeMaterialsToTheWaterEdge) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "riverbank.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  for (const auto* uniform : {"uniform vec3 u_grass_secondary;",
                              "uniform vec3 u_grass_dry;",
                              "uniform vec3 u_soil_color;",
                              "uniform vec3 u_rock_low;",
                              "uniform vec3 u_rock_high;",
                              "uniform vec3 u_snow_color;",
                              "uniform int u_ground_type;",
                              "uniform float u_moisture_level;",
                              "uniform float u_rock_exposure;",
                              "uniform float u_snow_coverage;"}) {
    EXPECT_NE(frag.find(uniform), std::string::npos);
  }

  EXPECT_NE(flat.find("float mud_weight = biome_forest * 0.70"), std::string::npos);
  EXPECT_NE(flat.find("float sand_weight = biome_forest * 0.12"), std::string::npos);
  EXPECT_NE(flat.find("float stone_weight = biome_forest * 0.18"), std::string::npos);
  EXPECT_NE(flat.find("float broken_transition ="), std::string::npos);
  EXPECT_NE(flat.find("max(core_alpha, max(broken_transition"), std::string::npos);
  EXPECT_NE(flat.find("mix(earth, map_ground"), std::string::npos);
  EXPECT_EQ(flat.find("if (shore_t > edge_limit)"), std::string::npos);
  EXPECT_EQ(flat.find("float ground_blend"), std::string::npos);
}

TEST(ShaderSource, TerrainGroundUsesCoherentBiomeMaterialPatches) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("float meadow_field = (u_has_noise_atlas == 1) ? baked_noise.a "
                      ": clamp("),
            std::string::npos);
  EXPECT_NE(flat.find("float thatch_field = (u_has_noise_atlas == 1) ? "
                      "baked_noise_detail.r : clamp("),
            std::string::npos);
  EXPECT_NE(flat.find("float lush_patch = smoothstep("), std::string::npos);
  EXPECT_NE(flat.find("float drainage_field ="), std::string::npos);
  EXPECT_NE(flat.find("float exposure_field ="), std::string::npos);
  EXPECT_NE(flat.find("float material_patch ="), std::string::npos);
  EXPECT_NE(flat.find("float deposited_soil ="), std::string::npos);
  EXPECT_NE(flat.find("uniform int u_ground_type;"), std::string::npos);
  EXPECT_NE(flat.find("uniform int u_terrain_type;"), std::string::npos);
  EXPECT_NE(flat.find("float mountain_surface = float(u_terrain_type == 2);"),
            std::string::npos);
  EXPECT_NE(flat.find("float mountain_face ="), std::string::npos);
  EXPECT_NE(flat.find("float snowline ="), std::string::npos);
  EXPECT_NE(flat.find("mountain_surface * altitude_snow"), std::string::npos);
  EXPECT_NE(flat.find("float soil_mix = bare_patch * 0.70;"), std::string::npos);
  EXPECT_NE(flat.find("float litter_patch = smoothstep("), std::string::npos);
  EXPECT_NE(flat.find("fertile_sward * biome_fertile"), std::string::npos);
  EXPECT_NE(
      flat.find(
          "uniform float u_tile_size, u_macro_noise_scale, u_detail_noise_scale;"),
      std::string::npos);
  EXPECT_NE(flat.find("float detail_scale = max(u_detail_noise_scale, 0.045);"),
            std::string::npos);
  EXPECT_NE(flat.find("u_soil_blend_height - soil_width"), std::string::npos);
  EXPECT_NE(flat.find("u_soil_roughness * 0.48"), std::string::npos);
  EXPECT_NE(flat.find("if (u_has_height_tex == 1)"), std::string::npos);
}

TEST(ShaderSource, TerrainReadsTheBakedNoiseAtlasWithAProceduralFallback) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  const auto bake = read_text(root / "assets" / "shaders" / "terrain_field_bake.frag");
  ASSERT_FALSE(frag.empty());
  ASSERT_FALSE(bake.empty());

  const auto flat = collapse_whitespace(frag);
  const auto flat_bake = collapse_whitespace(bake);

  for (const auto* uniform : {"uniform int u_has_noise_atlas;",
                              "uniform sampler2D u_noise_atlas;",
                              "uniform vec2 u_noise_atlas_world_size;"}) {
    EXPECT_NE(frag.find(uniform), std::string::npos);
  }

  EXPECT_NE(flat.find("#include \"terrain_noise.glsl\""), std::string::npos);
  EXPECT_NE(flat_bake.find("#include \"terrain_noise.glsl\""), std::string::npos);

  EXPECT_NE(flat.find("baked_noise = texture(u_noise_atlas, atlas_uv);"),
            std::string::npos);

  for (const auto* field : {"float regional_field = (u_has_noise_atlas == 1) ? "
                            "baked_noise.r : clamp(",
                            "float soil_field = (u_has_noise_atlas == 1) ? "
                            "baked_noise.g : clamp(",
                            "float moisture_field = (u_has_noise_atlas == 1) ? "
                            "baked_noise.b : clamp(",
                            "float meadow_field = (u_has_noise_atlas == 1) ? "
                            "baked_noise.a : clamp("}) {
    EXPECT_NE(flat.find(field), std::string::npos);
  }
}

TEST(ShaderSource, AlpineSnowPatchesRenderAcrossTerrain) {
  const auto root = find_repo_root();
  const auto plane = read_text(root / "assets" / "shaders" / "ground_plane.frag");
  const auto terrain = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(plane.empty());
  ASSERT_FALSE(terrain.empty());

  const auto flat_plane = collapse_whitespace(plane);
  const auto flat_terrain = collapse_whitespace(terrain);

  EXPECT_NE(flat_plane.find("if (u_ground_type == 3 && u_snow_coverage > 0.01)"),
            std::string::npos);
  EXPECT_NE(flat_plane.find("snow_accumulation * u_snow_coverage"), std::string::npos);

  EXPECT_NE(flat_terrain.find("if (u_ground_type == 3 && u_snow_coverage > 0.01)"),
            std::string::npos);
  EXPECT_NE(flat_terrain.find("alpine_snow_accumulation * u_snow_coverage"),
            std::string::npos);
  EXPECT_NE(flat_terrain.find("mountain_surface * altitude_snow"), std::string::npos);
}

TEST(ShaderSource, TerrainGroundShadesFromInterpolatedNormals) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("vec3 smooth_normal = normalize(v_normal);"), std::string::npos);
  EXPECT_NE(flat.find("mix(smooth_normal, facet_normal, facet_weight)"),
            std::string::npos);

  EXPECT_NE(flat.find("curvature_from_normal_field(smooth_normal)"), std::string::npos);

  EXPECT_NE(flat.find("float band_limit("), std::string::npos);
  EXPECT_NE(flat.find("relief_octave("), std::string::npos);
}

TEST(ShaderSource, TerrainGroundOccludesHollowsFromTheHeightField) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("float terrain_sky_openness(vec2 uv, float center_height)"),
            std::string::npos);
  EXPECT_NE(flat.find("const float k_openness_radii[3]"), std::string::npos);
  EXPECT_NE(flat.find("inversesqrt(highest_tangent * highest_tangent + 1.0)"),
            std::string::npos);

  EXPECT_NE(flat.find("float sheltered_ground = smoothstep(0.35, 0.92, sky_openness);"),
            std::string::npos);
  EXPECT_NE(flat.find("float terrain_cavity = 1.0 - sheltered_ground;"),
            std::string::npos);
  EXPECT_NE(flat.find("ambient_occlusion *= mix(1.0, sheltered_ground"),
            std::string::npos);

  EXPECT_NE(flat.find("terrain_cavity * 0.20 - slope * 0.24"), std::string::npos);
  EXPECT_NE(flat.find("terrain_cavity * 0.14"), std::string::npos);
  EXPECT_NE(flat.find("wind_deposit *= 0.92 + terrain_cavity"), std::string::npos);
}

TEST(ShaderSource, TerrainGroundCarriesDetailAtTheBattleCameraScale) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("const float k_mosaic_frequency = 0.24;"), std::string::npos);
  EXPECT_NE(flat.find("const float k_fleck_frequency = 1.15;"), std::string::npos);
  EXPECT_NE(flat.find("float mosaic_fade = band_limit(coord_footprint"),
            std::string::npos);
  EXPECT_NE(flat.find("float fleck_fade = band_limit(coord_footprint"),
            std::string::npos);
  EXPECT_NE(flat.find("mix(grass_color, u_grass_dry, mosaic_dry"), std::string::npos);
  EXPECT_NE(flat.find("mix(grass_color, deep_sward, mosaic_lush"), std::string::npos);
  EXPECT_NE(flat.find("soil_mix = max(soil_mix, ground_scuff"), std::string::npos);
}

TEST(ShaderSource, TerrainGroundErodesAlongTheFallLine) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "terrain_chunk.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("vec2 flow_down = normal.xz;"), std::string::npos);
  EXPECT_NE(flat.find("vec2 flow_across = vec2(-flow_down.y, flow_down.x);"),
            std::string::npos);
  EXPECT_NE(flat.find("const float k_rill_elongation"), std::string::npos);
  EXPECT_NE(flat.find("float rill_incision ="), std::string::npos);
  EXPECT_NE(flat.find("soil_mix = max(soil_mix, rill_incision"), std::string::npos);
  EXPECT_NE(flat.find("relief_gradient += rill_gradient * rill_strength"),
            std::string::npos);

  EXPECT_NE(flat.find("float micro_rise = dot(relief_gradient, sun_ground_dir)"),
            std::string::npos);
  EXPECT_NE(flat.find("float micro_shadow = smoothstep(sun_tangent"),
            std::string::npos);
  EXPECT_NE(flat.find("ndl * direct_occlusion * 0.76"), std::string::npos);
}

TEST(ShaderSource, NoCallerCompilesAShaderFileWithoutResolvingIncludes) {
  const auto root = find_repo_root();
  std::vector<std::string> offenders;

  for (const auto* subdir : {"app", "game", "render", "tools", "ui", "utils"}) {
    const auto directory = root / subdir;
    if (!std::filesystem::exists(directory)) {
      continue;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const auto extension = entry.path().extension().string();
      if (extension != ".cpp" && extension != ".h") {
        continue;
      }
      if (read_text(entry.path()).find("addShaderFromSourceFile") !=
          std::string::npos) {
        offenders.push_back(
            std::filesystem::relative(entry.path(), root).generic_string());
      }
    }
  }

  ASSERT_TRUE(offenders.empty()) << offenders.front();
}

TEST(ShaderSource, RoadsKeepPackedEarthSeparateFromPaving) {
  const auto root = find_repo_root();
  const auto frag = read_text(root / "assets" / "shaders" / "road.frag");
  ASSERT_FALSE(frag.empty());
  const auto flat = collapse_whitespace(frag);

  EXPECT_NE(flat.find("if (u_surface_kind == 2)"), std::string::npos);
  EXPECT_NE(flat.find("else if (u_surface_kind == 1)"), std::string::npos);
  EXPECT_NE(flat.find("vec2 aggregate_cells = worley_f("), std::string::npos);
  EXPECT_NE(flat.find("float rut_left ="), std::string::npos);
  EXPECT_NE(flat.find("u_alpha * edge_alpha"), std::string::npos);
}

TEST(ShaderSource, DirectionalShadowBlockMatchesTheUploadedStruct) {
  const auto root = find_repo_root();
  const auto glsl =
      read_text(root / "assets" / "shaders" / "include" / "directional_shadows.glsl");
  ASSERT_FALSE(glsl.empty());

  const int declared_cascades = glsl_int_constant(glsl, "SOI_MAX_SHADOW_CASCADES");
  EXPECT_EQ(declared_cascades, Render::GL::k_max_shadow_cascades);
  ASSERT_GT(declared_cascades, 0);

  const auto block = parse_std140_block(
      glsl,
      "DirectionalShadows",
      {{"SOI_MAX_SHADOW_CASCADES", static_cast<std::size_t>(declared_cascades)}});
  ASSERT_FALSE(block.member_names.empty()) << "DirectionalShadows block not found";

  EXPECT_EQ(block.float_count, Render::GL::DirectionalShadowBlock::k_float_count)
      << "the shader block and DirectionalShadowBlock no longer agree on size";

  const std::vector<std::string> expected_order{"u_shadow_light_vp",
                                                "u_shadow_split_distances",
                                                "u_shadow_params",
                                                "u_shadow_camera_position",
                                                "u_shadow_bias",
                                                "u_shadow_cascade_texel_world",
                                                "u_shadow_cascade_depth_span"};
  EXPECT_EQ(block.member_names, expected_order)
      << "DirectionalShadowBlock::packed_std140 writes these in declaration order";
}

TEST(ShaderSource, LocalLightingBlockMatchesTheUploadedStruct) {
  const auto root = find_repo_root();
  const auto glsl =
      read_text(root / "assets" / "shaders" / "include" / "local_lighting.glsl");
  ASSERT_FALSE(glsl.empty());

  const int declared_lights = glsl_int_constant(glsl, "SOI_MAX_LOCAL_LIGHTS");
  EXPECT_EQ(static_cast<std::size_t>(declared_lights), Render::k_max_local_lights);
  ASSERT_GT(declared_lights, 0);

  const auto block = parse_std140_block(
      glsl,
      "LocalLighting",
      {{"SOI_MAX_LOCAL_LIGHTS", static_cast<std::size_t>(declared_lights)}});
  ASSERT_FALSE(block.member_names.empty()) << "LocalLighting block not found";

  EXPECT_EQ(block.float_count, Render::LocalLightingBlock::k_float_count);

  const std::vector<std::string> expected_order{
      "u_local_position_radius", "u_local_color_intensity", "u_local_light_meta"};
  EXPECT_EQ(block.member_names, expected_order)
      << "pack_local_lights_std140 writes these in declaration order";
}
