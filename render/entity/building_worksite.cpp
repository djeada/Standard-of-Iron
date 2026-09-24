#include "building_worksite.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "building_render_common.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/units/spawn_type.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

constexpr QVector3D k_scaffold_timber{0.47F, 0.36F, 0.24F};
constexpr QVector3D k_scaffold_plank{0.55F, 0.44F, 0.31F};
constexpr QVector3D k_curb_stone{0.62F, 0.59F, 0.53F};
constexpr QVector3D k_salvage_timber{0.50F, 0.38F, 0.25F};
constexpr QVector3D k_salvage_stone{0.66F, 0.63F, 0.57F};
constexpr QVector3D k_work_dust{0.66F, 0.61F, 0.52F};

constexpr float k_scaffold_offset = 0.32F;
constexpr float k_scaffold_bay = 1.6F;
constexpr float k_scaffold_lift = 0.95F;
constexpr float k_pole_radius = 0.045F;
constexpr float k_ledger_radius = 0.03F;
constexpr float k_transition_puff_seconds = 1.1F;

auto hash01(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  std::uint32_t h = seed * 747796405U + salt * 2891336453U + 0x9E3779B9U;
  h ^= h >> 16U;
  h *= 0x7FEB352DU;
  h ^= h >> 15U;
  h *= 0x846CA68BU;
  h ^= h >> 16U;
  return static_cast<float>(h & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

auto smooth(float edge0, float edge1, float x) noexcept -> float {
  float const t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

// A point on the footprint in world space: `x` and `z` are metres from the
// centre along the building's own axes.
auto site_point(const BuildingWorksite& site,
                float x,
                float z,
                float y = 0.0F) -> QVector3D {
  float const yaw = site.yaw_degrees * (k_pi / 180.0F);
  float const c = std::cos(yaw);
  float const s = std::sin(yaw);
  return {site.base.x() + (x * c) + (z * s),
          site.base.y() + y,
          site.base.z() - (x * s) + (z * c)};
}

auto site_box(const BuildingWorksite& site,
              const QVector3D& centre_local,
              const QVector3D& half_extent,
              float extra_yaw_degrees = 0.0F) -> QMatrix4x4 {
  QMatrix4x4 model;
  model.translate(
      site_point(site, centre_local.x(), centre_local.z(), centre_local.y()));
  model.rotate(site.yaw_degrees + extra_yaw_degrees, 0.0F, 1.0F, 0.0F);
  model.scale(half_extent);
  return model;
}

// Posts along one side of the footprint, evenly spaced, corners included.
auto side_posts(float half_length) -> int {
  return std::max(
      2, static_cast<int>(std::ceil((2.0F * half_length) / k_scaffold_bay)) + 1);
}

} // namespace

auto building_worksite_for(const Engine::Core::World& world,
                           Engine::Core::EntityID entity_id) -> BuildingWorksite {
  BuildingWorksite site;
  if (auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id)) {
    site.base =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    site.yaw_degrees = transform->rotation.y;
  }
  if (auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id)) {
    site.footprint = building_collapse_footprint(unit->spawn_type);
  } else if (auto const* preview =
                 world.try_get<Engine::Core::ConstructionPreviewComponent>(entity_id)) {
    if (auto const type = Game::Units::spawn_typeFromString(preview->product_type)) {
      site.footprint = building_collapse_footprint(*type);
    }
  } else if (auto const* wall =
                 world.try_get<Engine::Core::WallConstructionSiteComponent>(
                     entity_id)) {
    site.footprint = building_collapse_footprint(wall->product_type);
  }
  site.seed = static_cast<std::uint32_t>(entity_id) * 2654435761U;
  return site;
}

auto construction_built_fraction(float progress) noexcept -> float {
  // The foundation course is laid first; the walls rise through the middle of
  // the job; the last tenth is roofing and finishing at full height.
  return std::max(0.04F, smooth(0.08F, 0.90F, progress));
}

auto construction_scaffold_fraction(float progress) noexcept -> float {
  return smooth(0.06F, 0.28F, progress) * (1.0F - smooth(0.86F, 0.99F, progress));
}

auto dismantle_standing_fraction(float progress) noexcept -> float {
  // Nearly flat by the last stroke, so leaving the world is not a pop.
  return 1.0F - (0.96F * smooth(0.0F, 1.0F, progress));
}

auto building_standing_model(const QMatrix4x4& model,
                             const QVector3D& base,
                             float standing) -> QMatrix4x4 {
  if (standing >= 0.999F) {
    return model;
  }
  QMatrix4x4 press;
  press.translate(base);
  press.scale(1.0F, std::clamp(standing, 0.01F, 1.0F), 1.0F);
  press.translate(-base);
  return press * model;
}

void submit_foundation_curb(ISubmitter& out, const BuildingWorksite& site) {
  Mesh* cube = get_unit_cube();
  if (cube == nullptr) {
    return;
  }
  float const hw = site.footprint.half_width;
  float const hd = site.footprint.half_depth;
  constexpr float k_half_height = 0.07F;
  constexpr float k_half_width = 0.14F;
  out.mesh(cube,
           site_box(site,
                    {0.0F, k_half_height, hd},
                    {hw + k_half_width, k_half_height, k_half_width}),
           k_curb_stone,
           nullptr,
           1.0F);
  out.mesh(cube,
           site_box(site,
                    {0.0F, k_half_height, -hd},
                    {hw + k_half_width, k_half_height, k_half_width}),
           k_curb_stone * 0.94F,
           nullptr,
           1.0F);
  out.mesh(cube,
           site_box(site,
                    {hw, k_half_height, 0.0F},
                    {k_half_width, k_half_height, hd - k_half_width}),
           k_curb_stone * 0.97F,
           nullptr,
           1.0F);
  out.mesh(cube,
           site_box(site,
                    {-hw, k_half_height, 0.0F},
                    {k_half_width, k_half_height, hd - k_half_width}),
           k_curb_stone * 0.91F,
           nullptr,
           1.0F);
}

void submit_scaffolding(ISubmitter& out, const BuildingWorksite& site, float raised) {
  if (raised <= 0.01F) {
    return;
  }
  Mesh* cube = get_unit_cube();
  float const top = site.footprint.height * 1.04F * std::clamp(raised, 0.0F, 1.0F);
  float const hw = site.footprint.half_width + k_scaffold_offset;
  float const hd = site.footprint.half_depth + k_scaffold_offset;

  struct Side {
    float ax, az, bx, bz;
    float out_x, out_z;
  };
  const std::array<Side, 4> sides{{
      {-hw, hd, hw, hd, 0.0F, 1.0F},
      {hw, -hd, -hw, -hd, 0.0F, -1.0F},
      {hw, hd, hw, -hd, 1.0F, 0.0F},
      {-hw, -hd, -hw, hd, -1.0F, 0.0F},
  }};

  for (int side_index = 0; side_index < 4; ++side_index) {
    Side const& side = sides[side_index];
    float const half_length = 0.5F * std::hypot(side.bx - side.ax, side.bz - side.az);
    int const posts = side_posts(half_length);
    for (int i = 0; i < posts; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(posts - 1);
      float const x = side.ax + ((side.bx - side.ax) * t);
      float const z = side.az + ((side.bz - side.az) * t);
      out.cylinder(site_point(site, x, z, 0.0F),
                   site_point(site, x, z, top),
                   k_pole_radius,
                   k_scaffold_timber,
                   1.0F);
    }

    for (float lift = k_scaffold_lift; lift <= top + 0.001F; lift += k_scaffold_lift) {
      for (int i = 0; i + 1 < posts; ++i) {
        float const t0 = static_cast<float>(i) / static_cast<float>(posts - 1);
        float const t1 = static_cast<float>(i + 1) / static_cast<float>(posts - 1);
        QVector3D const a = site_point(site,
                                       side.ax + ((side.bx - side.ax) * t0),
                                       side.az + ((side.bz - side.az) * t0),
                                       lift);
        QVector3D const b = site_point(site,
                                       side.ax + ((side.bx - side.ax) * t1),
                                       side.az + ((side.bz - side.az) * t1),
                                       lift);
        out.cylinder(a, b, k_ledger_radius, k_scaffold_timber * 0.92F, 1.0F);

        // One brace per bay, alternating, below the working level.
        if (((i + side_index) % 2) == 0 && lift - k_scaffold_lift >= 0.0F) {
          QVector3D const low = site_point(site,
                                           side.ax + ((side.bx - side.ax) * t0),
                                           side.az + ((side.bz - side.az) * t0),
                                           lift - k_scaffold_lift);
          out.cylinder(low, b, k_ledger_radius * 0.8F, k_scaffold_timber * 0.85F, 1.0F);
        }
      }

      // A plank walkway along the side at every second lift.
      if (cube != nullptr &&
          static_cast<int>(std::lround(lift / k_scaffold_lift)) % 2 == 1) {
        float const mid_x = 0.5F * (side.ax + side.bx) - (side.out_x * 0.18F);
        float const mid_z = 0.5F * (side.az + side.bz) - (side.out_z * 0.18F);
        bool const along_x = std::abs(side.bx - side.ax) > std::abs(side.bz - side.az);
        QVector3D const half = along_x ? QVector3D(half_length, 0.025F, 0.2F)
                                       : QVector3D(0.2F, 0.025F, half_length);
        out.mesh(cube,
                 site_box(site, {mid_x, lift + 0.03F, mid_z}, half),
                 k_scaffold_plank,
                 nullptr,
                 1.0F);
      }
    }
  }
}

void submit_salvage_stacks(ISubmitter& out,
                           const BuildingWorksite& site,
                           float progress) {
  Mesh* cube = get_unit_cube();
  if (cube == nullptr || progress <= 0.02F) {
    return;
  }
  // Stacks sit off the building's +x side, clear of the footprint, and gain a
  // course for every quarter of the work.
  int const courses = std::clamp(static_cast<int>(std::ceil(progress * 4.0F)), 1, 4);
  float const x = site.footprint.half_width + 1.0F;
  float const spread = std::min(site.footprint.half_depth * 0.55F, 1.4F);

  // Timber: beams laid in alternate directions, like a log crib.
  for (int course = 0; course < courses; ++course) {
    bool const crosswise = (course % 2) == 1;
    for (int beam = 0; beam < 3; ++beam) {
      float const offset = (static_cast<float>(beam) - 1.0F) * 0.24F;
      float const y = 0.09F + (static_cast<float>(course) * 0.17F);
      QVector3D const centre = crosswise ? QVector3D(x + offset, y, -spread)
                                         : QVector3D(x, y, -spread + offset);
      QVector3D const half =
          crosswise ? QVector3D(0.08F, 0.08F, 0.46F) : QVector3D(0.46F, 0.08F, 0.08F);
      float const shade = 0.88F + (hash01(site.seed, 200U + course * 3U + beam) * 0.2F);
      out.mesh(
          cube, site_box(site, centre, half), k_salvage_timber * shade, nullptr, 1.0F);
    }
  }

  // Dressed stone: a squared block pile, one layer smaller than the last.
  for (int course = 0; course < courses; ++course) {
    int const per_side = std::max(1, 3 - course);
    float const y = 0.13F + (static_cast<float>(course) * 0.26F);
    for (int i = 0; i < per_side; ++i) {
      for (int j = 0; j < per_side; ++j) {
        float const step = 0.28F;
        float const origin = -0.5F * step * static_cast<float>(per_side - 1);
        QVector3D const centre(x + origin + (static_cast<float>(i) * step),
                               y,
                               spread + origin + (static_cast<float>(j) * step));
        float const shade =
            0.86F + (hash01(site.seed, 300U + course * 16U + i * 4U + j) * 0.2F);
        out.mesh(cube,
                 site_box(site, centre, {0.13F, 0.12F, 0.13F}),
                 k_salvage_stone * shade,
                 nullptr,
                 1.0F);
      }
    }
  }
}

void submit_worksite_dust(ISubmitter& out,
                          const BuildingWorksite& site,
                          float strength,
                          float animation_time) {
  if (strength <= 0.01F) {
    return;
  }
  float const radius = std::sqrt(site.footprint.half_width * site.footprint.half_depth);
  float const angle =
      hash01(site.seed, static_cast<std::uint32_t>(animation_time)) * 2.0F * k_pi;
  QVector3D const at = site_point(site,
                                  std::cos(angle) * site.footprint.half_width * 0.8F,
                                  std::sin(angle) * site.footprint.half_depth * 0.8F);
  out.combat_dust(at, k_work_dust, radius * 0.55F, 0.75F * strength, animation_time);
}

void submit_structure_work_dressing(ISubmitter& out,
                                    const Engine::Core::World& world,
                                    Engine::Core::EntityID entity_id,
                                    float animation_time) {
  if (!world.has<Engine::Core::BuildingComponent>(entity_id)) {
    return;
  }
  auto const* repair =
      world.try_get<Engine::Core::StructureRepairPresentationComponent>(entity_id);
  auto const* dismantle =
      world.try_get<Engine::Core::DismantleSiteComponent>(entity_id);
  float const transition = building_state_transition_age(
      static_cast<std::uint32_t>(entity_id), animation_time);
  if (repair == nullptr && dismantle == nullptr &&
      (transition < 0.0F || transition > k_transition_puff_seconds)) {
    return;
  }

  BuildingWorksite const site = building_worksite_for(world, entity_id);

  if (repair != nullptr && dismantle == nullptr) {
    submit_scaffolding(out, site, smooth(0.0F, 1.0F, repair->scaffold));
    float const knock = 1.0F - smooth(0.0F, 0.7F, repair->since_restore);
    submit_worksite_dust(out, site, knock, animation_time);
  }

  if (dismantle != nullptr) {
    submit_salvage_stacks(out, site, dismantle->progress);
    if (dismantle->active_workers > 0) {
      submit_worksite_dust(out, site, 0.45F, animation_time);
    }
  }

  // A swap between damage states happens under a burst of dust and grit, so
  // the new mesh arrives the way masonry actually gives way.
  if (transition >= 0.0F && transition <= k_transition_puff_seconds) {
    float const radius =
        std::sqrt(site.footprint.half_width * site.footprint.half_depth);
    float const fade = 1.0F - (transition / k_transition_puff_seconds);
    out.combat_dust(site.base, k_work_dust, radius * 1.2F, 1.3F * fade, animation_time);
    out.stone_impact(site_point(site, 0.0F, 0.0F, site.footprint.height * 0.45F),
                     k_work_dust,
                     radius * 0.5F,
                     1.4F,
                     transition);
  }
}

auto structure_work_model(const QMatrix4x4& model,
                          const Engine::Core::World& world,
                          Engine::Core::EntityID entity_id) -> QMatrix4x4 {
  auto const* dismantle =
      world.try_get<Engine::Core::DismantleSiteComponent>(entity_id);
  if (dismantle == nullptr || !world.has<Engine::Core::BuildingComponent>(entity_id)) {
    return model;
  }
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  if (transform == nullptr) {
    return model;
  }
  return building_standing_model(
      model,
      QVector3D(transform->position.x, transform->position.y, transform->position.z),
      dismantle_standing_fraction(dismantle->progress));
}

} // namespace Render::GL
