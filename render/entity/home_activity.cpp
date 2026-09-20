#include "home_activity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "animation/clip_manifest.h"
#include "building_state.h"
#include "civilian_actor.h"
#include "game/core/component_gameplay.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "home_props.h"
#include "registry.h"
#include "render/creature/archetype_registry.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/graphics_settings.h"

namespace Render::GL {
namespace {
constexpr auto mix = ambient_hash;

// A hearth is lit for a stretch and then out, so a street is never uniformly
// smoking and never uniformly bare.
constexpr float k_hearth_period = 190.0F;
constexpr float k_hearth_duty = 0.38F;
constexpr float k_hearth_ramp = 9.0F;

auto anchors() -> std::array<HomeSmokeAnchor, 2>& {
  static std::array<HomeSmokeAnchor, 2> registered{};
  return registered;
}

auto derive_bowl(bool carthage) -> Render::Creature::ArchetypeId {
  const auto& rig = nation_civilian_rig(carthage);
  if (!rig.valid()) {
    return Render::Creature::k_invalid_archetype;
  }
  const std::array<EquipmentHandle, 1> bowl{home_props().soup_bowl};
  return resolve_humanoid_equipment_archetype(
      std::string("home/resident/") + (carthage ? "carthage" : "roman") + "/bowl",
      rig.idle,
      bowl);
}
} // namespace

void register_home_smoke_anchor(bool carthage, HomeSmokeAnchor anchor) {
  anchors()[carthage ? 1U : 0U] = anchor;
}

auto home_smoke_anchor(bool carthage) -> const HomeSmokeAnchor& {
  return anchors()[carthage ? 1U : 0U];
}

auto home_bowl_archetype(bool carthage) -> Render::Creature::ArchetypeId {
  static const std::array<Render::Creature::ArchetypeId, 2> derived{derive_bowl(false),
                                                                    derive_bowl(true)};
  return derived[carthage ? 1U : 0U];
}

void HomeActivity::begin_frame(Engine::Core::World* snapshot,
                               float time,
                               Engine::Core::World* identity) {
  auto* next_world = identity != nullptr ? identity : snapshot;
  if (world != next_world || time < frame_time || !gag_seen ||
      time - gag_started >= Spill::k_sequence_end) {
    gag_home = 0;
  }
  previous_time = world == next_world && time >= frame_time ? frame_time : time;
  frame_time = time;
  world = next_world;
  gag_seen = false;

  const auto quality = GraphicsSettings::instance().quality();
  max_plumes = quality == GraphicsQuality::Low      ? 0
               : quality == GraphicsQuality::Medium ? 8
               : quality == GraphicsQuality::High   ? 20
                                                    : 32;
  // The gag is a full rigged actor, so it follows the creature budget rather
  // than the cheap plume budget.
  actors_allowed =
      quality != GraphicsQuality::Low && quality != GraphicsQuality::Medium;
  remaining_plumes = max_plumes;
  homes.clear();
  if (snapshot == nullptr || max_plumes == 0) {
    return;
  }
  // One scan per frame over the locked snapshot, never the live simulation.
  for (auto [id, unit] : snapshot->view<Engine::Core::UnitComponent>()) {
    if (unit.spawn_type != Game::Units::SpawnType::Home || unit.health <= 0 ||
        get_building_state(static_cast<float>(unit.health) /
                           static_cast<float>(std::max(1, unit.max_health))) ==
            BuildingState::Destroyed ||
        Game::Core::is_neutral_owner(unit.owner_id) ||
        snapshot->has<Engine::Core::PendingRemovalComponent>(id) ||
        snapshot->has<Engine::Core::DeathAnimationComponent>(id) ||
        snapshot->has<Engine::Core::DismantleSiteComponent>(id) ||
        snapshot->has<Engine::Core::ConstructionPreviewComponent>(id)) {
      continue;
    }
    homes.push_back(id);
  }
  std::sort(homes.begin(), homes.end());
}

auto HomeActivity::inhabited(std::uint64_t id) const -> bool {
  return std::binary_search(homes.begin(), homes.end(), id);
}

auto HomeActivity::hearth_intensity(std::uint64_t id, float time) const -> float {
  // Review hook: a lit subset is the point, which makes both anchors hard to
  // inspect in one capture. This lights every eligible house.
  static const bool always = ambient_review_interval("SOI_HOME_SMOKE_ALWAYS") > 0.0F;
  if (always) {
    return 0.85F;
  }
  const auto seed =
      mix(static_cast<std::uint32_t>(id) ^ static_cast<std::uint32_t>(id >> 32U));
  // A per-house offset and a per-house period stretch: neighbours never share
  // a cycle, so a street has no visible rhythm.
  const float offset = static_cast<float>(seed % 1000U) / 1000.0F * k_hearth_period;
  const float period =
      k_hearth_period * (0.78F + (static_cast<float>((seed >> 11U) % 500U) / 1000.0F));
  const float duty =
      k_hearth_duty * (0.72F + (static_cast<float>((seed >> 21U) % 600U) / 1000.0F));
  const float lit_seconds = period * duty;
  float phase = std::fmod(time + offset, period);
  if (phase < 0.0F) {
    phase += period;
  }
  if (phase >= lit_seconds) {
    return 0.0F;
  }
  // Smooth start and stop: a hearth is banked up and dies down.
  const float ramp = std::min(k_hearth_ramp, lit_seconds * 0.4F);
  const float rise = std::clamp(phase / ramp, 0.0F, 1.0F);
  const float fall = std::clamp((lit_seconds - phase) / ramp, 0.0F, 1.0F);
  const float strength =
      0.55F + (static_cast<float>((seed >> 5U) % 450U) / 1000.0F); // 0.55 - 1.0
  return rise * fall * strength;
}

auto HomeActivity::gag(std::uint64_t id, float time) -> float {
  const float review = ambient_review_interval("SOI_HOME_GAG_SECONDS");
  if (gag_home == 0 && actors_allowed && review > 0.0F) {
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / review)));
    const float start = static_cast<float>(cycle) * review;
    if (previous_time < start && time >= start) {
      gag_home = id;
      gag_started = time;
    }
  } else if (gag_home == 0 && actors_allowed) {
    const float interval = std::max(300.0F, cooldown_seconds) + 300.0F;
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / interval)));
    const auto seed = mix(static_cast<std::uint32_t>(id) ^
                          static_cast<std::uint32_t>(id >> 32U) ^ mix(cycle));
    const float start =
        static_cast<float>(cycle) * interval + 240.0F + static_cast<float>(seed % 300U);
    if ((seed & 3U) == 0 && previous_time < start && time >= start &&
        time - start < 0.5F) {
      gag_home = id;
      gag_started = time;
    }
  }
  if (gag_home != id) {
    return -1.0F;
  }
  gag_seen = true;
  return time - gag_started;
}

namespace {
// Distance walked from the doorstep at each beat: out, sprawled, then back.
auto spill_walk_metres(float t) -> float {
  if (t < Spill::k_emerge_end) {
    return Spill::k_walk_out_metres * (t / Spill::k_emerge_end);
  }
  if (t < Spill::k_return_end) {
    return Spill::k_walk_out_metres;
  }
  const float back =
      (t - Spill::k_return_end) / (Spill::k_sequence_end - Spill::k_return_end);
  return Spill::k_walk_out_metres * (1.0F - std::clamp(back, 0.0F, 1.0F));
}
} // namespace

void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage) {
  auto* activity = ctx.home_activity;
  if (activity == nullptr || ctx.entity == nullptr || ctx.template_prewarm ||
      activity->max_plumes == 0) {
    return;
  }
  const auto id = ctx.entity->get_id();
  if (!activity->inhabited(id)) {
    return;
  }
  const auto& anchor = home_smoke_anchor(carthage);
  if (!anchor.valid()) {
    return;
  }
  const float scale = ctx.model.column(1).toVector3D().length();
  const float pixels = ctx.screen_metrics.projected_radius_px_from_distance_sq(
      ctx.distance_sq, scale * 0.5F);
  if ((pixels >= 0 && pixels < 9) || ctx.distance_sq > 190.0F * 190.0F) {
    return;
  }

  auto to_world = [&](const QVector3D& local) {
    auto placed = ctx.model;
    placed.translate(local);
    return placed.column(3).toVector3D();
  };
  auto visible = [&](const QVector3D& point, float radius) {
    if (ctx.submission_visibility == nullptr) {
      return false;
    }
    const auto* fog = ctx.submission_visibility->snapshot();
    const auto mode = ctx.submission_fog_mode == SubmissionFogMode::Ignore
                          ? SubmissionFogMode::Ignore
                          : SubmissionFogMode::VisibleOnly;
    if (mode != SubmissionFogMode::Ignore && fog != nullptr && fog->initialized &&
        !fog->is_visible_world(point.x(), point.z())) {
      return false;
    }
    return ctx.submission_visibility
               ->evaluate_sphere_with_margin(point, radius, mode, FogExtent::Anchor)
               .accepted() &&
           !ctx.submission_visibility->occludes_lens_gap(point);
  };

  const float intensity = activity->hearth_intensity(id, ctx.animation_time);
  if (intensity > 0.0F && activity->remaining_plumes > 0) {
    const auto vent = to_world(anchor.vent);
    if (visible(vent, anchor.plume_radius * scale)) {
      // The plume is authored in house-local units, so it scales with the
      // model rather than towering over a small house.
      out.hearth_smoke(vent,
                       anchor.smoke_tint,
                       anchor.plume_radius * scale,
                       intensity,
                       ctx.animation_time);
      --activity->remaining_plumes;
    }
  }

  if (!activity->actors_allowed || (pixels >= 0 && pixels < 22)) {
    return;
  }
  const auto& rig = nation_civilian_rig(carthage);
  const auto bowl = home_bowl_archetype(carthage);
  if (!rig.valid() || bowl == Render::Creature::k_invalid_archetype) {
    return;
  }
  const auto doorstep = to_world(anchor.doorstep);
  if (!visible(doorstep, 1.4F)) {
    return;
  }
  const float t = activity->gag(id, ctx.animation_time);
  if (t < 0.0F || t >= Spill::k_sequence_end) {
    return;
  }

  // The path runs straight out from the door, on the house's own facing, so it
  // never crosses the building and never needs the navigation grid.
  auto outward = ctx.model.mapVector(anchor.outward);
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-4F) {
    outward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  outward.normalize();
  const float walked = spill_walk_metres(t);
  auto position = doorstep + (outward * walked);
  if (ctx.world_view.has_terrain()) {
    position.setY(std::max(position.y(),
                           ctx.world_view.terrain()->resolve_surface_world_y(
                               position.x(), position.z(), 0.3F, position.y())));
  }

  const float yaw = std::atan2(outward.x(), outward.z()) * 180.0F / 3.14159265F;
  const bool returning = t >= Spill::k_return_end;
  QMatrix4x4 world;
  world.translate(position);
  world.rotate(returning ? yaw + 180.0F : yaw, 0, 1, 0);

  auto archetype = bowl;
  std::uint16_t clip = Animation::k_humanoid_walk_clip;
  float phase = walked / Spill::k_walk_metres_per_cycle;
  bool held = false;
  if (t < Spill::k_emerge_end) {
    // Out through the door, bowl in hand.
  } else if (t < Spill::k_trip_end) {
    // The trip: a stagger, bowl still held, just before it goes.
    clip = Animation::k_humanoid_react_stagger_clip;
    phase = (t - Spill::k_emerge_end) / (Spill::k_trip_end - Spill::k_emerge_end);
    held = true;
  } else if (t < Spill::k_sprawl_end) {
    // Flat out. The bowl is gone from the hand and the broth is on the ground.
    archetype = rig.idle;
    clip = Animation::k_humanoid_dead_infantry_face_clip;
    phase = 0.0F;
    held = true;
  } else if (t < Spill::k_rise_end) {
    // Getting up is the fall played backwards.
    archetype = rig.idle;
    clip = Animation::k_humanoid_die_infantry_face_clip;
    phase =
        1.0F - ((t - Spill::k_sprawl_end) / (Spill::k_rise_end - Spill::k_sprawl_end));
    held = true;
  } else if (t < Spill::k_stare_end) {
    // Standing over the mess, empty-handed.
    archetype = rig.idle;
    clip = Animation::k_humanoid_idle_clip;
    phase = (t - Spill::k_rise_end) * 0.4F;
  } else if (t < Spill::k_return_end) {
    archetype = rig.idle;
    clip = Animation::k_humanoid_idle_clip;
    phase = (t - Spill::k_rise_end) * 0.4F;
  } else {
    // Back inside, empty bowl left behind.
    archetype = rig.idle;
  }

  begin_civilian_actors();
  add_civilian_actor(
      ctx,
      rig,
      CivilianActor{
          .archetype = archetype,
          .clip = clip,
          .phase = held ? std::clamp(phase, 0.0F, 1.0F) : phase - std::floor(phase),
          .world = world,
          .owner_id = static_cast<std::uint32_t>(id),
          .instance = 1U,
          .seed = mix(static_cast<std::uint32_t>(id) * 2654435761U),
          .distant = false,
      });
  submit_civilian_actors(out);

  if (t >= Spill::k_trip_end) {
    // The spill itself, fading as the broth soaks away.
    // Just beyond where they landed, so they stand beside the mess rather
    // than in it.
    auto puddle = doorstep + (outward * (Spill::k_walk_out_metres + 0.55F));
    puddle.setY(position.y() + 0.01F);
    QMatrix4x4 spill;
    spill.translate(puddle);
    spill.rotate(yaw, 0, 1, 0);
    const float fade =
        1.0F - std::clamp((t - Spill::k_trip_end) /
                              (Spill::k_sequence_end - Spill::k_trip_end),
                          0.0F,
                          1.0F);
    submit_render_instance(out,
                           RenderInstance{.archetype = &soup_puddle_archetype(),
                                          .world = spill,
                                          .alpha_multiplier = 0.30F + 0.55F * fade});
  }
}

} // namespace Render::GL
