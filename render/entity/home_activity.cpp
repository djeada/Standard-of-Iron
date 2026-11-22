#include "render/entity/home_activity.h"

#include <QQuaternion>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "animation/clip_manifest.h"
#include "game/core/component_gameplay.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/entity/building_state.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/home_props.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/frame/graphics_settings.h"

namespace Render::GL {

namespace {

constexpr auto mix = ambient_hash;

constexpr float k_tau = 6.2831853F;

constexpr float k_hearth_period = 190.0F;
constexpr float k_hearth_duty = 0.56F;
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
  remaining_ambient_actors = actors_allowed ? (max_plumes >= 32 ? 48 : 28) : 0;
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
  if (roll(turn, 1U) >= 0.72F) {
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

} // namespace Render::GL
