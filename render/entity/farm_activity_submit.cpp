#include "render/entity/farm_activity_submit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "animation/clip_manifest.h"
#include "game/core/component_economy.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_worker_props.h"
#include "render/entity/registry.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/render_archetype.h"

namespace Render::GL {

namespace {

constexpr float k_pi = 3.14159265F;
constexpr float k_tend_cycle_seconds = 2.6F;
constexpr float k_reap_cycle_seconds = 2.2F;
constexpr float k_kneel_cycle_seconds = 3.0F;
constexpr float k_idle_cycle_seconds = 8.0F;
constexpr float k_soil_y = 0.035F;

constexpr float k_blend_seconds = 0.35F;

constexpr float k_turn_metres = 0.45F;

constexpr float k_lane_jitter = 0.03F;

constexpr float k_bundle_x = 0.78F;

constexpr auto mix = ambient_hash;

auto salt(std::uint32_t seed, std::uint32_t tag) -> std::uint32_t {
  return mix(seed ^ (tag * 0x9E3779B9U));
}

auto unit(std::uint32_t hash) -> float {
  return static_cast<float>(hash & 0xFFFFU) / 65536.0F;
}

auto field_seed(std::uint64_t id) -> std::uint32_t {
  return mix(static_cast<std::uint32_t>(id) ^ static_cast<std::uint32_t>(id >> 32U));
}

auto smoothstep(float t) -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto fract(float value) -> float {
  return value - std::floor(value);
}

auto yaw_of(const QVector3D& direction) -> float {
  if (direction.x() * direction.x() + direction.z() * direction.z() < 1e-8F) {
    return 0.0F;
  }
  return std::atan2(direction.x(), direction.z()) * 180.0F / k_pi;
}

auto lerp_yaw(float from, float to, float t) -> float {
  float delta = std::fmod(to - from + 540.0F, 360.0F) - 180.0F;
  return from + delta * std::clamp(t, 0.0F, 1.0F);
}

auto flat(QVector3D v) -> QVector3D {
  v.setY(0.0F);
  const float length = v.length();
  return length > 1e-6F ? v / length : QVector3D(0.0F, 0.0F, 1.0F);
}

auto walk_fraction(float gag_time) -> float {
  if (gag_time < Nap::k_approach_start) {
    return 0.0F;
  }
  if (gag_time < Nap::k_approach_end) {
    return (gag_time - Nap::k_approach_start) /
           (Nap::k_approach_end - Nap::k_approach_start);
  }
  if (gag_time < Nap::k_return_start) {
    return 1.0F;
  }
  const float back =
      (gag_time - Nap::k_return_start) / (Nap::k_sequence_end - Nap::k_return_start);
  return 1.0F - std::clamp(back, 0.0F, 1.0F);
}

auto walk_is_moving(float gag_time) -> bool {
  return (gag_time >= Nap::k_approach_start && gag_time < Nap::k_approach_end) ||
         gag_time >= Nap::k_return_start;
}

auto yaw_towards(const QVector3D& from,
                 const QVector3D& to,
                 const QVector3D& fallback) -> float {
  auto direction = to - from;
  direction.setY(0.0F);
  if (direction.lengthSquared() < 1e-4F) {
    direction = fallback - from;
    direction.setY(0.0F);
  }
  if (direction.lengthSquared() < 1e-4F) {
    return 0.0F;
  }
  return std::atan2(direction.x(), direction.z()) * 180.0F / k_pi;
}

enum StookSlot : std::uint8_t {
  k_stook_straw = 0U,
  k_stook_shade = 1U,
};
constexpr int k_stook_min_sheaves = 5;
constexpr int k_stook_max_sheaves = 8;

auto build_stook(int sheaves) -> RenderArchetype {

  std::vector<GeneratedEquipmentPrimitive> primitives;
  primitives.reserve(static_cast<std::size_t>(sheaves) * 3U);
  const std::uint32_t seed = mix(0x5700CU + static_cast<std::uint32_t>(sheaves));
  for (int i = 0; i < sheaves; ++i) {
    const auto h = salt(seed, static_cast<std::uint32_t>(i) + 1U);
    const float angle =
        (static_cast<float>(i) / static_cast<float>(sheaves)) * 2.0F * k_pi +
        (unit(h) - 0.5F) * 0.5F;
    const float lean = 0.052F + unit(salt(h, 2U)) * 0.012F;
    const float height = 0.205F + unit(salt(h, 3U)) * 0.03F;
    const QVector3D foot(std::cos(angle) * lean, 0.0F, std::sin(angle) * lean);
    const QVector3D head(std::cos(angle) * 0.010F, height, std::sin(angle) * 0.010F);
    const QVector3D band = foot + (head - foot) * 0.58F;

    primitives.push_back(generated_cone(foot, band, 0.0135F, k_stook_straw));
    primitives.push_back(generated_cone(head, band, 0.0125F, k_stook_straw));
    primitives.push_back(generated_cylinder(band - (head - foot) * 0.03F,
                                            band + (head - foot) * 0.03F,
                                            0.0075F,
                                            k_stook_shade));
  }

  primitives.push_back(generated_cylinder(QVector3D(0.0F, 0.000F, 0.0F),
                                          QVector3D(0.0F, 0.006F, 0.0F),
                                          0.085F,
                                          k_stook_shade));
  return build_generated_equipment_archetype(
      "farm_stook_" + std::to_string(sheaves),
      std::span<const GeneratedEquipmentPrimitive>(primitives.data(),
                                                   primitives.size()));
}

auto stook_archetype(int sheaves) -> const RenderArchetype& {
  static const auto table = []() {
    std::array<RenderArchetype, k_stook_max_sheaves - k_stook_min_sheaves + 1> built{};
    for (int n = k_stook_min_sheaves; n <= k_stook_max_sheaves; ++n) {
      built[static_cast<std::size_t>(n - k_stook_min_sheaves)] = build_stook(n);
    }
    return built;
  }();
  const int clamped = std::clamp(sheaves, k_stook_min_sheaves, k_stook_max_sheaves);
  return table[static_cast<std::size_t>(clamped - k_stook_min_sheaves)];
}

struct GathererPlan {
  float bundle{0.0F};
  float lift{0.0F};
  float drop{0.0F};
  float walk_speed{0.0F};
  float offset_fraction{0.0F};
  float bundle_z{0.0F};
  bool hatted{true};
};

auto gatherer_plan(std::uint64_t field_id) -> GathererPlan {
  const auto w = salt(field_seed(field_id), 601U);
  GathererPlan plan{};
  plan.bundle = 5.5F + 5.0F * unit(salt(w, 1U));
  plan.lift = 0.9F + 0.3F * unit(salt(w, 2U));
  plan.drop = 1.0F + 0.4F * unit(salt(w, 3U));
  plan.walk_speed = 0.66F + 0.20F * unit(salt(w, 4U));
  plan.offset_fraction = unit(salt(w, 5U));
  plan.bundle_z = 0.02F + 0.28F * unit(salt(w, 6U));
  plan.hatted = (salt(w, 7U) % 5U) != 0U;
  return plan;
}

struct LanePlace {
  QVector3D position;
  float yaw{0.0F};
  float heading{1.0F};
};

auto place_on_lane(const QVector3D& a,
                   const QVector3D& b,
                   float start_fraction,
                   float advance) -> LanePlace {
  LanePlace place{};
  const QVector3D span = b - a;
  const float length = std::max(span.length(), 0.01F);
  const QVector3D direction = span / length;
  const float loop_length = 2.0F * length;
  float loop = std::fmod(start_fraction * loop_length + advance, loop_length);
  if (loop < 0.0F) {
    loop += loop_length;
  }
  const bool forward = loop < length;
  const float along = forward ? loop : loop_length - loop;
  const float since_turn = forward ? loop : loop - length;
  place.position = a + direction * along;
  place.heading = forward ? 1.0F : -1.0F;
  const float now = yaw_of(direction * place.heading);
  const float before = yaw_of(direction * -place.heading);
  place.yaw = lerp_yaw(before, now, smoothstep(since_turn / k_turn_metres));
  return place;
}

struct ActorPose {
  QVector3D position;
  float yaw{0.0F};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  std::uint16_t clip{0};
  float phase{0.0F};
  bool held{false};
  std::uint16_t blend_clip{Animation::k_unmapped_clip};
  float blend_phase{0.0F};
  float blend_weight{0.0F};
};

void blend_in(ActorPose& pose,
              std::uint16_t from_clip,
              float from_phase,
              float elapsed) {
  pose.blend_clip = from_clip;
  pose.blend_phase = from_phase;
  pose.blend_weight = 1.0F - smoothstep(elapsed / k_blend_seconds);
}

struct WorkerContext {
  const FarmWorkerVisual* visual{nullptr};
  const NationCivilianRig* rig{nullptr};
  bool ripe{false};
};

auto lane_worker_pose(const WorkerContext& wc,
                      int worker,
                      const QVector3D& lane_a,
                      const QVector3D& lane_b,
                      const QVector3D& crop_direction,
                      const FarmWorkerBeat& beat) -> ActorPose {
  const auto place = place_on_lane(lane_a, lane_b, beat.start_fraction, beat.advance);
  const bool headland = worker == k_farm_headland_lane;
  ActorPose pose{};
  pose.position = place.position;

  const float work_yaw = headland ? yaw_of(crop_direction) + place.heading * 16.0F
                                  : place.yaw + beat.face_tilt;
  if (headland) {
    pose.archetype = beat.hatted ? wc.visual->hatted_tending : wc.rig->idle;
    pose.clip = Animation::k_humanoid_construct_kneel_chisel_clip;
  } else if (wc.ripe) {
    pose.archetype = beat.hatted ? wc.visual->hatted_reaping : wc.rig->working;
    pose.clip = Animation::k_humanoid_construct_reap_clip;
  } else {
    pose.archetype = beat.hatted ? wc.visual->hatted_tending : wc.rig->idle;
    pose.clip = Animation::k_humanoid_construct_chisel_clip;
  }
  const std::uint16_t work_clip = pose.clip;
  switch (beat.kind) {
  case FarmWorkerBeat::Kind::Work:
    pose.phase = fract(beat.elapsed / beat.stroke_cycle);
    pose.yaw = lerp_yaw(place.yaw, work_yaw, smoothstep(beat.elapsed / 0.7F));
    blend_in(pose, Animation::k_humanoid_walk_clip, beat.previous_phase, beat.elapsed);
    break;
  case FarmWorkerBeat::Kind::Pause:

    pose.clip = Animation::k_humanoid_idle_clip;
    pose.phase = fract(beat.elapsed / k_idle_cycle_seconds);
    pose.yaw = lerp_yaw(
        work_yaw, place.yaw, smoothstep(beat.elapsed / std::min(1.4F, beat.duration)));
    blend_in(pose, work_clip, beat.previous_phase, beat.elapsed);
    break;
  case FarmWorkerBeat::Kind::Step:
    pose.clip = Animation::k_humanoid_walk_clip;

    pose.phase = fract(beat.walked / Nap::k_walk_metres_per_cycle);
    pose.yaw = place.yaw;
    blend_in(pose, Animation::k_humanoid_idle_clip, beat.previous_phase, beat.elapsed);
    break;
  }
  return pose;
}

auto gatherer_pose(const WorkerContext& wc,
                   const QVector3D& bundle_spot,
                   const QVector3D& drop_spot,
                   const QVector3D& stook,
                   const QVector3D& crop_direction,
                   const FarmGathererBeat& beat) -> ActorPose {
  const float trip = std::max((drop_spot - bundle_spot).length(), 0.01F);
  const float to_stook_yaw = yaw_of(flat(drop_spot - bundle_spot));
  const float to_bundle_yaw = yaw_of(flat(bundle_spot - drop_spot));
  const float crop_yaw = yaw_of(crop_direction);
  const auto plain = beat.hatted ? wc.visual->hatted_tending : wc.rig->idle;
  const auto laden =
      beat.hatted ? wc.visual->hatted_gathering : wc.visual->bare_gathering;
  ActorPose pose{};
  switch (beat.kind) {
  case FarmGathererBeat::Kind::Bundle:
    pose.position = bundle_spot;
    pose.archetype = plain;
    pose.clip = Animation::k_humanoid_construct_kneel_chisel_clip;
    pose.phase = fract(beat.elapsed / k_kneel_cycle_seconds);
    pose.yaw = lerp_yaw(to_bundle_yaw, crop_yaw, smoothstep(beat.elapsed / 0.8F));
    blend_in(pose, Animation::k_humanoid_walk_clip, 0.0F, beat.elapsed);
    break;
  case FarmGathererBeat::Kind::Lift:
    pose.position = bundle_spot;
    pose.archetype = laden;
    pose.clip = Animation::k_humanoid_idle_clip;
    pose.phase = fract(beat.elapsed / k_idle_cycle_seconds);
    pose.yaw =
        lerp_yaw(crop_yaw, to_stook_yaw, smoothstep(beat.elapsed / beat.duration));
    blend_in(
        pose, Animation::k_humanoid_construct_kneel_chisel_clip, 0.0F, beat.elapsed);
    break;
  case FarmGathererBeat::Kind::Carry:
    pose.position = bundle_spot + (drop_spot - bundle_spot) * (beat.walked / trip);
    pose.archetype = laden;
    pose.clip = Animation::k_humanoid_walk_clip;
    pose.phase = fract(beat.walked / Nap::k_walk_metres_per_cycle);
    pose.yaw = to_stook_yaw;
    blend_in(pose, Animation::k_humanoid_idle_clip, 0.0F, beat.elapsed);
    break;
  case FarmGathererBeat::Kind::Drop:
    pose.position = drop_spot;
    pose.archetype = plain;
    pose.clip = Animation::k_humanoid_idle_clip;
    pose.phase = fract(beat.elapsed / k_idle_cycle_seconds);
    pose.yaw = yaw_of(flat(stook - drop_spot));
    blend_in(pose,
             Animation::k_humanoid_walk_clip,
             fract(trip / Nap::k_walk_metres_per_cycle),
             beat.elapsed);
    break;
  case FarmGathererBeat::Kind::Return:
    pose.position = drop_spot + (bundle_spot - drop_spot) * (beat.walked / trip);
    pose.archetype = plain;
    pose.clip = Animation::k_humanoid_walk_clip;
    pose.phase = fract(beat.walked / Nap::k_walk_metres_per_cycle);
    pose.yaw = lerp_yaw(yaw_of(flat(stook - drop_spot)),
                        to_bundle_yaw,
                        smoothstep(beat.walked / k_turn_metres));
    blend_in(pose, Animation::k_humanoid_idle_clip, 0.0F, beat.elapsed);
    break;
  }
  return pose;
}

template <typename Actor>
concept HasClipBlend = requires(Actor actor) {
  actor.blend_clip;
  actor.blend_phase;
  actor.blend_weight;
};

template <typename Actor>
void apply_clip_blend(Actor& actor, const ActorPose& pose) {
  if constexpr (HasClipBlend<Actor>) {
    if (pose.blend_weight > 0.0F && pose.blend_clip != pose.clip) {
      actor.blend_clip = pose.blend_clip;
      actor.blend_phase = pose.blend_phase;
      actor.blend_weight = pose.blend_weight;
    }
  } else {
    (void)actor;
    (void)pose;
  }
}

} // namespace

void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage) {
  auto* activity = ctx.farm_activity;
  if (activity == nullptr || activity->workers_per_field == 0 ||
      ctx.entity == nullptr || ctx.template_prewarm) {
    return;
  }
  const auto& visual = farm_worker_visual(carthage);
  if (!visual.valid()) {
    return;
  }

  const auto* field = activity->field(ctx.entity->get_id());
  if (field == nullptr) {
    return;
  }
  const float scale = ctx.model.column(1).toVector3D().length();
  const float pixels = ctx.screen_metrics.projected_radius_px_from_distance_sq(
      ctx.distance_sq, scale * 0.35F);
  if ((pixels >= 0 && pixels < 7) || ctx.distance_sq > 160.0F * 160.0F) {
    return;
  }
  const auto seed = field_seed(field->id);
  const bool ripe = field->growth >= 1.0F;
  const float time = ctx.animation_time;

  if (ripe) {
    auto model = ctx.model;
    model.translate(farm_activity_stook_anchor());
    model.rotate(unit(salt(seed, 51U)) * 360.0F, 0.0F, 1.0F, 0.0F);
    const float bleach = 0.90F + 0.16F * unit(salt(seed, 52U));
    const std::array<QVector3D, 2> palette{QVector3D(0.79F, 0.66F, 0.36F) * bleach,
                                           QVector3D(0.61F, 0.48F, 0.24F) * bleach};
    submit_render_instance(
        out,
        RenderInstance{.archetype = &stook_archetype(k_stook_min_sheaves +
                                                     std::min(field->harvests, 3)),
                       .world = model,
                       .palette = palette,
                       .alpha_multiplier = ctx.alpha_multiplier});
  }
  if (activity->remaining == 0) {
    return;
  }

  const auto roster = farm_activity_roster(field->id);
  const int count = std::min({roster.count,
                              activity->workers_per_field,
                              activity->remaining,
                              pixels >= 0 && pixels < 15 ? 1 : 3});
  const auto& rig = nation_civilian_rig(carthage);
  const WorkerContext wc{&visual, &rig, ripe};

  const auto to_world = [&](const QVector3D& local) {
    return ctx.model.map(local);
  };
  const auto to_direction = [&](const QVector3D& local) {
    return flat(ctx.model.mapVector(local));
  };
  const auto ground = [&](QVector3D point) {
    if (ctx.world_view.has_terrain()) {

      point.setY(std::max(point.y(),
                          ctx.world_view.terrain()->resolve_surface_world_y(
                              point.x(), point.z(), k_soil_y * scale, point.y())));
    }
    return point;
  };
  const bool rows_along_x = !carthage;
  const QVector3D stook_world = to_world(farm_activity_stook_anchor());
  const QVector3D drop_world = to_world(farm_activity_stook_drop());

  const auto sample = [&](int slot, float at) -> ActorPose {
    const int worker = roster.worker[static_cast<std::size_t>(slot)];
    ActorPose pose{};
    if (worker == k_farm_headland_lane && ripe) {
      const float bundle_z = gatherer_plan(field->id).bundle_z;
      const QVector3D bundle_world =
          to_world(QVector3D(k_bundle_x, k_soil_y, bundle_z));
      const float trip = (drop_world - bundle_world).length();
      pose = gatherer_pose(wc,
                           bundle_world,
                           drop_world,
                           stook_world,
                           to_direction(QVector3D(-1.0F, 0.0F, 0.0F)),
                           farm_gatherer_beat(field->id, trip, at));
    } else {
      auto lane = farm_activity_lane(rows_along_x, worker);

      const float jitter =
          (unit(salt(seed, 71U + static_cast<std::uint32_t>(worker))) - 0.5F) * 2.0F *
          k_lane_jitter;
      lane.from += lane.crop * jitter;
      lane.to += lane.crop * jitter;
      pose = lane_worker_pose(wc,
                              worker,
                              to_world(lane.from),
                              to_world(lane.to),
                              to_direction(lane.crop),
                              farm_worker_beat(field->id, worker, ripe, at));
    }
    pose.position = ground(pose.position);
    return pose;
  };

  std::array<ActorPose, 3> poses{};
  std::array<bool, 3> visible{};
  const auto* visibility = ctx.submission_visibility != nullptr
                               ? ctx.submission_visibility->snapshot()
                               : nullptr;
  const auto fog_mode = ctx.submission_fog_mode == SubmissionFogMode::Ignore
                            ? SubmissionFogMode::Ignore
                            : SubmissionFogMode::VisibleOnly;
  const auto can_see = [&](const QVector3D& anchor) {
    return ctx.submission_visibility != nullptr &&
           (fog_mode == SubmissionFogMode::Ignore || visibility == nullptr ||
            !visibility->initialized ||
            visibility->is_visible_world(anchor.x(), anchor.z())) &&
           ctx.submission_visibility
               ->evaluate_sphere_with_margin(anchor, 1.2F, fog_mode, FogExtent::Anchor)
               .accepted() &&
           !ctx.submission_visibility->occludes_lens_gap(anchor);
  };
  for (int i = 0; i < count; ++i) {
    poses[static_cast<std::size_t>(i)] = sample(i, time);
    visible[static_cast<std::size_t>(i)] =
        can_see(poses[static_cast<std::size_t>(i)].position);
  }

  const bool gag_cast = count >= 2 && roster.worker[1] == 1 && visible[0] && visible[1];
  const float gag_time =
      gag_cast ? activity->gag(field->id,
                               farm_activity_growth_stage(field->growth),
                               time,
                               farm_worker_stays_put(
                                   field->id, 0, ripe, time, Nap::k_sequence_end))
               : -1.0F;
  const bool gag_running = gag_time >= 0 && gag_time < Nap::k_sequence_end;

  begin_civilian_actors();
  for (int i = 0; i < count; ++i) {
    const auto slot = static_cast<std::size_t>(i);
    if (!visible[slot]) {
      continue;
    }
    auto pose = poses[slot];
    const auto work_cycle = ripe ? k_reap_cycle_seconds : k_tend_cycle_seconds;

    if (gag_running && i == 0) {

      const auto found = sample(0, activity->gag_started);
      pose.yaw = found.yaw;
      pose.blend_weight = 0.0F;
      if (gag_time < Nap::k_sit_down_end) {
        pose.clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
        pose.phase = gag_time / Nap::k_sit_down_end;
        pose.held = true;
      } else if (gag_time < Nap::k_asleep_end) {
        pose.archetype = visual.napping;
        pose.clip = Animation::k_humanoid_showcase_rest_sit_clip;
        pose.phase = 0.0F;
        pose.held = true;
      } else if (gag_time < Nap::k_get_up_end) {

        pose.clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
        pose.phase = 1.0F - ((gag_time - Nap::k_asleep_end) /
                             (Nap::k_get_up_end - Nap::k_asleep_end));
        pose.held = true;
      } else if (gag_time < Nap::k_pretend_end) {
        pose.clip = found.clip;
        pose.archetype = found.archetype;
        pose.phase = fract((gag_time - Nap::k_get_up_end) / (work_cycle * 0.45F));
      }
    }

    if (gag_running && i == 1) {

      const float travel = walk_fraction(gag_time);
      if (travel > 0.0F) {
        const auto sleeper = poses[0].position;
        const auto left = sample(1, activity->gag_started).position;
        const auto home = gag_time >= Nap::k_return_start ? pose.position : left;
        const auto stop = sleeper + (flat(left - sleeper) * 1.6F);
        const auto here = ground(home + ((stop - home) * travel));
        const bool moving = walk_is_moving(gag_time);
        pose.position = here;
        pose.yaw = yaw_towards(here,
                               moving ? (gag_time >= Nap::k_return_start ? home : stop)
                                      : sleeper,
                               left);
        pose.blend_weight = 0.0F;
        pose.held = false;
        if (moving) {
          pose.clip = Animation::k_humanoid_walk_clip;
          pose.phase =
              fract((home - stop).length() * travel / Nap::k_walk_metres_per_cycle);
        } else {
          pose.clip = Animation::k_humanoid_idle_clip;
          pose.phase = fract(time / k_idle_cycle_seconds);
        }
      }
    }

    QMatrix4x4 world;
    world.translate(pose.position);
    world.rotate(pose.yaw, 0.0F, 1.0F, 0.0F);
    CivilianActor actor{
        .archetype = pose.archetype,
        .clip = pose.clip,
        .phase = pose.held ? std::clamp(pose.phase, 0.0F, 1.0F) : fract(pose.phase),
        .world = world,
        .owner_id = static_cast<std::uint32_t>(field->id),
        .instance = static_cast<std::uint16_t>(i + 1),
        .seed = salt(seed, 977U + static_cast<std::uint32_t>(roster.worker[slot])),
        .distant = pixels >= 0 && pixels < 12,
    };
    apply_clip_blend(actor, pose);
    add_civilian_actor(ctx, rig, actor);
    --activity->remaining;
  }
  submit_civilian_actors(out);
}

} // namespace Render::GL
