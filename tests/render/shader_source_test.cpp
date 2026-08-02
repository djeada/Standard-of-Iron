#include <cctype>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <locale>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

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
  const auto single = read_text(root / "assets" / "shaders" / "character_skinned.frag");
  const auto instanced =
      read_text(root / "assets" / "shaders" / "character_skinned_instanced.frag");
  ASSERT_FALSE(single.empty());
  ASSERT_FALSE(instanced.empty());

  for (const auto* source : {&single, &instanced}) {
    EXPECT_NE(source->find("#include \"environment_lighting.glsl\""),
              std::string::npos);
    EXPECT_NE(source->find("uniform vec3 u_camera_position;"), std::string::npos);
    EXPECT_NE(source->find("shade_readable_character"), std::string::npos);
    EXPECT_NE(source->find("environment_primary_direction()"), std::string::npos);
    EXPECT_NE(source->find("environment_ambient_intensity()"), std::string::npos);
    EXPECT_NE(source->find("float readable_ambient = max(scene_ambient, 0.18);"),
              std::string::npos);
    EXPECT_NE(source->find("float rim = pow("), std::string::npos);
    EXPECT_NE(source->find("if (material_id == 2)"), std::string::npos);
    EXPECT_EQ(source->find("normalize(vec3(0.65, 0.50, 0.40))"), std::string::npos);
  }
}

TEST(ShaderSource, EveryWorldLightingVariantUsesSharedEnvironmentBlock) {
  const auto root = find_repo_root();
  const std::vector<std::string> shaders{
      "basic.frag",
      "basic_instanced.frag",
      "character_skinned.frag",
      "character_skinned_instanced.frag",
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

TEST(ShaderSource, GeneralWorldShadersDoNotHardCodeDaylightColors) {
  const auto root = find_repo_root();
  for (const auto* name : {"basic.frag",
                           "basic_instanced.frag",
                           "character_skinned.frag",
                           "character_skinned_instanced.frag",
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
                           "character_skinned_instanced.frag",
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
  EXPECT_NE(shadow.find("for (int y = -3; y <= 3; ++y)"), std::string::npos);
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

  EXPECT_NE(flat.find("float meadow_field = clamp("), std::string::npos);
  EXPECT_NE(flat.find("float thatch_field = clamp("), std::string::npos);
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
