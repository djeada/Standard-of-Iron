#include "home_activity.h"

#include <QQuaternion>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
constexpr float k_pi = 3.14159265F;
constexpr float k_tau = 6.2831853F;

constexpr float k_hearth_period = 190.0F;
constexpr float k_hearth_duty = 0.46F;
constexpr float k_hearth_ramp = 9.0F;

auto anchors() -> std::array<HomeSmokeAnchor, 2>& {
  static std::array<HomeSmokeAnchor, 2> registered{};
  return registered;
}

auto low32(std::uint64_t id) -> std::uint32_t {
  return static_cast<std::uint32_t>(id) ^ static_cast<std::uint32_t>(id >> 32U);
}

auto roll(std::uint32_t seed, std::uint32_t salt) -> float {
  return static_cast<float>(mix(seed ^ (salt * 0x9e3779b9U)) & 0xffffU) / 65536.0F;
}

auto smoothstep(float edge0, float edge1, float x) -> float {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5F), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto wrap(float value, float period) -> float {
  float phase = std::fmod(value, period);
  if (phase < 0.0F) {
    phase += period;
  }
  return phase;
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

auto hearth_character(std::uint64_t id) -> HearthCharacter {
  const auto seed = mix(low32(id) ^ 0x3c6ef372U);
  HearthCharacter character;
  character.size = 0.80F + (0.45F * roll(seed, 1U));
  character.density = 0.62F + (0.38F * roll(seed, 2U));
  character.warmth = roll(seed, 3U);
  character.speed = 0.82F + (0.36F * roll(seed, 4U));
  character.time_offset = 600.0F * roll(seed, 5U);

  const float tended = roll(seed, 6U);
  character.puff_depth = tended < 0.55F ? 0.0F : 0.12F + (0.22F * roll(seed, 7U));
  character.puff_rate = 0.05F + (0.09F * roll(seed, 8U));
  character.puff_phase = k_tau * roll(seed, 9U);
  return character;
}

auto DoorstepVisit::step_seconds() const noexcept -> float {
  return out_metres / Doorstep::k_walk_speed;
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

  actors_allowed =
      quality != GraphicsQuality::Low && quality != GraphicsQuality::Medium;
  remaining_plumes = max_plumes;
  remaining_actors = actors_allowed ? (max_plumes >= 32 ? 10 : 6) : 0;
  remaining_ambient_actors = actors_allowed ? (max_plumes >= 32 ? 28 : 16) : 0;
  homes.clear();
  if (snapshot == nullptr || max_plumes == 0) {
    return;
  }

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

auto HomeActivity::meal_bias() const noexcept -> float {

  const float n = night_factor;
  const float twilight = 4.0F * n * (1.0F - n);

  return 1.00F + (0.55F * twilight) - (0.30F * n);
}

auto HomeActivity::hearth_intensity(std::uint64_t id, float time) const -> float {

  const auto seed = mix(low32(id));

  const float offset = static_cast<float>(seed % 1000U) / 1000.0F * k_hearth_period;
  const float period =
      k_hearth_period * (0.78F + (static_cast<float>((seed >> 11U) % 500U) / 1000.0F));
  const float duty =
      k_hearth_duty * (0.72F + (static_cast<float>((seed >> 21U) % 600U) / 1000.0F));
  const float lit_seconds = std::min(period * duty * meal_bias(), period * 0.92F);
  const float phase = wrap(time + offset, period);
  if (phase >= lit_seconds) {
    return 0.0F;
  }

  const float ramp = std::min(k_hearth_ramp, lit_seconds * 0.4F);
  const float rise = std::clamp(phase / ramp, 0.0F, 1.0F);
  const float fall = std::clamp((lit_seconds - phase) / ramp, 0.0F, 1.0F);
  const float strength = 0.55F + (static_cast<float>((seed >> 5U) % 450U) / 1000.0F);
  return rise * fall * strength;
}

auto HomeActivity::lamp_state(std::uint64_t id, float time) const -> LampState {
  LampState state;
  const auto seed = mix(low32(id) ^ 0x9e3779b9U);

  if (roll(seed, 1U) < 0.22F) {
    return state;
  }

  const float threshold = 0.10F + (0.42F * roll(seed, 2U));
  const float ease = smoothstep(threshold, threshold + 0.24F, night_factor);
  if (ease <= 0.0F) {
    return state;
  }

  const float period = 240.0F + (220.0F * roll(seed, 3U));
  const float offset = period * roll(seed, 4U);
  const float lit_seconds = period * (0.70F + (0.22F * roll(seed, 5U)));
  const float phase = wrap(time + offset, period);
  if (phase >= lit_seconds) {
    return state;
  }
  constexpr float k_ramp = 6.0F;
  const float schedule = std::clamp(phase / k_ramp, 0.0F, 1.0F) *
                         std::clamp((lit_seconds - phase) / k_ramp, 0.0F, 1.0F);

  const float rate = 1.6F + (2.2F * roll(seed, 6U));
  const float depth = 0.05F + (0.12F * roll(seed, 7U));
  const float flicker_phase = k_tau * roll(seed, 8U);
  const float quick = 0.5F + (0.5F * std::sin((time * rate * k_tau) + flicker_phase));
  const float slow =
      0.7F + (0.3F * std::sin((time * rate * 0.37F * k_tau) + (flicker_phase * 1.7F)));
  const float flicker = (1.0F - depth) + (depth * quick * slow);
  state.strength = ease * schedule * flicker;

  const float amber = roll(seed, 9U);
  state.color = QVector3D(1.0F, 0.74F - (0.16F * amber), 0.40F - (0.18F * amber));
  return state;
}

auto HomeActivity::doorstep_plan(std::uint64_t id, float time) const -> DoorstepVisit {
  DoorstepVisit visit;
  const auto seed = mix(low32(id) ^ 0x51ed270bU);

  const float period = Doorstep::k_period * (0.72F + (0.70F * roll(seed, 1U)));
  const float offset = period * roll(seed, 2U);
  const float shifted = std::max(0.0F, time + offset);
  const auto cycle = static_cast<std::uint32_t>(std::floor(shifted / period));
  const float phase = shifted - (static_cast<float>(cycle) * period);
  const auto turn = mix(seed ^ mix(cycle * 0x27d4eb2fU));
  if (roll(turn, 1U) >= 0.58F) {
    return visit;
  }
  visit.activity = static_cast<DoorstepActivity>(
      static_cast<int>(roll(turn, 2U) * static_cast<float>(DoorstepActivity::Count)) %
      static_cast<int>(DoorstepActivity::Count));
  visit.residents = visit.activity == DoorstepActivity::Talk ? 2 : 1;
  visit.out_metres = 1.1F + (0.9F * roll(turn, 3U));
  visit.lateral = (roll(turn, 4U) - 0.5F) * 1.2F;
  visit.duration = Doorstep::k_visit_min +
                   ((Doorstep::k_visit_max - Doorstep::k_visit_min) * roll(turn, 5U));
  if (visit.activity == DoorstepActivity::Errand) {
    visit.errand_metres = 1.8F + (1.6F * roll(turn, 6U));
    visit.lateral *= 0.4F;
    visit.duration =
        std::max(visit.duration,
                 (2.0F * visit.step_seconds()) +
                     (2.0F * visit.errand_metres / Doorstep::k_walk_speed) + 5.0F);
  } else if (visit.activity == DoorstepActivity::Scrub) {
    visit.out_metres = 0.75F;
    visit.lateral *= 0.5F;
  } else if (visit.activity == DoorstepActivity::Sit) {
    visit.out_metres = std::min(visit.out_metres, 1.0F);
    visit.duration = std::max(visit.duration, 16.0F);
  }
  visit.duration = std::min(visit.duration, period * 0.45F);
  if (phase >= visit.duration) {
    return visit;
  }
  visit.t = phase;
  return visit;
}

auto HomeActivity::doorstep_visit(std::uint64_t id, float time) const -> float {
  return doorstep_plan(id, time).t;
}

auto HomeActivity::curtain_parting(std::uint64_t id, float time) const -> float {
  const auto visit = doorstep_plan(id, time);
  if (!visit.active()) {
    return 0.0F;
  }

  auto bump = [](float since) {
    const float u = (since - 0.15F) / 0.5F;
    return std::max(0.0F, 1.0F - (u * u));
  };
  float part = 0.0F;
  for (int r = 0; r < visit.residents; ++r) {
    const float delay = r == 0 ? 0.0F : Doorstep::k_companion_delay;
    const float t = visit.t - delay;
    const float end = visit.duration - (2.0F * delay);
    if (t < 0.0F) {
      continue;
    }
    part = std::max(part, bump(t));
    part = std::max(part, bump(end - t));
  }
  return std::clamp(part, 0.0F, 1.0F);
}

auto HomeActivity::shutter_angle(std::uint64_t id, int window) const -> float {
  const auto seed =
      mix(low32(id) ^ 0x7f4a7c15U ^ (static_cast<std::uint32_t>(window) * 0x85ebca6bU));

  const float style = roll(seed, 1U);
  float angle = 0.0F;
  if (style < 0.22F) {
    angle = 2.0F + (6.0F * roll(seed, 2U));
  } else if (style < 0.52F) {
    angle = 30.0F + (45.0F * roll(seed, 2U));
  } else {
    angle = 135.0F + (38.0F * roll(seed, 2U));
  }

  const float threshold = 0.18F + (0.50F * roll(seed, 3U));
  const float closing = smoothstep(threshold, threshold + 0.28F, night_factor);
  return angle * (1.0F - closing);
}

auto HomeActivity::gag(std::uint64_t id, float time) -> float {
  if (gag_home == 0 && actors_allowed) {

    const float interval = std::max(330.0F, cooldown_seconds * 0.6F);

    const float jitter = std::max(0.0F, interval - 305.0F);
    const auto cycle =
        static_cast<std::uint32_t>(std::max(0.0F, std::floor(time / interval)));
    const auto seed = mix(low32(id) ^ mix(cycle));
    const float start = (static_cast<float>(cycle) * interval) +
                        ((static_cast<float>(seed % 1000U) / 1000.0F) * jitter);
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

auto yaw_of(const QVector3D& direction) -> float {
  return std::atan2(direction.x(), direction.z()) * 180.0F / k_pi;
}

struct DoorFrame {
  QVector3D doorstep;
  QVector3D outward;
  QVector3D lateral;
  float scale{1.0F};
};

auto door_frame(const DrawContext& ctx,
                const HomeSmokeAnchor& anchor,
                const QVector3D& doorstep) -> DoorFrame {
  auto outward = ctx.model.mapVector(anchor.outward);
  outward.setY(0.0F);
  if (outward.lengthSquared() < 1e-4F) {
    outward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  outward.normalize();
  return DoorFrame{.doorstep = doorstep,
                   .outward = outward,
                   .lateral = QVector3D(outward.z(), 0.0F, -outward.x()),
                   .scale = ctx.model.column(1).toVector3D().length()};
}

auto ground(const DrawContext& ctx,
            const DoorFrame& frame,
            QVector3D point,
            float out) -> QVector3D {
  if (!ctx.world_view.has_terrain()) {
    return point;
  }
  const float terrain = ctx.world_view.terrain()->resolve_surface_world_y(
      point.x(), point.z(), 0.3F, point.y());
  const float street = std::max(terrain, frame.doorstep.y() - (0.18F * frame.scale));
  const float off_plinth = std::clamp(out / 0.7F, 0.0F, 1.0F);
  point.setY(std::max(terrain, point.y() + ((street - point.y()) * off_plinth)));
  return point;
}

struct ResidentPose {
  QVector3D position;
  float yaw{0.0F};
  std::uint16_t clip{Animation::k_humanoid_idle_clip};
  float phase{0.0F};
  bool held{false};
  bool drawn{false};
  std::uint16_t blend_clip{0xFFFFU};
  float blend_phase{0.0F};
  float blend_weight{0.0F};
};

constexpr float k_resident_turn_seconds = 0.6F;
constexpr float k_resident_blend_seconds = 0.35F;

auto resident_smooth(float t) -> float {
  const float c = std::clamp(t, 0.0F, 1.0F);
  return c * c * (3.0F - 2.0F * c);
}

auto resident_turn(float from, float to, float t) -> float {
  return from + std::remainder(to - from, 360.0F) * resident_smooth(t);
}

void blend_from(ResidentPose& pose, std::uint16_t clip, float phase, float elapsed) {
  if (elapsed < 0.0F || elapsed >= k_resident_blend_seconds || clip == pose.clip) {
    return;
  }
  pose.blend_clip = clip;
  pose.blend_phase = phase - std::floor(phase);
  pose.blend_weight = 1.0F - resident_smooth(elapsed / k_resident_blend_seconds);
}

auto resident_pose(const DrawContext& ctx,
                   const DoorFrame& frame,
                   const DoorstepVisit& visit,
                   int r,
                   std::uint32_t seed) -> ResidentPose {
  ResidentPose pose;
  const float delay = r == 0 ? 0.0F : Doorstep::k_companion_delay;
  const float t = visit.t - delay;
  const float duration = visit.duration - (2.0F * delay);
  if (t < 0.0F || t >= duration) {
    return pose;
  }
  const float step = visit.step_seconds();

  const float lateral =
      visit.residents > 1 ? visit.lateral + (r == 0 ? -0.55F : 0.55F) : visit.lateral;
  const float time = ctx.animation_time;
  const float idle_rate = 0.24F + (0.10F * roll(seed, 11U));
  const float breath_rate = 1.0F / (7.0F + (2.0F * roll(seed, 15U)));
  const float idle_phase = roll(seed, 12U);

  auto along = [&](float out, float across) {
    return frame.doorstep + (frame.outward * out) + (frame.lateral * across);
  };

  auto path = [&](float s) {
    return along(visit.out_metres * s, lateral * (0.25F + (0.75F * s)));
  };
  const QVector3D settle = path(1.0F);
  const QVector3D heading = (settle - path(0.0F)).normalized();

  pose.drawn = true;
  if (t < step) {
    const float s = t / step;
    pose.position = path(s);
    pose.yaw = yaw_of(heading);
    pose.clip = Animation::k_humanoid_walk_clip;

    pose.phase = (visit.out_metres * s) / Spill::k_walk_metres_per_cycle;
    pose.position = ground(ctx, frame, pose.position, visit.out_metres * s);
    return pose;
  }
  if (t >= duration - step) {
    const float back = t - (duration - step);
    const float s = 1.0F - (back / step);
    pose.position = path(s);
    const float settled_yaw =
        yaw_of(frame.outward) + ((roll(seed, 13U) - 0.5F) * 60.0F);
    pose.yaw =
        resident_turn(settled_yaw, yaw_of(-heading), back / k_resident_turn_seconds);
    pose.clip = Animation::k_humanoid_walk_clip;
    pose.phase = (visit.out_metres * (1.0F - s)) / Spill::k_walk_metres_per_cycle;
    blend_from(pose, Animation::k_humanoid_idle_clip, time * breath_rate, back);
    pose.position = ground(ctx, frame, pose.position, visit.out_metres * s);
    return pose;
  }

  const float busy = t - step;
  const float busy_end = duration - (2.0F * step);
  pose.position = ground(ctx, frame, settle, visit.out_metres);

  const float turn = (roll(seed, 13U) - 0.5F) * 60.0F;
  pose.yaw = yaw_of(frame.outward) + turn;
  pose.phase = (time * idle_rate) + idle_phase;
  const float arrival_phase = visit.out_metres / Spill::k_walk_metres_per_cycle;
  switch (visit.activity) {
  case DoorstepActivity::Stand:
    pose.clip = Animation::k_humanoid_idle_weave_clip;
    break;
  case DoorstepActivity::Sweep:
    pose.clip = Animation::k_humanoid_construct_chisel_clip;
    pose.phase = (time / 2.6F) + idle_phase;
    break;
  case DoorstepActivity::Scrub:

    pose.clip = Animation::k_humanoid_construct_kneel_chisel_clip;
    pose.phase = (time / 3.0F) + idle_phase;
    pose.yaw = yaw_of(-frame.outward) + (turn * 0.4F);
    break;
  case DoorstepActivity::Sit: {
    pose.yaw = yaw_of(frame.outward) + (turn * 0.5F);
    if (busy < Doorstep::k_sit_down_seconds) {
      pose.clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
      pose.phase = busy / Doorstep::k_sit_down_seconds;
      pose.held = true;
    } else if (busy >= busy_end - Doorstep::k_sit_down_seconds) {

      pose.clip = Animation::k_humanoid_showcase_rest_sit_down_clip;
      pose.phase = (busy_end - busy) / Doorstep::k_sit_down_seconds;
      pose.held = true;
    } else {
      pose.clip = Animation::k_humanoid_showcase_rest_sit_clip;
      pose.phase = time * 0.12F;
    }
    break;
  }
  case DoorstepActivity::Errand: {

    const float leg = visit.errand_metres / Doorstep::k_walk_speed;
    const float pause = std::max(0.0F, busy_end - (2.0F * leg));
    const float side = roll(seed, 14U) < 0.5F ? -1.0F : 1.0F;
    const QVector3D far_end = settle + (frame.lateral * (side * visit.errand_metres));
    float walked = 0.0F;
    bool walking = true;
    bool back = false;
    if (busy < leg) {
      walked = visit.errand_metres * (busy / leg);
    } else if (busy < leg + pause) {
      walked = visit.errand_metres;
      walking = false;
    } else {
      walked = visit.errand_metres * (1.0F - ((busy - leg - pause) / leg));
      back = true;
    }
    walked = std::clamp(walked, 0.0F, visit.errand_metres);
    pose.position = ground(
        ctx, frame, settle + (frame.lateral * (side * walked)), visit.out_metres);
    const float out_yaw = yaw_of((far_end - settle).normalized());
    const float pause_yaw = yaw_of(frame.outward) + (side * 40.0F);
    if (walking) {
      pose.clip = Animation::k_humanoid_walk_clip;
      const float covered =
          back ? visit.errand_metres + (visit.errand_metres - walked) : walked;
      pose.phase = (arrival_phase * Spill::k_walk_metres_per_cycle + covered) /
                   Spill::k_walk_metres_per_cycle;
      if (back) {
        const float since = busy - leg - pause;
        pose.yaw =
            resident_turn(pause_yaw, out_yaw + 180.0F, since / k_resident_turn_seconds);
        blend_from(pose, Animation::k_humanoid_idle_weave_clip, pose.phase, since);
      } else {
        pose.yaw =
            resident_turn(yaw_of(heading), out_yaw, busy / k_resident_turn_seconds);
      }
    } else {
      pose.clip = Animation::k_humanoid_idle_weave_clip;
      const float since = busy - leg;
      pose.yaw = resident_turn(out_yaw, pause_yaw, since / k_resident_turn_seconds);
      blend_from(
          pose,
          Animation::k_humanoid_walk_clip,
          (arrival_phase * Spill::k_walk_metres_per_cycle + visit.errand_metres) /
              Spill::k_walk_metres_per_cycle,
          since);
    }
    return pose;
  }
  case DoorstepActivity::Talk: {

    const QVector3D other =
        along(visit.out_metres, visit.lateral + (r == 0 ? 0.55F : -0.55F));
    pose.yaw = yaw_of((other - settle).normalized());
    pose.clip = r == 0 ? Animation::k_humanoid_idle_clip
                       : Animation::k_humanoid_idle_weave_clip;
    if (r == 0) {
      pose.phase = (time * breath_rate) + idle_phase;
    }
    break;
  }
  case DoorstepActivity::Count:
    break;
  }
  const float heading_yaw = yaw_of(heading);
  pose.yaw = resident_turn(heading_yaw, pose.yaw, busy / k_resident_turn_seconds);
  if (!pose.held) {
    blend_from(pose, Animation::k_humanoid_walk_clip, arrival_phase, busy);
  }
  return pose;
}

void submit_doorstep_residents(const DrawContext& ctx,
                               ISubmitter& out,
                               bool carthage,
                               const HomeSmokeAnchor& anchor,
                               std::uint64_t id,
                               const QVector3D& doorstep) {
  auto* activity = ctx.home_activity;
  if (activity == nullptr || activity->remaining_actors <= 0) {
    return;
  }
  const auto visit = activity->doorstep_plan(id, ctx.animation_time);
  if (!visit.active()) {
    return;
  }
  const auto& rig = nation_civilian_rig(carthage);
  if (!rig.valid()) {
    return;
  }
  const auto frame = door_frame(ctx, anchor, doorstep);
  begin_civilian_actors();
  int submitted = 0;
  for (int r = 0; r < visit.residents && activity->remaining_actors > 0; ++r) {
    const auto seed =
        mix((low32(id) * 2246822519U) + (static_cast<std::uint32_t>(r) * 7919U));
    const auto pose = resident_pose(ctx, frame, visit, r, seed);
    if (!pose.drawn) {
      continue;
    }
    QMatrix4x4 world;
    world.translate(pose.position);
    world.rotate(pose.yaw, 0, 1, 0);
    add_civilian_actor(ctx,
                       rig,
                       CivilianActor{
                           .archetype = rig.idle,
                           .clip = pose.clip,
                           .phase = pose.held ? std::clamp(pose.phase, 0.0F, 1.0F)
                                              : pose.phase - std::floor(pose.phase),
                           .world = world,
                           .owner_id = static_cast<std::uint32_t>(id),
                           .instance = static_cast<std::uint16_t>(2 + r),
                           .seed = seed,
                           .distant = false,
                           .blend_clip = pose.blend_clip,
                           .blend_phase = pose.blend_phase,
                           .blend_weight = pose.blend_weight,
                       });
    --activity->remaining_actors;
    ++submitted;
  }
  if (submitted > 0) {
    submit_civilian_actors(out);
  }
}

void submit_cloth(const DrawContext& ctx,
                  ISubmitter& out,
                  const HomeSmokeAnchor& anchor,
                  std::uint64_t id,
                  float parting) {
  const auto seed = mix(low32(id) ^ 0x2545f491U);
  const float time = ctx.animation_time;
  const float breeze = k_tau * roll(seed, 1U);
  const float strength = 0.6F + (0.7F * roll(seed, 2U));
  const float gust_rate = 0.31F + (0.20F * roll(seed, 3U));
  const float flutter_rate = 1.15F + (0.35F * roll(seed, 4U));
  const auto& strip = cloth_strip_archetype(anchor.cloth_indigo);
  const auto& tail = cloth_strip_tail_archetype(anchor.cloth_indigo);
  for (int i = 0; i < k_cloth_strips; ++i) {
    const float fi = static_cast<float>(i);
    const float across = ((fi / (k_cloth_strips - 1)) - 0.5F) * anchor.cloth_span;

    const float gust =
        0.5F + (0.5F * std::sin((time * gust_rate * k_tau) + breeze - (fi * 0.55F)));
    const float lift = strength * (2.2F + (6.0F * gust * gust));
    const float wave = std::sin((time * flutter_rate * k_tau) + breeze + (fi * 0.9F));

    const float lag =
        std::sin((time * flutter_rate * k_tau) + breeze + (fi * 0.9F) - 1.1F);
    const float upper = wave * lift;
    const float lower = (lag * lift * 1.35F) + (upper * 0.35F);

    const float side = (fi - ((k_cloth_strips - 1) * 0.5F)) < 0.0F ? -1.0F : 1.0F;
    const float part =
        parting * side *
        (24.0F + (16.0F * (1.0F - std::abs(across) /
                                      std::max(anchor.cloth_span * 0.5F, 1e-3F))));

    auto model = ctx.model;
    model.translate(anchor.cloth_top + QVector3D(across, 0.0F, 0.0F));
    model.rotate(part, 0.0F, 0.0F, 1.0F);
    model.rotate(upper, 1.0F, 0.0F, 0.15F);
    submit_render_instance(out,
                           RenderInstance{.archetype = &strip,
                                          .world = model,
                                          .alpha_multiplier = ctx.alpha_multiplier});
    model.translate(0.0F, -k_cloth_link_length, 0.0F);
    model.rotate(part * 0.4F, 0.0F, 0.0F, 1.0F);
    model.rotate(lower - upper, 1.0F, 0.0F, 0.15F);
    submit_render_instance(out,
                           RenderInstance{.archetype = &tail,
                                          .world = model,
                                          .alpha_multiplier = ctx.alpha_multiplier});
  }
}

void submit_shutters(const DrawContext& ctx,
                     ISubmitter& out,
                     bool carthage,
                     const HomeSmokeAnchor& anchor,
                     std::uint64_t id) {
  auto* activity = ctx.home_activity;
  const auto& leaf = shutter_leaf_archetype(carthage);
  int window = 0;
  for (const float side : {-1.0F, 1.0F}) {
    for (const float along : {-1.0F, 1.0F}) {
      const float angle = activity->shutter_angle(id, window++);
      const QVector3D centre(
          side * anchor.window.x(), anchor.window.y(), along * anchor.window.z());
      for (const float edge : {-1.0F, 1.0F}) {

        auto model = ctx.model;
        model.translate(centre + QVector3D(0.0F, 0.0F, edge * anchor.shutter_width));
        model.rotate(edge > 0.0F ? 180.0F : 0.0F, 0.0F, 1.0F, 0.0F);
        model.rotate(side * -edge * angle, 0.0F, 1.0F, 0.0F);
        model.scale(1.0F, anchor.shutter_height, anchor.shutter_width);
        submit_render_instance(
            out,
            RenderInstance{.archetype = &leaf,
                           .world = model,
                           .alpha_multiplier = ctx.alpha_multiplier});
      }
    }
  }
}

void submit_laundry(const DrawContext& ctx,
                    ISubmitter& out,
                    bool carthage,
                    const HomeSmokeAnchor& anchor,
                    std::uint64_t id) {
  auto* activity = ctx.home_activity;
  const auto seed = mix(low32(id) ^ 0x68e31da4U);
  if (roll(seed, 1U) >= (carthage ? 0.48F : 0.36F)) {
    return;
  }
  const QVector3D run = anchor.line_b - anchor.line_a;
  const float length = run.length();
  if (length < 1e-3F) {
    return;
  }
  const QVector3D axis = run / length;
  const auto turn = QQuaternion::rotationTo(QVector3D(1.0F, 0.0F, 0.0F), axis);
  {
    auto model = ctx.model;
    model.translate(anchor.line_a);
    model.rotate(turn);
    model.scale(length, 1.0F, 1.0F);
    submit_render_instance(out,
                           RenderInstance{.archetype = &washing_line_archetype(),
                                          .world = model,
                                          .alpha_multiplier = ctx.alpha_multiplier});
  }
  if (anchor.line_pole) {
    auto model = ctx.model;
    model.translate(
        QVector3D(anchor.line_b.x(), anchor.line_b.y() - 0.42F, anchor.line_b.z()));
    model.scale(1.0F, 0.46F, 1.0F);
    submit_render_instance(out,
                           RenderInstance{.archetype = &washing_pole_archetype(),
                                          .world = model,
                                          .alpha_multiplier = ctx.alpha_multiplier});
  }
  const int pieces =
      1 + (static_cast<int>(roll(seed, 2U) * 3.0F) % std::max(1, anchor.max_laundry));
  const float time = ctx.animation_time;
  const float breeze = k_tau * roll(seed, 3U);
  const float gust_rate = 0.27F + (0.18F * roll(seed, 4U));
  for (int i = 0; i < pieces; ++i) {
    const auto piece = mix(seed + (static_cast<std::uint32_t>(i) * 977U));

    const float threshold = 0.25F + (0.45F * roll(piece, 1U));
    const float alpha =
        1.0F - smoothstep(threshold, threshold + 0.20F, activity->night());
    if (alpha < 0.03F) {
      continue;
    }
    const float width =
        (carthage ? 0.16F : 0.11F) + ((carthage ? 0.14F : 0.05F) * roll(piece, 2U));
    const float height = 0.20F + (0.16F * roll(piece, 3U));
    const float slot = (static_cast<float>(i) + 0.5F) / static_cast<float>(pieces);
    const float at = std::clamp(slot + ((roll(piece, 4U) - 0.5F) * 0.3F /
                                        static_cast<float>(pieces)),
                                0.08F,
                                0.92F) *
                     length;
    const int colour =
        static_cast<int>(roll(piece, 5U) * static_cast<float>(k_laundry_colours));
    const float gust =
        0.5F + (0.5F * std::sin((time * gust_rate * k_tau) + breeze - (slot * 1.8F)));
    const float sway =
        std::sin((time * 1.05F * k_tau * (0.9F + (0.2F * roll(piece, 6U)))) + breeze +
                 (slot * 2.4F)) *
        (3.0F + (7.0F * gust));
    auto model = ctx.model;
    model.translate(anchor.line_a + (axis * at));
    model.rotate(turn);
    model.rotate(sway, 1.0F, 0.0F, 0.0F);
    model.scale(width, height, 1.0F);
    submit_render_instance(
        out,
        RenderInstance{.archetype = &laundry_piece_archetype(colour),
                       .world = model,
                       .alpha_multiplier = ctx.alpha_multiplier * alpha});
  }
}
} // namespace

namespace {

void submit_hens(const DrawContext& ctx,
                 ISubmitter& out,
                 const HomeSmokeAnchor& anchor,
                 std::uint64_t id) {
  const auto seed = mix(low32(id) ^ 0x6a09e667U);
  if (roll(seed, 1U) < 0.30F) {
    return;
  }
  const int count = 2 + static_cast<int>(roll(seed, 2U) * 2.99F);
  const float front = anchor.doorstep.z();
  const float time = ctx.animation_time;

  auto yard_point = [&](std::uint32_t hen_seed, std::int64_t leg) {
    const auto leg_seed =
        mix(hen_seed ^ static_cast<std::uint32_t>(leg * 0x27d4eb2dLL));
    const bool side = roll(hen_seed, 3U) > 0.6F;
    if (side) {
      return QVector3D(front + 0.10F + roll(leg_seed, 1U) * 0.16F,
                       0.012F,
                       -0.80F + roll(leg_seed, 2U) * 1.60F);
    }
    return QVector3D(-0.95F + roll(leg_seed, 1U) * 1.90F,
                     0.012F,
                     front + 0.06F + roll(leg_seed, 2U) * 0.26F);
  };
  auto to_world = [&](const QVector3D& local) {
    auto placed = ctx.model;
    placed.translate(local);
    return placed.column(3).toVector3D();
  };

  for (int hen = 0; hen < count; ++hen) {
    const auto hen_seed = mix(seed + static_cast<std::uint32_t>(hen) * 0x9e3779b9U);
    const float leg_seconds = 4.5F + roll(hen_seed, 4U) * 3.5F;
    const float clock = (time + roll(hen_seed, 5U) * 97.0F) / leg_seconds;
    const auto leg = static_cast<std::int64_t>(std::floor(clock));
    const float u = clock - std::floor(clock);
    constexpr float k_trot_share = 0.22F;

    const QVector3D from = to_world(yard_point(hen_seed, leg));
    const QVector3D to = to_world(yard_point(hen_seed, leg + 1));
    QVector3D heading = to - from;
    heading.setY(0.0F);
    const float yaw = heading.lengthSquared() > 1e-6F
                          ? std::atan2(heading.x(), heading.z()) * 180.0F / k_pi
                          : 0.0F;

    QVector3D position = to;
    float body_bob = 0.0F;
    float head_drop = 0.0F;
    float head_turn = 0.0F;
    if (u < k_trot_share) {
      const float s = smoothstep(0.0F, 1.0F, u / k_trot_share);
      position = from + (to - from) * s;
      body_bob = std::abs(std::sin(u / k_trot_share * k_pi * 7.0F)) * 0.010F;
    } else {
      const float pecking = (u - k_trot_share) * leg_seconds;
      const float cycle = wrap(pecking + roll(hen_seed, 6U), 1.9F);
      if (cycle < 1.1F) {
        const float jab = wrap(cycle, 0.37F) / 0.37F;
        head_drop =
            jab < 0.35F ? jab / 0.35F : std::max(0.0F, 1.0F - (jab - 0.35F) / 0.4F);
      } else {
        head_turn =
            std::sin((cycle - 1.1F) * 7.0F + roll(hen_seed, 7U) * k_tau) * 40.0F;
      }
    }

    const int breed = static_cast<int>(roll(hen_seed, 8U) * (k_hen_breeds - 0.01F));
    QMatrix4x4 body;
    body.translate(position + QVector3D(0.0F, body_bob, 0.0F));
    body.rotate(yaw, 0.0F, 1.0F, 0.0F);
    submit_render_instance(
        out, RenderInstance{.archetype = &hen_body_archetype(breed), .world = body});

    QMatrix4x4 head = body;
    const QVector3D neck_up(0.0F, 0.146F, 0.070F);
    const QVector3D neck_down(0.0F, 0.046F, 0.118F);
    head.translate(neck_up + (neck_down - neck_up) * head_drop);
    head.rotate(head_turn, 0.0F, 1.0F, 0.0F);
    head.rotate(72.0F * head_drop, 1.0F, 0.0F, 0.0F);
    submit_render_instance(
        out, RenderInstance{.archetype = &hen_head_archetype(breed), .world = head});
  }
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
  constexpr float k_far = 190.0F;
  if ((pixels >= 0 && pixels < 9) || ctx.distance_sq > k_far * k_far) {
    return;
  }
  const float distance = std::sqrt(std::max(ctx.distance_sq, 0.0F));

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
    const auto character = hearth_character(id);
    if (visible(vent, anchor.plume_radius * character.size * scale)) {

      const float puff =
          1.0F - character.puff_depth +
          (character.puff_depth *
           (0.5F + (0.5F * std::sin((ctx.animation_time * character.puff_rate * k_tau) +
                                    character.puff_phase))));

      const float far_fade = 1.0F - smoothstep(k_far * 0.72F, k_far, distance);

      const QVector3D cool = anchor.smoke_tint * QVector3D(0.94F, 0.97F, 1.03F);
      const QVector3D warm = anchor.smoke_tint * QVector3D(1.10F, 1.00F, 0.86F);

      const float dusk = 1.0F - (0.62F * activity->night());
      const QVector3D tint = (cool + ((warm - cool) * character.warmth)) * dusk;
      const float night_thin = 1.0F - (0.35F * activity->night());

      out.hearth_smoke(
          vent,
          tint,
          anchor.plume_radius * character.size * scale,
          std::clamp(
              intensity * character.density * puff * far_fade * night_thin, 0.0F, 1.0F),
          (ctx.animation_time * character.speed) + character.time_offset);
      --activity->remaining_plumes;
    }
  }

  const bool detailed =
      (pixels < 0 || pixels >= 14) && visible(to_world(anchor.doorstep), 1.4F);
  if (anchor.cloth_span > 0.0F && detailed) {
    const float parting =
        anchor.cloth_at_door ? activity->curtain_parting(id, ctx.animation_time) : 0.0F;
    submit_cloth(ctx, out, anchor, id, parting);
  }
  if (anchor.shutter_width > 0.0F && detailed) {
    submit_shutters(ctx, out, carthage, anchor, id);
  }
  if (anchor.max_laundry > 0 && detailed) {
    submit_laundry(ctx, out, carthage, anchor, id);
  }
  if (detailed && activity->hens) {
    submit_hens(ctx, out, anchor, id);
  }

  if (activity->night() > 0.02F) {
    const auto lamp = activity->lamp_state(id, ctx.animation_time);
    const auto lamp_at = to_world(anchor.lamp);
    if (lamp.strength > 0.01F && visible(lamp_at, 1.0F)) {
      auto model = ctx.model;
      model.translate(anchor.lamp);
      auto glow = model.column(3);
      model = QMatrix4x4{};
      model.setColumn(3, glow);
      submit_render_instance(out,
                             RenderInstance{.archetype = &lamp_glow_archetype(),
                                            .world = model,
                                            .alpha_multiplier = 0.42F * lamp.strength});
      Render::LocalLight light;
      light.position = lamp_at + QVector3D(0.0F, 0.45F, 0.0F);
      light.color = lamp.color;
      light.radius = 3.8F;
      light.intensity = 0.72F * lamp.strength;
      out.local_light(light);
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

    submit_doorstep_residents(ctx, out, carthage, anchor, id, doorstep);
    return;
  }

  const auto frame = door_frame(ctx, anchor, doorstep);
  const auto& outward = frame.outward;
  const float walked = spill_walk_metres(t);
  const auto position = ground(ctx, frame, doorstep + (outward * walked), walked);

  const float yaw = yaw_of(outward);
  const bool returning = t >= Spill::k_return_end;
  QMatrix4x4 world;
  world.translate(position);
  world.rotate(returning ? yaw + 180.0F : yaw, 0, 1, 0);

  auto archetype = bowl;
  std::uint16_t clip = Animation::k_humanoid_walk_clip;
  float phase = walked / Spill::k_walk_metres_per_cycle;
  bool held = false;
  if (t < Spill::k_emerge_end) {

  } else if (t < Spill::k_trip_end) {

    clip = Animation::k_humanoid_react_stagger_clip;
    phase = (t - Spill::k_emerge_end) / (Spill::k_trip_end - Spill::k_emerge_end);
    held = true;
  } else if (t < Spill::k_sprawl_end) {

    archetype = rig.idle;
    clip = Animation::k_humanoid_dead_infantry_face_clip;
    phase = 0.0F;
    held = true;
  } else if (t < Spill::k_rise_end) {

    archetype = rig.idle;
    clip = Animation::k_humanoid_die_infantry_face_clip;
    phase =
        1.0F - ((t - Spill::k_sprawl_end) / (Spill::k_rise_end - Spill::k_sprawl_end));
    held = true;
  } else if (t < Spill::k_stare_end) {

    archetype = rig.idle;
    clip = Animation::k_humanoid_idle_clip;
    phase = (t - Spill::k_rise_end) * 0.4F;
  } else if (t < Spill::k_return_end) {
    archetype = rig.idle;
    clip = Animation::k_humanoid_idle_clip;
    phase = (t - Spill::k_rise_end) * 0.4F;
  } else {

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
