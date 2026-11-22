#include "render/entity/farm_activity.h"

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
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/render_archetype.h"

namespace Render::GL {

namespace {

constexpr float k_tend_cycle_seconds = 2.6F;
constexpr float k_reap_cycle_seconds = 2.2F;
constexpr float k_kneel_cycle_seconds = 3.0F;
constexpr float k_idle_cycle_seconds = 8.0F;
constexpr float k_soil_y = 0.035F;

constexpr float k_lane_jitter = 0.03F;
constexpr float k_lane_clear_half_width = 0.075F;
constexpr float k_headland_x = 0.80F;

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

auto fract(float value) -> float {
  return value - std::floor(value);
}

auto derive_visual(bool carthage) -> FarmWorkerVisual {

  const auto& rig = nation_civilian_rig(carthage);
  FarmWorkerVisual visual{};
  if (!rig.valid()) {
    return visual;
  }
  const auto& props = farm_worker_props();
  const std::string nation = carthage ? "carthage" : "roman";
  const std::array<EquipmentHandle, 1> hat{props.sun_hat};
  const std::array<EquipmentHandle, 2> hat_and_sheaf{props.sun_hat, props.wheat_sheaf};
  const std::array<EquipmentHandle, 1> sheaf{props.wheat_sheaf};
  const std::array<EquipmentHandle, 1> tilted{props.tilted_sun_hat};
  visual.hatted_tending = resolve_humanoid_equipment_archetype(
      "farm/worker/" + nation + "/tend", rig.idle, hat);
  visual.hatted_reaping = resolve_humanoid_equipment_archetype(
      "farm/worker/" + nation + "/reap", rig.working, hat);
  visual.hatted_gathering = resolve_humanoid_equipment_archetype(
      "farm/worker/" + nation + "/gather", rig.idle, hat_and_sheaf);
  visual.bare_gathering = resolve_humanoid_equipment_archetype(
      "farm/worker/" + nation + "/gather_bare", rig.idle, sheaf);
  visual.napping = resolve_humanoid_equipment_archetype(
      "farm/worker/" + nation + "/nap", rig.working, tilted);
  return visual;
}

struct Bout {
  float work{0.0F};
  float pause{0.0F};
  float step{0.0F};
  float step_metres{0.0F};
  float stroke_cycle{0.0F};
};

struct WorkerPlan {
  std::array<Bout, 3> bouts{};
  float period{0.0F};
  float offset{0.0F};
  float loop_metres{0.0F};
  float stroke_cycle{0.0F};
  float walk_speed{0.0F};
  float start_fraction{0.0F};
  float face_tilt{0.0F};
  bool hatted{true};
};

auto worker_plan(std::uint64_t field_id, int worker, bool ripe) -> WorkerPlan {
  const auto w =
      salt(field_seed(field_id), 101U + static_cast<std::uint32_t>(worker) * 17U);
  WorkerPlan plan{};
  const bool kneeling = worker == k_farm_headland_lane;
  const float jitter = 0.86F + 0.28F * unit(salt(w, 1U));
  const float schedule_cycle =
      (kneeling ? k_kneel_cycle_seconds : k_tend_cycle_seconds) * jitter;
  const float base = kneeling ? k_kneel_cycle_seconds
                     : ripe   ? k_reap_cycle_seconds
                              : k_tend_cycle_seconds;
  plan.stroke_cycle = base * jitter;

  plan.walk_speed = 0.62F + 0.18F * unit(salt(w, 2U));
  for (std::uint32_t j = 0; j < 3U; ++j) {
    auto& bout = plan.bouts[j];
    const auto strokes =
        static_cast<float>((kneeling ? 4U : 3U) + (salt(w, 10U + j) % 4U));
    bout.work = strokes * schedule_cycle;
    const float wanted = base * jitter;
    bout.stroke_cycle = bout.work / std::max(1.0F, std::round(bout.work / wanted));
    bout.pause = 1.3F + 2.4F * unit(salt(w, 20U + j));
    bout.step = 1.0F + 1.1F * unit(salt(w, 30U + j));
    bout.step_metres = bout.step * plan.walk_speed;
    plan.period += bout.work + bout.pause + bout.step;
    plan.loop_metres += bout.step_metres;
  }
  plan.offset = unit(salt(w, 3U)) * plan.period;
  plan.start_fraction = unit(salt(w, 4U));
  plan.hatted = (salt(w, 5U) % 5U) != 0U;
  plan.face_tilt =
      (28.0F + 24.0F * unit(salt(w, 6U))) * ((salt(w, 7U) & 1U) ? 1.0F : -1.0F);
  return plan;
}

auto sample_worker(const WorkerPlan& plan, float time) -> FarmWorkerBeat {
  FarmWorkerBeat beat{};
  beat.stroke_cycle = plan.stroke_cycle;
  beat.start_fraction = plan.start_fraction;
  beat.face_tilt = plan.face_tilt;
  beat.hatted = plan.hatted;
  const float total = std::max(0.0F, time + plan.offset);
  const float loops = std::floor(total / plan.period);
  float local = total - loops * plan.period;
  beat.advance = loops * plan.loop_metres;
  for (std::size_t j = 0; j < plan.bouts.size(); ++j) {
    const auto& bout = plan.bouts[j];
    const auto& before = plan.bouts[(j + plan.bouts.size() - 1U) % plan.bouts.size()];
    if (local < bout.work) {
      beat.kind = FarmWorkerBeat::Kind::Work;
      beat.elapsed = local;
      beat.duration = bout.work;
      beat.stroke_cycle = bout.stroke_cycle;

      beat.previous_phase = fract(before.step_metres / Nap::k_walk_metres_per_cycle);
      return beat;
    }
    local -= bout.work;
    if (local < bout.pause) {
      beat.kind = FarmWorkerBeat::Kind::Pause;
      beat.elapsed = local;
      beat.duration = bout.pause;
      beat.previous_phase = 0.0F;
      return beat;
    }
    local -= bout.pause;
    if (local < bout.step) {
      beat.kind = FarmWorkerBeat::Kind::Step;
      beat.elapsed = local;
      beat.duration = bout.step;
      beat.walked = (local / bout.step) * bout.step_metres;
      beat.advance += beat.walked;
      beat.previous_phase = fract(bout.pause / k_idle_cycle_seconds);
      return beat;
    }
    local -= bout.step;
    beat.advance += bout.step_metres;
  }

  beat.kind = FarmWorkerBeat::Kind::Work;
  beat.elapsed = 0.0F;
  beat.duration = plan.bouts[0].work;
  beat.previous_phase = fract(plan.bouts[2].step_metres / Nap::k_walk_metres_per_cycle);
  return beat;
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

} // namespace

auto farm_worker_visual(bool carthage) -> const FarmWorkerVisual& {
  static const std::array<FarmWorkerVisual, 2> derived{derive_visual(false),
                                                       derive_visual(true)};
  return derived[carthage ? 1U : 0U];
}

auto farm_activity_lane(bool rows_along_x, int index) -> FarmLane {

  if (index == k_farm_headland_lane) {
    return {QVector3D(k_headland_x, k_soil_y, -0.14F),
            QVector3D(k_headland_x, k_soil_y, 0.30F),
            QVector3D(-1.0F, 0.0F, 0.0F)};
  }
  if (rows_along_x) {
    return index == 0 ? FarmLane{QVector3D(-0.70F, k_soil_y, -0.44F),
                                 QVector3D(0.40F, k_soil_y, -0.44F),
                                 QVector3D(0.0F, 0.0F, 1.0F)}
                      : FarmLane{QVector3D(-0.70F, k_soil_y, -0.02F),
                                 QVector3D(0.55F, k_soil_y, -0.02F),
                                 QVector3D(0.0F, 0.0F, -1.0F)};
  }
  return index == 0 ? FarmLane{QVector3D(-0.45F, k_soil_y, -0.40F),
                               QVector3D(-0.45F, k_soil_y, 0.26F),
                               QVector3D(1.0F, 0.0F, 0.0F)}
                    : FarmLane{QVector3D(0.05F, k_soil_y, -0.62F),
                               QVector3D(0.05F, k_soil_y, 0.26F),
                               QVector3D(-1.0F, 0.0F, 0.0F)};
}

auto farm_activity_clearing(const QVector3D& point, bool rows_along_x) -> bool {
  for (int lane = 0; lane < k_farm_headland_lane; ++lane) {
    const auto geometry = farm_activity_lane(rows_along_x, lane);
    const QVector3D span = geometry.to - geometry.from;
    const float length = span.length();
    const QVector3D direction = span / length;
    const QVector3D offset = point - geometry.from;
    const float along = offset.x() * direction.x() + offset.z() * direction.z();
    const float across = offset.x() * direction.z() - offset.z() * direction.x();
    if (along > -0.06F && along < length + 0.06F &&
        std::abs(across) < k_lane_clear_half_width + k_lane_jitter) {
      return true;
    }
  }
  return false;
}

auto farm_activity_stook_anchor() -> QVector3D {
  return {k_headland_x, k_soil_y, -0.40F};
}

auto farm_activity_stook_drop() -> QVector3D {
  return {k_headland_x, k_soil_y, -0.24F};
}

auto farm_activity_growth_stage(float growth) -> int {
  Engine::Core::FarmComponent farm;
  farm.growth = growth;
  return farm.growth_stage();
}

auto farm_activity_roster(std::uint64_t field_id) -> FarmRoster {
  const auto seed = field_seed(field_id);
  const float draw = unit(salt(seed, 41U));
  FarmRoster roster{};
  if (draw < 0.55F) {
    roster = {{0, 1, 2}, 3};
  } else if (draw < 0.90F) {

    roster =
        (salt(seed, 42U) & 1U) ? FarmRoster{{0, 1, 0}, 2} : FarmRoster{{0, 2, 0}, 2};
  } else {
    roster = {{0, 0, 0}, 1};
  }
  return roster;
}

auto farm_worker_beat(std::uint64_t field_id,
                      int worker,
                      bool ripe,
                      float time) -> FarmWorkerBeat {
  return sample_worker(worker_plan(field_id, worker, ripe), time);
}

auto farm_worker_stays_put(
    std::uint64_t field_id, int worker, bool ripe, float time, float seconds) -> bool {
  const auto plan = worker_plan(field_id, worker, ripe);
  const auto now = sample_worker(plan, time);
  const auto later = sample_worker(plan, time + seconds);

  return now.kind != FarmWorkerBeat::Kind::Step &&
         later.kind != FarmWorkerBeat::Kind::Step && now.advance == later.advance;
}

auto farm_gatherer_beat(std::uint64_t field_id,
                        float trip_metres,
                        float time) -> FarmGathererBeat {
  const auto plan = gatherer_plan(field_id);
  const float walk = std::max(trip_metres, 0.01F) / plan.walk_speed;
  const float period = plan.bundle + plan.lift + walk + plan.drop + walk;
  float local = std::fmod(std::max(0.0F, time) + plan.offset_fraction * period, period);
  FarmGathererBeat beat{};
  beat.bundle_z = plan.bundle_z;
  beat.hatted = plan.hatted;
  const std::array<std::pair<FarmGathererBeat::Kind, float>, 5> beats{{
      {FarmGathererBeat::Kind::Bundle, plan.bundle},
      {FarmGathererBeat::Kind::Lift, plan.lift},
      {FarmGathererBeat::Kind::Carry, walk},
      {FarmGathererBeat::Kind::Drop, plan.drop},
      {FarmGathererBeat::Kind::Return, walk},
  }};
  for (const auto& [kind, duration] : beats) {
    if (local < duration) {
      beat.kind = kind;
      beat.elapsed = local;
      beat.duration = duration;
      if (kind == FarmGathererBeat::Kind::Carry ||
          kind == FarmGathererBeat::Kind::Return) {
        beat.walked = (local / duration) * trip_metres;
      }
      return beat;
    }
    local -= duration;
  }
  beat.kind = FarmGathererBeat::Kind::Bundle;
  beat.duration = plan.bundle;
  return beat;
}

void FarmActivity::begin_frame(Engine::Core::World* snapshot,
                               float time,
                               Engine::Core::World* identity) {
  auto* next_world = identity != nullptr ? identity : snapshot;
  if (world != next_world || time < frame_time || !gag_seen ||
      time - gag_started >= 9.0F) {
    gag_field = 0;
  }
  previous_time = world == next_world && time >= frame_time ? frame_time : time;
  frame_time = time;
  world = next_world;
  gag_seen = false;
  const auto quality = GraphicsSettings::instance().quality();
  workers_per_field = quality == GraphicsQuality::Low      ? 0
                      : quality == GraphicsQuality::Medium ? 1
                                                           : 3;
  remaining = quality == GraphicsQuality::Ultra  ? 96
              : quality == GraphicsQuality::High ? 64
                                                 : 24;
  fields.clear();
  if (snapshot == nullptr || workers_per_field == 0) {
    return;
  }

  claimed.clear();
  for (auto [id, builder] :
       snapshot->view<Engine::Core::BuilderProductionComponent>()) {
    (void)id;
    if (builder.structure_task_entity_id != 0) {
      claimed.push_back(builder.structure_task_entity_id);
    }
  }
  std::sort(claimed.begin(), claimed.end());
  for (auto [id, farm, unit] :
       snapshot->view<Engine::Core::FarmComponent, Engine::Core::UnitComponent>()) {
    if (farm.growth <= 0 || unit.health <= 0 ||
        get_building_state(static_cast<float>(unit.health) /
                           static_cast<float>(std::max(1, unit.max_health))) ==
            BuildingState::Destroyed ||
        Game::Core::is_neutral_owner(unit.owner_id) ||
        std::binary_search(claimed.begin(), claimed.end(), id) ||
        snapshot->has<Engine::Core::PendingRemovalComponent>(id) ||
        snapshot->has<Engine::Core::DeathAnimationComponent>(id) ||
        snapshot->has<Engine::Core::DismantleSiteComponent>(id) ||
        snapshot->has<Engine::Core::ConstructionPreviewComponent>(id)) {
      continue;
    }
    fields.push_back(Field{id, farm.growth, farm.harvests});
  }
  std::sort(fields.begin(), fields.end(), [](const Field& a, const Field& b) {
    return a.id < b.id;
  });
}

auto FarmActivity::field(std::uint64_t id) const -> const Field* {
  const auto it = std::lower_bound(
      fields.begin(), fields.end(), id, [](const Field& f, std::uint64_t key) {
        return f.id < key;
      });
  return it != fields.end() && it->id == id ? &*it : nullptr;
}

auto FarmActivity::gag(std::uint64_t id,
                       int stage,
                       float time,
                       bool may_start) -> float {
  if (gag_field == id && gag_stage != stage) {
    gag_field = 0;
  }
  if (gag_field == 0 && workers_per_field >= 2) {

    const float interval = std::max(330.0F, cooldown_seconds * 0.6F);

    const float jitter = std::max(0.0F, interval - 305.0F);
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / interval)));
    const auto seed = mix(static_cast<std::uint32_t>(id) ^
                          static_cast<std::uint32_t>(id >> 32U) ^ mix(cycle));
    const float start = (static_cast<float>(cycle) * interval) +
                        ((static_cast<float>(seed % 1000U) / 1000.0F) * jitter);
    if (may_start && (seed & 3U) == 0 && previous_time < start && time >= start &&
        time - start < 0.5F) {
      gag_field = id;
      gag_stage = stage;
      gag_started = time;
    }
  }
  if (gag_field != id) {
    return -1.0F;
  }
  gag_seen = true;
  return time - gag_started;
}

} // namespace Render::GL
