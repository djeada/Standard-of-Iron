#include "barracks_activity.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "animation/clip_manifest.h"
#include "building_archetype_desc.h"
#include "building_render_common.h"
#include "building_state.h"
#include "civilian_actor.h"
#include "game/core/component.h"
#include "home_activity.h"
#include "registry.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/local_lighting.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr float k_pi = 3.14159265F;
constexpr float k_far_metres = 130.0F;
constexpr float k_min_pixels = 26.0F;
constexpr float k_idle_cycle_seconds = 8.0F;
constexpr float k_blend_seconds = 0.20F;

constexpr int k_rows = 3;
constexpr int k_columns = 2;
constexpr int k_recruits = k_rows * k_columns;
constexpr float k_row_spacing = 0.62F;
constexpr float k_column_spacing = 0.66F;
constexpr int k_clumsy_recruit = 3;
constexpr float k_parade_floor = 0.077F;

const QVector3D k_forge_offset{-0.34F, 0.0F, 0.02F};
const QVector3D k_anvil_offset{0.16F, 0.0F, -0.10F};
const QVector3D k_smith_offset{0.16F, 0.0F, -0.50F};
const QVector3D k_barrel_offset{0.52F, 0.0F, 0.12F};
constexpr float k_forge_top = 0.36F;
constexpr float k_anvil_top = 0.37F;

auto roll(std::uint32_t seed, std::uint32_t salt) -> float {
  return static_cast<float>(ambient_hash(seed ^ (salt * 0x9e3779b9U)) & 0xffffU) /
         65536.0F;
}

auto fract(float value) -> float {
  return value - std::floor(value);
}

auto smooth(float t) -> float {
  const float c = std::clamp(t, 0.0F, 1.0F);
  return c * c * (3.0F - 2.0F * c);
}

auto yaw_of(const QVector3D& direction) -> float {
  if ((direction.x() * direction.x()) + (direction.z() * direction.z()) < 1e-8F) {
    return 0.0F;
  }
  return std::atan2(direction.x(), direction.z()) * 180.0F / k_pi;
}

struct Beat {
  std::uint16_t clip;
  float seconds;
  bool looping;
};

constexpr std::array<Beat, 8> k_drill{{
    {Animation::k_humanoid_combat_ready_clip, 1.10F, true},
    {Animation::k_humanoid_attack_sword_a_clip, 1.00F, false},
    {Animation::k_humanoid_combat_ready_clip, 0.45F, true},
    {Animation::k_humanoid_attack_sword_b_clip, 1.00F, false},
    {Animation::k_humanoid_combat_ready_clip, 0.45F, true},
    {Animation::k_humanoid_attack_sword_c_clip, 1.10F, false},
    {Animation::k_humanoid_react_block_clip, 0.90F, false},
    {Animation::k_humanoid_combat_ready_clip, 0.80F, true},
}};

auto drill_seconds() -> float {
  float total = 0.0F;
  for (const Beat& beat : k_drill) {
    total += beat.seconds;
  }
  return total;
}

struct Stance {
  std::uint16_t clip{Animation::k_humanoid_idle_clip};
  float phase{0.0F};
  std::uint16_t blend_clip{0xFFFFU};
  float blend_phase{0.0F};
  float blend_weight{0.0F};
  int beat{0};
};

auto beat_phase(const Beat& beat, float into) -> float {
  if (beat.looping) {
    return fract(into / 2.4F);
  }
  return std::clamp(into / beat.seconds, 0.0F, 0.999F);
}

auto drill_stance(float time, bool clumsy) -> Stance {
  const float total = drill_seconds();
  float t = std::fmod(time, total);
  if (t < 0.0F) {
    t += total;
  }
  const auto cycle = static_cast<int>(std::floor(time / total));
  Stance stance;
  for (std::size_t i = 0; i < k_drill.size(); ++i) {
    Beat beat = k_drill[i];
    if (t >= beat.seconds) {
      t -= beat.seconds;
      continue;
    }
    if (clumsy && beat.clip == Animation::k_humanoid_react_block_clip &&
        (cycle % 2) == 0) {
      beat.clip = Animation::k_humanoid_react_stagger_clip;
    }
    stance.clip = beat.clip;
    stance.phase = beat_phase(beat, t);
    stance.beat = static_cast<int>(i);
    if (t < k_blend_seconds) {
      const Beat& before = k_drill[(i + k_drill.size() - 1) % k_drill.size()];
      stance.blend_clip = before.clip;
      stance.blend_phase = before.looping ? fract(before.seconds / 2.4F) : 0.999F;
      stance.blend_weight = 1.0F - smooth(t / k_blend_seconds);
    }
    return stance;
  }
  return stance;
}

auto at_ease_stance(float time, std::uint32_t seed) -> Stance {
  Stance stance;
  const float kind = roll(seed, 41U);
  if (kind < 0.30F) {
    stance.clip = Animation::k_humanoid_idle_squat_clip;
    stance.phase = fract(time / 3.0F + roll(seed, 42U));
  } else if (kind < 0.55F) {
    stance.clip = Animation::k_humanoid_idle_weave_clip;
    stance.phase = fract(time / 2.8F + roll(seed, 43U));
  } else {
    const float cycle = fract(time / 9.0F + roll(seed, 44U)) * 9.0F;
    if (cycle > 6.9F) {
      stance.clip = Animation::k_humanoid_taunt_dismissive_clip;
      stance.phase = std::clamp((cycle - 6.9F) / 2.1F, 0.0F, 0.999F);
    } else {
      stance.clip = Animation::k_humanoid_idle_clip;
      stance.phase = fract(cycle / k_idle_cycle_seconds + roll(seed, 45U));
    }
  }
  return stance;
}

void add_actor(const DrawContext& ctx,
               const NationCivilianRig& rig,
               Render::Creature::ArchetypeId archetype,
               const QVector3D& world_position,
               float yaw,
               const Stance& stance,
               std::uint32_t owner,
               std::uint16_t instance,
               std::uint32_t seed,
               bool distant) {
  QMatrix4x4 world;
  world.translate(world_position);
  world.rotate(yaw, 0.0F, 1.0F, 0.0F);
  add_civilian_actor(ctx,
                     rig,
                     CivilianActor{.archetype = archetype,
                                   .clip = stance.clip,
                                   .phase = stance.phase,
                                   .world = world,
                                   .owner_id = owner,
                                   .instance = instance,
                                   .seed = seed,
                                   .distant = distant,
                                   .blend_clip = stance.blend_clip,
                                   .blend_phase = stance.blend_phase,
                                   .blend_weight = stance.blend_weight});
}

void draw_forge_fire(const DrawContext& ctx,
                     ISubmitter& out,
                     const BarracksGrounds& grounds,
                     float heat,
                     float strike_phase) {
  Mesh* const cone = get_unit_cone();
  Mesh* const cube = get_unit_cube();
  const float time = ctx.animation_time;
  const QVector3D hearth =
      grounds.smithy_centre + k_forge_offset + QVector3D(0.0F, k_forge_top, 0.0F);
  for (int tongue = 0; tongue < 4; ++tongue) {
    const float offset = static_cast<float>(tongue) * 1.7F;
    const float flick =
        0.80F + 0.14F * std::sin(time * (6.1F + static_cast<float>(tongue)) + offset) +
        0.06F * std::sin(time * 12.3F + offset * 1.9F);
    const QVector3D base =
        hearth + QVector3D(std::cos(offset) * 0.06F, 0.0F, std::sin(offset) * 0.05F);
    const QVector3D tip = base + QVector3D(std::sin(time * 2.7F + offset) * 0.03F,
                                           (0.22F + 0.20F * heat) * flick,
                                           0.0F);
    out.mesh(cone,
             ctx.model * Render::Geom::cone_from_to(base, tip, 0.075F),
             (tongue % 2) == 0 ? QVector3D(1.0F, 0.80F, 0.30F)
                               : QVector3D(0.98F, 0.45F, 0.10F),
             nullptr,
             1.0F);
  }

  const float pump = 0.5F + 0.5F * std::sin(time * (2.2F + 2.0F * heat));
  QMatrix4x4 bellows;
  bellows.translate(grounds.smithy_centre + k_forge_offset +
                    QVector3D(-0.02F, 0.20F + 0.035F * pump, 0.30F));
  bellows.scale(0.085F, 0.035F + 0.035F * pump, 0.13F);
  out.mesh(cube, ctx.model * bellows, QVector3D(0.36F, 0.24F, 0.16F), nullptr, 1.0F);

  const QVector3D anvil =
      grounds.smithy_centre + k_anvil_offset + QVector3D(0.0F, k_anvil_top, 0.0F);
  const float glow = 0.70F + 0.30F * std::sin(time * 1.3F);
  QMatrix4x4 bar;
  bar.translate(anvil + QVector3D(0.0F, 0.012F, 0.0F));
  bar.rotate(24.0F, 0.0F, 1.0F, 0.0F);
  bar.scale(0.11F, 0.010F, 0.018F);
  out.mesh(cube,
           ctx.model * bar,
           QVector3D(1.0F, 0.42F + 0.30F * glow * heat, 0.10F + 0.10F * glow),
           nullptr,
           1.0F);

  constexpr float k_impact = 0.46F;
  constexpr float k_spark_span = 0.24F;
  const float since = strike_phase - k_impact;
  if (since >= 0.0F && since < k_spark_span) {
    const float s = since / k_spark_span;
    const auto strike = static_cast<std::uint32_t>(std::floor(time * 3.0F));
    for (std::uint32_t i = 0; i < 9U; ++i) {
      const float angle = roll(strike, i * 3U + 1U) * 2.0F * k_pi;
      const float reach = (0.16F + 0.26F * roll(strike, i * 3U + 2U)) * s;
      const float lift =
          (0.30F + 0.30F * roll(strike, i * 3U + 3U)) * s - 0.42F * s * s;
      const QVector3D at =
          anvil +
          QVector3D(std::cos(angle) * reach, 0.02F + lift, std::sin(angle) * reach);
      QMatrix4x4 spark;
      spark.translate(at);
      spark.scale(0.014F * (1.0F - 0.6F * s));
      out.mesh(cube, ctx.model * spark, QVector3D(1.0F, 0.86F, 0.42F), nullptr, 1.0F);
    }
  }

  Render::LocalLight light;
  light.position = ctx.model.map(hearth + QVector3D(0.0F, 0.25F, 0.0F));
  light.color = QVector3D(1.0F, 0.56F, 0.24F);
  light.radius = 5.5F + 1.5F * heat;
  light.intensity = (0.55F + 0.55F * heat) * (0.92F + 0.05F * std::sin(time * 7.1F) +
                                              0.03F * std::sin(time * 13.0F));
  out.local_light(light);

  out.hearth_smoke(ctx.model.map(hearth + QVector3D(0.0F, 0.72F, 0.0F)),
                   QVector3D(0.34F, 0.33F, 0.33F),
                   0.80F + 0.35F * heat,
                   0.70F + 0.30F * heat,
                   time * (0.7F + 0.3F * heat));

  const float quench = fract(time / 17.0F);
  if (quench > 0.82F) {
    const float burst = std::sin((quench - 0.82F) / 0.18F * k_pi);
    out.hearth_smoke(ctx.model.map(grounds.smithy_centre + k_barrel_offset +
                                   QVector3D(0.0F, 0.34F, 0.0F)),
                     QVector3D(0.92F, 0.93F, 0.95F),
                     0.55F,
                     0.90F * burst,
                     time * 1.6F);
  }
}

} // namespace

void add_barracks_grounds(BuildingArchetypeDesc& desc,
                          const BarracksGrounds& g,
                          const BarracksGroundsStyle& s) {
  const auto intact = k_building_state_mask_intact;
  const auto normal = BuildingStateMask::Normal;

  desc.add_box(QVector3D(g.parade_centre.x(), -0.10F, g.parade_centre.z()),
               QVector3D(g.parade_half_x, 0.16F, g.parade_half_z),
               s.earth);
  desc.add_box(QVector3D(g.parade_centre.x(), 0.065F, g.parade_centre.z()),
               QVector3D(g.parade_half_x - 0.06F, 0.012F, g.parade_half_z - 0.06F),
               s.sand);
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const float x0 = g.parade_centre.x() - g.parade_half_x;
    const float x1 = g.parade_centre.x() + g.parade_half_x;
    const float z0 = g.parade_centre.z() - g.parade_half_z;
    const float z1 = g.parade_centre.z() + g.parade_half_z;
    desc.add_cylinder(QVector3D(x0, 0.09F, z0),
                      QVector3D(x1, 0.09F, z0),
                      0.045F,
                      s.timber_dark,
                      intact);
    desc.add_cylinder(QVector3D(x0, 0.09F, z1),
                      QVector3D(x1, 0.09F, z1),
                      0.045F,
                      s.timber_dark,
                      intact);
    desc.add_cylinder(QVector3D(x0, 0.09F, z0),
                      QVector3D(x0, 0.09F, z1),
                      0.045F,
                      s.timber_dark,
                      intact);
  }
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    for (const float sx : {-1.0F, 1.0F}) {
      for (const float sz : {-1.0F, 1.0F}) {
        desc.add_box(QVector3D(g.parade_centre.x() + sx * g.parade_half_x,
                               0.05F,
                               g.parade_centre.z() + sz * g.parade_half_z),
                     QVector3D(0.07F, 0.05F, 0.07F),
                     s.stone,
                     intact);
      }
    }
  }
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const float palus_x = g.parade_centre.x() - g.parade_half_x + 0.16F;
    for (const float pz : {-0.55F, 0.55F}) {
      const QVector3D foot(palus_x, 0.0F, g.parade_centre.z() + pz);
      desc.add_cylinder(
          foot, foot + QVector3D(0.0F, 0.78F, 0.0F), 0.05F, s.timber, intact);
      desc.add_box(foot + QVector3D(0.0F, 0.56F, 0.0F),
                   QVector3D(0.20F, 0.022F, 0.022F),
                   s.timber_dark,
                   normal);
    }
    const QVector3D dummy(palus_x, 0.0F, g.parade_centre.z() - 0.55F);
    desc.add_box(dummy + QVector3D(0.0F, 0.50F, 0.0F),
                 QVector3D(0.11F, 0.15F, 0.07F),
                 s.straw,
                 normal);
    desc.add_cylinder(dummy + QVector3D(0.0F, 0.78F, 0.0F),
                      dummy + QVector3D(0.0F, 0.92F, 0.0F),
                      0.075F,
                      s.iron * 1.6F,
                      normal);
    desc.add_cone(dummy + QVector3D(0.0F, 0.92F, 0.0F),
                  dummy + QVector3D(0.0F, 1.04F, 0.03F),
                  0.035F,
                  s.team_cloth,
                  normal);
  }

  const QVector3D c = g.smithy_centre;
  desc.add_box(
      QVector3D(c.x(), -0.10F, c.z()), QVector3D(0.86F, 0.13F, 0.56F), s.earth_dark);
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    const QVector3D forge = c + k_forge_offset;
    desc.add_box(forge + QVector3D(0.0F, 0.15F, 0.0F),
                 QVector3D(0.20F, 0.15F, 0.18F),
                 s.stone,
                 intact);
    desc.add_box(forge + QVector3D(0.0F, 0.325F, 0.0F),
                 QVector3D(0.22F, 0.025F, 0.20F),
                 s.stone_dark,
                 intact);
    desc.add_box(forge + QVector3D(0.0F, k_forge_top - 0.005F, 0.0F),
                 QVector3D(0.14F, 0.012F, 0.12F),
                 QVector3D(0.16F, 0.07F, 0.04F),
                 intact);
    desc.add_box(forge + QVector3D(-0.17F, 0.62F, -0.02F),
                 QVector3D(0.05F, 0.28F, 0.16F),
                 s.stone_dark,
                 normal);
    desc.add_box(forge + QVector3D(-0.04F, 0.92F, -0.02F),
                 QVector3D(0.18F, 0.03F, 0.18F),
                 s.stone_dark,
                 normal);
  }
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const QVector3D anvil = c + k_anvil_offset;
    desc.add_cylinder(
        anvil, anvil + QVector3D(0.0F, 0.26F, 0.0F), 0.10F, s.timber_dark, intact);
    const QVector3D barrel = c + k_barrel_offset;
    desc.add_cylinder(
        barrel, barrel + QVector3D(0.0F, 0.30F, 0.0F), 0.11F, s.timber, intact);
    desc.add_cylinder(barrel + QVector3D(0.0F, 0.285F, 0.0F),
                      barrel + QVector3D(0.0F, 0.30F, 0.0F),
                      0.095F,
                      QVector3D(0.20F, 0.33F, 0.40F),
                      normal);
    for (const float px : {-0.80F, -0.10F}) {
      for (const float pz : {-0.44F, 0.44F}) {
        const float top = pz > 0.0F ? 1.22F : 1.02F;
        desc.add_box(c + QVector3D(px, top * 0.5F, pz),
                     QVector3D(0.035F, top * 0.5F, 0.035F),
                     s.timber_dark,
                     intact);
      }
    }
    const QVector3D rack = c + QVector3D(0.62F, 0.0F, -0.30F);
    for (const float dz : {-0.22F, 0.22F}) {
      desc.add_box(rack + QVector3D(0.0F, 0.34F, dz),
                   QVector3D(0.022F, 0.34F, 0.022F),
                   s.timber,
                   intact);
    }
    desc.add_box(rack + QVector3D(0.0F, 0.64F, 0.0F),
                 QVector3D(0.022F, 0.018F, 0.25F),
                 s.timber,
                 intact);
    for (const float dz : {-0.14F, -0.05F, 0.04F, 0.13F}) {
      desc.add_rotated_box(rack + QVector3D(0.03F, 0.40F, dz),
                           QVector3D(0.006F, 0.26F, 0.016F),
                           QVector3D(0.0F, 0.0F, -9.0F),
                           s.iron * 2.4F,
                           normal);
    }
  }
  {
    BuildingPartMaterial const metal(desc, k_building_material_metal);
    const QVector3D anvil = c + k_anvil_offset;
    desc.add_box(anvil + QVector3D(0.0F, 0.31F, 0.0F),
                 QVector3D(0.10F, 0.05F, 0.055F),
                 s.iron,
                 intact);
    desc.add_cone(anvil + QVector3D(0.10F, 0.32F, 0.0F),
                  anvil + QVector3D(0.22F, 0.34F, 0.0F),
                  0.04F,
                  s.iron,
                  intact);
  }
  desc.add_rotated_box(c + QVector3D(-0.45F, 1.16F, 0.0F),
                       QVector3D(0.42F, 0.022F, 0.56F),
                       QVector3D(12.5F, 0.0F, 0.0F),
                       s.roof,
                       normal);
  for (const float rx : {-0.74F, -0.45F, -0.16F}) {
    desc.add_rotated_box(c + QVector3D(rx, 1.185F, 0.0F),
                         QVector3D(0.035F, 0.016F, 0.57F),
                         QVector3D(12.5F, 0.0F, 0.0F),
                         s.roof_dark,
                         normal);
  }
}

void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds) {
  if (ctx.entity == nullptr || ctx.template_prewarm) {
    return;
  }
  if (resolve_building_state(ctx) != BuildingState::Normal) {
    return;
  }
  if (ctx.distance_sq > k_far_metres * k_far_metres) {
    return;
  }

  const auto* production =
      ctx.entity->registry() != nullptr
          ? ctx.entity->registry()->try_get<Engine::Core::ProductionComponent>(
                ctx.entity->get_id())
          : nullptr;
  const bool training = production != nullptr && production->in_progress;
  const float heat = training ? 1.0F : 0.35F;
  const float time = ctx.animation_time;
  const auto owner = static_cast<std::uint32_t>(ctx.entity->get_id());

  const float strike_seconds = training ? 0.95F : 1.60F;
  const float strike_phase = fract(time / strike_seconds + roll(owner, 91U));
  draw_forge_fire(ctx, out, grounds, heat, strike_phase);

  auto* activity = ctx.home_activity;
  if (activity == nullptr || !activity->actors_allowed ||
      activity->remaining_ambient_actors <= 0 || ctx.submission_visibility == nullptr) {
    return;
  }
  const float scale = ctx.model.column(0).toVector3D().length();
  const float pixels =
      ctx.screen_metrics.projected_radius_px_from_distance_sq(ctx.distance_sq, scale);
  if (pixels >= 0.0F && pixels < k_min_pixels) {
    return;
  }
  const QVector3D centre = ctx.model.column(3).toVector3D();
  const auto* fog = ctx.submission_visibility->snapshot();
  if (ctx.submission_fog_mode != SubmissionFogMode::Ignore && fog != nullptr &&
      fog->initialized && !fog->is_visible_world(centre.x(), centre.z())) {
    return;
  }
  const auto& civilian = nation_civilian_rig(carthage);
  const auto& crew = nation_crew_rig(carthage);
  if (!civilian.valid()) {
    return;
  }
  const bool distant = pixels >= 0.0F && pixels < 60.0F;
  const auto armed = civilian.armed != Render::Creature::k_invalid_archetype
                         ? civilian.armed
                         : civilian.idle;

  begin_civilian_actors();
  std::uint16_t instance = 80;

  const QVector3D forward(0.0F, 0.0F, 1.0F);
  const QVector3D master_local =
      grounds.parade_centre +
      QVector3D(0.0F, k_parade_floor, grounds.parade_half_z - 0.22F);
  const QVector3D master_world = ctx.model.map(master_local);

  for (int index = 0; index < k_recruits; ++index) {
    if (activity->remaining_ambient_actors <= 0) {
      break;
    }
    const std::uint32_t seed =
        ambient_hash(owner * 2246822519U + static_cast<std::uint32_t>(index) + 17U);
    const int row = index / k_columns;
    const int column = index % k_columns;
    QVector3D local = grounds.parade_centre +
                      QVector3D((static_cast<float>(column) - 0.5F) * k_column_spacing,
                                k_parade_floor,
                                (0.2F - static_cast<float>(row)) * k_row_spacing);
    Stance stance;
    float yaw_offset = 0.0F;
    Render::Creature::ArchetypeId archetype = armed;
    if (training) {
      const bool clumsy = index == k_clumsy_recruit;
      const float lag = roll(seed, 3U) * 0.09F + (clumsy ? 0.36F : 0.0F);
      stance = drill_stance(time - lag, clumsy);
    } else {
      stance = at_ease_stance(time, seed);
      yaw_offset = (roll(seed, 5U) - 0.5F) * 110.0F;
      local += QVector3D(
          (roll(seed, 6U) - 0.5F) * 0.22F, 0.0F, (roll(seed, 7U) - 0.5F) * 0.22F);
      archetype = civilian.idle;
    }
    const QVector3D world = ctx.model.map(local);
    const float yaw = yaw_of(ctx.model.map(local + forward) - world) + yaw_offset;
    add_actor(
        ctx, civilian, archetype, world, yaw, stance, owner, instance++, seed, distant);
    --activity->remaining_ambient_actors;
  }

  if (activity->remaining_ambient_actors > 0) {
    const std::uint32_t seed = ambient_hash(owner * 668265263U + 5U);
    Stance stance;
    float yaw = yaw_of(ctx.model.map(master_local - forward) - master_world);
    if (training) {
      const Stance squad = drill_stance(time + 0.25F, false);
      const bool calling = !k_drill[static_cast<std::size_t>(squad.beat)].looping;
      if (calling) {
        stance.clip = Animation::k_humanoid_taunt_dismissive_clip;
        stance.phase = squad.phase;
      } else {
        stance.clip = Animation::k_humanoid_idle_clip;
        stance.phase = fract(time / k_idle_cycle_seconds);
      }
      yaw += 14.0F * std::sin(time * 0.6F);
    } else {
      stance.clip = Animation::k_humanoid_idle_clip;
      stance.phase = fract(time / k_idle_cycle_seconds + 0.3F);
      yaw += 35.0F * std::sin(time * 0.17F);
    }
    add_actor(ctx,
              civilian,
              armed,
              master_world,
              yaw,
              stance,
              owner,
              instance++,
              seed,
              distant);
    --activity->remaining_ambient_actors;
  }
  submit_civilian_actors(out);

  if (crew.valid() && activity->remaining_ambient_actors > 0) {
    begin_civilian_actors();
    const std::uint32_t seed = ambient_hash(owner * 3266489917U + 9U);
    const QVector3D smith_local = grounds.smithy_centre + k_smith_offset;
    const QVector3D anvil_local = grounds.smithy_centre + k_anvil_offset;
    const QVector3D world = ctx.model.map(smith_local);
    Stance stance;
    stance.clip = Animation::k_humanoid_construct_hammer_clip;
    stance.phase = strike_phase;
    add_actor(ctx,
              crew,
              crew.working,
              world,
              yaw_of(ctx.model.map(anvil_local) - world),
              stance,
              owner,
              instance++,
              seed,
              distant);
    --activity->remaining_ambient_actors;
    submit_civilian_actors(out);
  }
}

} // namespace Render::GL
