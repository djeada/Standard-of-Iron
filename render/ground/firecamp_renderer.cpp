#include "firecamp_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/gl/primitives.h"
#include "render/scene_renderer.h"
#include "render/terrain_contact.h"
#include "scatter_runtime.h"
#include "scatter_submission.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                 const Game::Map::BiomeSettings& biome_settings,
                                 const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);

  m_state.track_visible_instances = true;

  auto& firecamp_params = m_state.params;
  firecamp_params.time = 0.0F;
  firecamp_params.flicker_speed = 4.4F;
  firecamp_params.flicker_amount = 0.028F;
  firecamp_params.glow_strength = 1.16F;

  generate_firecamp_instances();
}

void FireCampRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const FireCampInstanceGpu& instance) -> const QVector4D& {
        return instance.pos_intensity;
      },
      renderer.static_world_visibility_filter_enabled()
          ? renderer.submission_visibility().snapshot()
          : nullptr);
  if (visible_count == 0) {
    return;
  }

  FireCampBatchParams params = m_state.params;
  params.time = renderer.get_animation_time();
  params.flicker_amount =
      m_state.params.flicker_amount * (0.9F + 0.25F * std::sin(params.time * 1.3F));
  params.glow_strength = m_state.params.glow_strength *
                         (0.85F + 0.2F * std::sin(params.time * 1.7F + 1.2F));
  TerrainScatterCmd cmd;
  cmd.visibility = renderer.visibility_mask();
  cmd.species = TerrainScatterCmd::Species::FireCamp;
  cmd.firecamp = params;
  Scatter::submit_visible_chunks(renderer, m_state, cmd);

  Mesh* const stone_mesh = get_unit_sphere();
  constexpr float k_daylight_firelight = 0.35F;
  const float night = environment_night_amount(renderer.environment_lighting());
  const float daylight_scale =
      k_daylight_firelight + (1.0F - k_daylight_firelight) * night;
  for (const auto& instance : m_state.visible_instances) {
    const QVector4D pos_intensity = instance.pos_intensity;
    const QVector4D radius_phase = instance.radius_phase;

    const QVector3D camp_pos = pos_intensity.toVector3D();

    if (!renderer.submission_visibility().accepts_sphere(
            camp_pos, 2.0F, SubmissionFogMode::Revealed)) {
      continue;
    }
    const float intensity = std::clamp(pos_intensity.w(), 0.6F, 1.6F);
    const float base_radius = std::max(radius_phase.x(), 1.0F);

    {
      const float flicker =
          0.92F + 0.08F * std::sin((params.time * 2.3F) + (radius_phase.y() * 6.28F));
      const FireLightShape shape = fire_light_shape(base_radius);
      Render::LocalLight firelight;
      firelight.position = camp_pos + QVector3D(0.0F, shape.height_above_ground, 0.0F);
      firelight.color = QVector3D(1.0F, 0.52F, 0.19F);
      firelight.radius = shape.reach;
      firelight.intensity = intensity * flicker * daylight_scale;
      renderer.local_light(firelight);
    }

    const auto decor_index = static_cast<std::size_t>(radius_phase.w());
    if (decor_index >= m_camp_decor.size()) {
      continue;
    }
    const CampDecor& decor = m_camp_decor[decor_index];

    const QVector3D ember_color(0.62F, 0.13F, 0.03F);
    const float ember_pulse =
        0.55F + 0.45F * std::sin(params.time * 3.1F + decor.phase * 2.1F);

    for (const auto& stone : decor.stones) {
      renderer.mesh(stone_mesh, stone.model, stone.color);
    }
    for (const auto& piece : decor.cylinders) {
      const float weight = piece.ember_weight * ember_pulse;
      const QVector3D color = piece.base_color * (1.0F - weight) + ember_color * weight;
      renderer.cylinder(piece.start, piece.end, piece.radius, color, 1.0F);
    }
  }
}

auto FireCampRenderer::fire_light_shape(float camp_radius) -> FireLightShape {

  constexpr float k_flame_height = 0.55F;
  constexpr float k_reach_per_camp_radius = 1.8F;
  constexpr float k_min_reach = 3.0F;
  constexpr float k_max_reach = 6.0F;

  const float radius = std::max(camp_radius, 1.0F);
  return {std::min(radius * 0.55F, k_flame_height),
          std::clamp(radius * k_reach_per_camp_radius, k_min_reach, k_max_reach)};
}

auto FireCampRenderer::hearth_radius(float authored_scale) -> float {
  constexpr float k_hearth_radius = 0.5F;
  return k_hearth_radius * std::clamp(authored_scale, 0.6F, 1.6F);
}

namespace {
constexpr float k_max_stone_tilt_degrees = 30.0F;
constexpr float k_degrees_per_radian = 57.2957795F;
} // namespace

auto FireCampRenderer::build_camp_decor(const Game::Map::TerrainService& terrain,
                                        float world_x,
                                        float world_z,
                                        float hearth_radius,
                                        float phase) -> CampDecor {
  CampDecor decor;
  decor.phase = phase;

  auto ground = [&terrain](float x, float z) {
    return terrain.resolve_surface_world_y(x, z);
  };
  auto grounded = [&ground](float x, float z, float lift) {
    return QVector3D(x, ground(x, z) + lift, z);
  };

  uint32_t state = hash_coords(
      static_cast<int>(std::floor(world_x)),
      static_cast<int>(std::floor(world_z)),
      static_cast<uint32_t>(phase * HashConstants::k_temporal_variation_frequency));

  const float base_yaw = rand_01(state) * MathConstants::k_two_pi;
  const float centre_y = ground(world_x, world_z);

  const QVector3D bark_color(0.22F, 0.16F, 0.11F);
  const QVector3D char_color(0.05F, 0.035F, 0.028F);
  const float log_radius = hearth_radius * 0.085F;
  constexpr int k_tepee_logs = 5;
  for (int log = 0; log < k_tepee_logs; ++log) {
    const float angle = base_yaw +
                        MathConstants::k_two_pi * static_cast<float>(log) /
                            static_cast<float>(k_tepee_logs) +
                        remap(rand_01(state), -0.22F, 0.22F);
    const float cos_a = std::cos(angle);
    const float sin_a = std::sin(angle);
    const float reach = hearth_radius * remap(rand_01(state), 0.62F, 0.78F);
    const QVector3D outer =
        grounded(world_x + cos_a * reach, world_z + sin_a * reach, log_radius * 0.85F);
    const QVector3D inner(world_x + cos_a * hearth_radius * 0.06F,
                          centre_y + hearth_radius * remap(rand_01(state), 0.36F, 0.5F),
                          world_z + sin_a * hearth_radius * 0.06F);
    const QVector3D burn_line = outer + (inner - outer) * 0.5F;
    const float bark_tone = remap(rand_01(state), 0.8F, 1.1F);
    decor.cylinders.push_back({.start = outer,
                               .end = burn_line,
                               .base_color = bark_color * bark_tone,
                               .radius = log_radius,
                               .ember_weight = 0.0F,
                               .kind = DecorKind::Log});
    decor.cylinders.push_back({.start = burn_line,
                               .end = inner,
                               .base_color = char_color,
                               .radius = log_radius * 0.9F,
                               .ember_weight = 0.12F,
                               .kind = DecorKind::Log});
  }

  constexpr int k_stone_count = 10;
  const QVector3D stone_color(0.30F, 0.285F, 0.265F);
  const QVector3D soot_color(0.11F, 0.10F, 0.09F);
  for (int stone = 0; stone < k_stone_count; ++stone) {
    const float angle = base_yaw + MathConstants::k_two_pi * static_cast<float>(stone) /
                                       static_cast<float>(k_stone_count);
    const float ring = hearth_radius * remap(rand_01(state), 0.94F, 1.04F);
    const float centre_x = world_x + std::cos(angle) * ring;
    const float centre_z = world_z + std::sin(angle) * ring;
    const float size = hearth_radius * remap(rand_01(state), 0.17F, 0.23F);
    const QVector3D half_extents(size * remap(rand_01(state), 1.05F, 1.3F),
                                 size * remap(rand_01(state), 0.7F, 0.9F),
                                 size * remap(rand_01(state), 0.8F, 0.95F));
    const float tone = remap(rand_01(state), 0.75F, 1.1F);
    const float soot = remap(rand_01(state), 0.0F, 0.5F);
    const QVector3D centre = grounded(centre_x, centre_z, half_extents.y() * 0.3F);

    DecorStone piece;
    piece.centre = centre;
    piece.half_height = half_extents.y();
    piece.color = (stone_color * (1.0F - soot) + soot_color * soot) * tone;
    piece.model.translate(centre);
    piece.model.rotate(Render::ground_tilt_rotation(
        terrain.sample_ground_normal(centre_x, centre_z), k_max_stone_tilt_degrees));
    piece.model.rotate(-angle * k_degrees_per_radian + 90.0F, 0.0F, 1.0F, 0.0F);
    piece.model.rotate(remap(rand_01(state), -14.0F, 14.0F), 1.0F, 0.0F, 0.0F);
    piece.model.scale(half_extents);
    decor.stones.push_back(piece);
  }

  const float pile_angle =
      base_yaw + MathConstants::k_pi * remap(rand_01(state), 0.8F, 1.2F);
  const float pile_x = world_x + std::cos(pile_angle) * hearth_radius * 1.5F;
  const float pile_z = world_z + std::sin(pile_angle) * hearth_radius * 1.5F;
  const float along_x = -std::sin(pile_angle);
  const float along_z = std::cos(pile_angle);
  const float across_x = std::cos(pile_angle);
  const float across_z = std::sin(pile_angle);
  const float wood_radius = hearth_radius * 0.075F;
  const float wood_half = hearth_radius * 0.5F;
  const std::array<std::pair<float, float>, 3> pile{
      {{-1.05F, 1.0F}, {1.05F, 1.0F}, {0.0F, 2.75F}}};
  for (const auto& [offset, lift] : pile) {
    const float x = pile_x + across_x * offset * wood_radius;
    const float z = pile_z + across_z * offset * wood_radius;
    const float skew = remap(rand_01(state), -0.12F, 0.12F) * wood_half;
    decor.cylinders.push_back(
        {.start = grounded(x - along_x * (wood_half + skew),
                           z - along_z * (wood_half + skew),
                           wood_radius * lift),
         .end = grounded(x + along_x * (wood_half - skew),
                         z + along_z * (wood_half - skew),
                         wood_radius * lift),
         .base_color = bark_color * remap(rand_01(state), 0.95F, 1.25F),
         .radius = wood_radius,
         .ember_weight = 0.0F,
         .kind = DecorKind::Firewood});
  }

  return decor;
}

void FireCampRenderer::clear() {
  m_state.reset_instances();
  m_camp_decor.clear();
}

void FireCampRenderer::generate_firecamp_instances() {
  auto& firecamp_instances = m_state.instances;
  auto& firecamp_instance_count = m_state.instance_count;
  auto& firecamp_instances_dirty = m_state.instances_dirty;

  firecamp_instances.clear();
  m_camp_decor.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }
  const auto& terrain_service = world().terrain_or_empty();

  for (size_t i = 0; i < m_world_props.size(); ++i) {
    const auto& prop = m_world_props[i];
    if (prop.type != Game::Map::WorldProp::Type::FireCamp) {
      continue;
    }

    const auto [world_x, world_z] = terrain_service.world_prop_world_xz(prop);
    const float hearth = hearth_radius(prop.scale);
    const float centre_y = terrain_service.resolve_surface_world_y(world_x, world_z);

    constexpr int k_spokes = 8;
    float slope_x = 0.0F;
    float slope_z = 0.0F;
    std::array<float, k_spokes> rim_heights{};
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      const float angle = MathConstants::k_two_pi * static_cast<float>(spoke) /
                          static_cast<float>(k_spokes);
      const float rise =
          terrain_service.resolve_surface_world_y(world_x + std::cos(angle) * hearth,
                                                  world_z + std::sin(angle) * hearth) -
          centre_y;
      rim_heights[static_cast<std::size_t>(spoke)] = rise;
      slope_x += rise * std::cos(angle);
      slope_z += rise * std::sin(angle);
    }
    slope_x /= hearth * static_cast<float>(k_spokes) * 0.5F;
    slope_z /= hearth * static_cast<float>(k_spokes) * 0.5F;
    float bulge = 0.0F;
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      const float angle = MathConstants::k_two_pi * static_cast<float>(spoke) /
                          static_cast<float>(k_spokes);
      const float plane =
          (slope_x * std::cos(angle) + slope_z * std::sin(angle)) * hearth;
      bulge = std::max(bulge, rim_heights[static_cast<std::size_t>(spoke)] - plane);
    }

    const float base_radius = std::max(prop.radius, 1.0F);
    const float phase = static_cast<float>(i) * 1.234567F;

    const auto decor_index = static_cast<float>(m_camp_decor.size());
    m_camp_decor.push_back(
        build_camp_decor(terrain_service, world_x, world_z, hearth, phase));

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(world_x, centre_y, world_z, prop.intensity);
    instance.radius_phase = QVector4D(base_radius, phase, prop.scale, decor_index);
    instance.ground = QVector4D(slope_x, slope_z, hearth, std::min(bulge, 0.3F));
    firecamp_instances.push_back(instance);
  }

  firecamp_instance_count = firecamp_instances.size();
  firecamp_instances_dirty = firecamp_instance_count > 0;
}

} // namespace Render::GL
