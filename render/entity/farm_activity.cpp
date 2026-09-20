#include "farm_activity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

#include "animation/clip_manifest.h"
#include "building_state.h"
#include "farm_worker_props.h"
#include "game/core/component_economy.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "registry.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/graphics_settings.h"

namespace Render::GL {
namespace {
constexpr float k_tend_cycle_seconds = 2.6F;
constexpr float k_reap_cycle_seconds = 2.2F;

auto mix(std::uint32_t x) -> std::uint32_t {
  x ^= x >> 16U;
  x *= 0x7feb352dU;
  x ^= x >> 15U;
  x *= 0x846ca68bU;
  return x ^ (x >> 16U);
}

// 0 at its own anchor, 1 beside the sleeper. Out and back around the stare.
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
  return std::atan2(direction.x(), direction.z()) * 180.0F / 3.14159265F;
}

auto visuals() -> std::array<FarmWorkerVisual, 2>& {
  static std::array<FarmWorkerVisual, 2> registered{};
  return registered;
}
} // namespace

void register_farm_worker_visual(bool carthage, FarmWorkerVisual visual) {
  // Derive the hatted/laden variants once, at renderer registration, so no
  // archetype is built inside the frame loop.
  const auto& props = farm_worker_props();
  const std::string_view nation = carthage ? "carthage" : "roman";
  const std::array<EquipmentHandle, 1> hat{props.sun_hat};
  const std::array<EquipmentHandle, 2> hat_and_sheaf{props.sun_hat, props.wheat_sheaf};
  const std::array<EquipmentHandle, 1> tilted{props.tilted_sun_hat};
  visual.hatted_tending = resolve_humanoid_equipment_archetype(
      std::string("farm/worker/").append(nation).append("/tend"), visual.tending, hat);
  visual.hatted_reaping = resolve_humanoid_equipment_archetype(
      std::string("farm/worker/").append(nation).append("/reap"), visual.reaping, hat);
  visual.hatted_gathering = resolve_humanoid_equipment_archetype(
      std::string("farm/worker/").append(nation).append("/gather"),
      visual.tending,
      hat_and_sheaf);
  visual.napping = resolve_humanoid_equipment_archetype(
      std::string("farm/worker/").append(nation).append("/nap"),
      visual.reaping,
      tilted);
  visuals()[carthage ? 1U : 0U] = std::move(visual);
}

auto farm_worker_visual(bool carthage) -> const FarmWorkerVisual& {
  return visuals()[carthage ? 1U : 0U];
}

auto farm_activity_anchor(int index) -> QVector3D {
  return {-0.64F + static_cast<float>(index) * 0.43F,
          0.035F,
          -0.62F + static_cast<float>(index) * 0.25F};
}

auto farm_activity_growth_stage(float growth) -> int {
  Engine::Core::FarmComponent farm;
  farm.growth = growth;
  return farm.growth_stage();
}

auto farm_activity_clearing(const QVector3D& point) -> bool {
  for (int i = 0; i < 3; ++i) {
    const auto center = farm_activity_anchor(i) + QVector3D(0, 0, 0.025F);
    const float radius = i == 0 ? 0.22F : 0.17F;
    const float dx = point.x() - center.x();
    const float dz = point.z() - center.z();
    if (dx * dx + dz * dz < radius * radius) {
      return true;
    }
  }
  return false;
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
  // Two scans per frame, not a component lookup per field per frame. A real
  // economic worker assigned to a field owns that field's visuals, so the
  // decorative actors stand down for it. The task target is on the snapshot;
  // render must not ask a gameplay system what the task means.
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
    fields.push_back(Field{id, farm.growth});
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

// Review hook. The sequence is meant to be rare, which makes it impossible to
// capture: SOI_FARM_GAG_SECONDS shortens the window to that many seconds and
// makes every field eligible, so an Arena run can show it.
auto gag_review_interval() -> float {
  static const float seconds = []() {
    const char* value = std::getenv("SOI_FARM_GAG_SECONDS");
    if (value == nullptr) {
      return 0.0F;
    }
    return std::max(0.0F, static_cast<float>(std::atof(value)));
  }();
  return seconds;
}

auto FarmActivity::gag(std::uint64_t id, int stage, float time) -> float {
  if (gag_field == id && gag_stage != stage) {
    gag_field = 0;
  }
  const float review = gag_review_interval();
  if (gag_field == 0 && workers_per_field >= 2 && review > 0.0F) {
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / review)));
    const float start = static_cast<float>(cycle) * review;
    if (previous_time < start && time >= start) {
      gag_field = id;
      gag_stage = stage;
      gag_started = time;
    }
  } else if (gag_field == 0 && workers_per_field >= 2) {
    const float interval = std::max(300.0F, cooldown_seconds) + 300.0F;
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / interval)));
    const auto seed = mix(static_cast<std::uint32_t>(id) ^
                          static_cast<std::uint32_t>(id >> 32U) ^ mix(cycle));
    const float start =
        static_cast<float>(cycle) * interval + 300.0F + static_cast<float>(seed % 290U);
    if ((seed & 3U) == 0 && previous_time < start && time >= start &&
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

void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::CreatureLOD;
  namespace Pipeline = Render::Creature::Pipeline;

  auto* activity = ctx.farm_activity;
  if (activity == nullptr || activity->remaining == 0 ||
      activity->workers_per_field == 0 || ctx.entity == nullptr ||
      ctx.template_prewarm) {
    return;
  }
  const auto& visual = farm_worker_visual(carthage);
  if (!visual.valid()) {
    return;
  }
  // Eligibility -- growth, health, ownership, removal, dismantling and real
  // harvest tasks -- was resolved for every field in begin_frame().
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
  const int count = std::min({activity->workers_per_field,
                              activity->remaining,
                              pixels >= 0 && pixels < 15 ? 1 : 3});
  // Actors are people, not field decoration: the field's own scale never
  // reaches their world matrix, only its placement and row orientation do.
  std::array<QMatrix4x4, 3> models;
  std::array<bool, 3> visible{};
  const auto seed = mix(static_cast<std::uint32_t>(field->id) ^
                        static_cast<std::uint32_t>(field->id >> 32U));
  for (int i = 0; i < count; ++i) {
    // Keep each worker in the crop rectangle, clear of the shed, scarecrow and
    // perimeter. Small jitter stays between crop rows.
    const float jitter = static_cast<float>(mix(seed + i) % 100U) * 0.0005F;
    const QVector3D local = farm_activity_anchor(i) + QVector3D(0, 0, jitter);
    auto placement = ctx.model;
    placement.translate(local);
    auto anchor = placement.column(3).toVector3D();
    if (ctx.world_view.has_terrain()) {
      // Never sink below the field's own soil surface on graded foundations.
      anchor.setY(std::max(anchor.y(),
                           ctx.world_view.terrain()->resolve_surface_world_y(
                               anchor.x(), anchor.z(), 0.035F * scale, anchor.y())));
    }
    const auto* visibility = ctx.submission_visibility != nullptr
                                 ? ctx.submission_visibility->snapshot()
                                 : nullptr;
    const auto fog_mode = ctx.submission_fog_mode == SubmissionFogMode::Ignore
                              ? SubmissionFogMode::Ignore
                              : SubmissionFogMode::VisibleOnly;
    visible[i] =
        ctx.submission_visibility != nullptr &&
        (fog_mode == SubmissionFogMode::Ignore || visibility == nullptr ||
         !visibility->initialized ||
         visibility->is_visible_world(anchor.x(), anchor.z())) &&
        ctx.submission_visibility
            ->evaluate_sphere_with_margin(anchor, 1.2F, fog_mode, FogExtent::Anchor)
            .accepted() &&
        !ctx.submission_visibility->occludes_lens_gap(anchor);
    models[i] = QMatrix4x4{};
    models[i].translate(anchor);
    models[i].rotate((carthage ? 0.0F : 90.0F) + ((seed >> i) & 1U) * 180.0F, 0, 1, 0);
  }
  const float gag_time = count >= 2 && visible[0] && visible[1]
                             ? activity->gag(field->id,
                                             farm_activity_growth_stage(field->growth),
                                             ctx.animation_time)
                             : -1.0F;

  static thread_local Pipeline::CreatureRenderBatch batch;
  static thread_local Pipeline::CreaturePipeline pipeline;
  batch.clear();
  const Render::GL::HumanoidPose pose{};
  const Render::GL::HumanoidAnimationContext anim{};
  const bool ripe = field->growth >= 1.0F;
  const bool gag_running = gag_time >= 0 && gag_time < Nap::k_sequence_end;
  for (int i = 0; i < count; ++i) {
    if (!visible[i]) {
      continue;
    }
    auto archetype = ripe ? visual.hatted_reaping : visual.hatted_tending;
    std::uint16_t clip = ripe ? Animation::k_humanoid_construct_reap_clip
                              : Animation::k_humanoid_construct_chisel_clip;
    float cycle = ripe ? k_reap_cycle_seconds : k_tend_cycle_seconds;
    if (i == 2) {
      // The third hand bundles what the others cut, sheaf under the arm.
      archetype = visual.hatted_gathering;
      clip = ripe ? Animation::k_humanoid_resource_carry_clip
                  : Animation::k_humanoid_construct_kneel_chisel_clip;
      cycle = ripe ? 3.4F : 3.0F;
    }
    float phase = ctx.animation_time / cycle +
                  static_cast<float>(mix(seed + (i * 37U)) % 1000U) / 1000.0F;
    bool held = false;

    if (gag_running && i == 0) {
      // The sleeper: sit down, hat over the eyes, jolt up, then work far too
      // hard for a short while.
      if (gag_time < Nap::k_sit_down_end) {
        clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
        phase = gag_time / Nap::k_sit_down_end;
        held = true;
      } else if (gag_time < Nap::k_asleep_end) {
        archetype = visual.napping;
        clip = Animation::k_humanoid_showcase_rest_sit_clip;
        phase = 0.0F;
        held = true;
      } else if (gag_time < Nap::k_get_up_end) {
        // Standing up is the sit-down beat run backwards.
        clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
        phase = 1.0F - ((gag_time - Nap::k_asleep_end) /
                        (Nap::k_get_up_end - Nap::k_asleep_end));
        held = true;
      } else if (gag_time < Nap::k_pretend_end) {
        phase = ctx.animation_time / (cycle * 0.45F);
      }
    }

    if (gag_running && i == 1) {
      // The coworker walks over to look, holds the stare, then walks back.
      const float travel = walk_fraction(gag_time);
      if (travel > 0.0F) {
        const auto sleeper = models[0].column(3).toVector3D();
        const auto home = models[1].column(3).toVector3D();
        const auto stop = sleeper + ((home - sleeper).normalized() * 1.6F);
        const auto here = home + ((stop - home) * travel);
        const bool moving = walk_is_moving(gag_time);
        models[i] = QMatrix4x4{};
        models[i].translate(here);
        models[i].rotate(yaw_towards(here, moving ? stop : sleeper, home), 0, 1, 0);
        if (moving) {
          clip = Animation::k_humanoid_walk_clip;
          // Phase follows distance covered, not wall-clock, so feet do not skate.
          phase = (home - stop).length() * travel / Nap::k_walk_metres_per_cycle;
        } else {
          clip = Animation::k_humanoid_idle_clip;
          cycle = 3.6F;
          phase = ctx.animation_time / cycle;
        }
      }
    }

    Pipeline::CreatureGraphOutput output{};
    // Minimal resolves a per-state snapshot mesh rather than the requested
    // clip, so it is only used where a worker is a speck and the pose cannot
    // be read anyway.
    output.lod = pixels >= 0 && pixels < 12 ? CreatureLOD::Minimal : CreatureLOD::Full;
    output.pass_intent = Pipeline::RenderPassIntent::Main;
    output.seed = mix(seed + (i * 977U));
    output.world_matrix = models[i];
    output.world_already_grounded = true;
    output.entity_id = static_cast<Pipeline::EntityId>(field->id);
    output.instance_index = static_cast<std::uint16_t>(i + 1);
    output.spec = visual.spec;
    Pipeline::HumanoidAnimationSelection selection{};
    selection.requested_archetype = archetype;
    selection.resolved_archetype = archetype;
    selection.state = AnimationStateId::Idle;
    selection.phase = held ? std::clamp(phase, 0.0F, 1.0F) : phase - std::floor(phase);
    selection.clip_id = clip;
    output.humanoid_selection = selection;
    Render::GL::HumanoidVariant variant{};
    visual.fill_variant(ctx, output.seed, variant);
    batch.add_humanoid(output, pose, variant, anim);
    --activity->remaining;
  }
  if (!batch.empty()) {
    (void)pipeline.submit_requests(batch.requests(), out);
  }
}
} // namespace Render::GL
