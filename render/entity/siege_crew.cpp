#include "siege_crew.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "animation/clip_manifest.h"
#include "civilian_actor.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "registry.h"

namespace Render::GL {
namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_walk_speed = 0.95F;
constexpr float k_walk_metres_per_cycle = 0.69F;
constexpr float k_push_metres_per_cycle = 0.60F;
constexpr float k_arrive_metres = 0.035F;
constexpr float k_blend_seconds = 0.32F;
constexpr float k_turn_rate = 7.0F;
constexpr float k_push_enter = 0.22F;
constexpr float k_push_leave = 0.08F;
constexpr float k_load_linger_seconds = 2.5F;
constexpr float k_far_metres = 110.0F;
constexpr float k_min_pixels = 16.0F;
constexpr float k_minimal_pixels = 34.0F;

struct Station {
  float anchor_x;
  float anchor_z;
  float offset_x;
  float offset_z;
  float facing;
  std::uint16_t clip;
  float cycle;
};

constexpr float k_face_forward = 0.0F;
constexpr float k_face_back = k_pi;
constexpr float k_face_right = 0.5F * k_pi;

using Animation::k_humanoid_crew_crank_clip;
using Animation::k_humanoid_crew_heave_clip;
using Animation::k_humanoid_crew_push_clip;
using Animation::k_humanoid_idle_clip;
using Animation::k_humanoid_idle_squat_clip;
using Animation::k_humanoid_idle_weave_clip;

constexpr std::array<Station, 4> k_catapult_push{{
    {-0.20F, -0.35F, 0.0F, -0.21F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {0.20F, -0.35F, 0.0F, -0.21F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {-0.57F, -0.46F, 0.0F, -0.19F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {0.57F, -0.46F, 0.0F, -0.19F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
}};
constexpr std::array<Station, 4> k_catapult_load{{
    {-0.30F, 0.34F, 0.0F, 0.23F, k_face_back, k_humanoid_crew_crank_clip, 0.0F},
    {0.30F, 0.34F, 0.0F, 0.23F, k_face_back, k_humanoid_crew_crank_clip, 0.0F},
    {-0.42F, -0.02F, -0.25F, 0.0F, k_face_right, k_humanoid_crew_heave_clip, 0.0F},
    {0.63F, 0.40F, 0.30F, 0.25F, k_face_forward, k_humanoid_idle_clip, 8.0F},
}};
constexpr std::array<Station, 4> k_catapult_rest{{
    {-0.42F, -0.35F, -0.18F, -0.30F, 0.35F * k_pi, k_humanoid_idle_weave_clip, 3.0F},
    {0.25F, -0.35F, 0.10F, -0.40F, -0.10F * k_pi, k_humanoid_idle_squat_clip, 3.0F},
    {-0.57F, 0.20F, -0.36F, 0.12F, k_face_right, k_humanoid_idle_clip, 8.0F},
    {0.57F, 0.10F, 0.34F, 0.02F, -0.60F * k_pi, k_humanoid_idle_clip, 8.0F},
}};

constexpr std::array<Station, 4> k_ballista_push{{
    {-0.14F, -0.40F, 0.0F, -0.21F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {0.14F, -0.40F, 0.0F, -0.21F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {0.355F, -0.02F, 0.10F, -0.24F, k_face_forward, k_humanoid_crew_push_clip, 0.0F},
    {},
}};
constexpr std::array<Station, 4> k_ballista_load{{
    {-0.13F, -0.40F, 0.0F, -0.20F, k_face_forward, k_humanoid_crew_crank_clip, 0.0F},
    {0.13F, -0.40F, 0.0F, -0.20F, k_face_forward, k_humanoid_crew_crank_clip, 0.0F},
    {-0.29F, 0.05F, -0.24F, 0.0F, k_face_right, k_humanoid_crew_heave_clip, 0.0F},
    {},
}};
constexpr std::array<Station, 4> k_ballista_rest{{
    {-0.30F, -0.40F, -0.12F, -0.24F, 0.25F * k_pi, k_humanoid_idle_weave_clip, 3.0F},
    {0.30F, -0.30F, 0.24F, -0.12F, -0.35F * k_pi, k_humanoid_idle_squat_clip, 3.0F},
    {0.36F, 0.30F, 0.26F, 0.10F, -0.55F * k_pi, k_humanoid_idle_clip, 8.0F},
    {},
}};

auto stations_for(bool ballista, SiegeCrewMode mode) -> const std::array<Station, 4>& {
  switch (mode) {
  case SiegeCrewMode::Push:
    return ballista ? k_ballista_push : k_catapult_push;
  case SiegeCrewMode::Load:
    return ballista ? k_ballista_load : k_catapult_load;
  case SiegeCrewMode::Rest:
    break;
  }
  return ballista ? k_ballista_rest : k_catapult_rest;
}

auto fract(float value) -> float {
  return value - std::floor(value);
}

auto wrap_angle(float angle) -> float {
  return std::remainder(angle, 2.0F * k_pi);
}

auto member_offset(std::size_t index) -> float {
  return fract(static_cast<float>(ambient_hash(0x51E6E000U + index)) *
               (1.0F / 4294967296.0F));
}

void set_clip(SiegeCrewMember& member, std::uint16_t clip, float phase) {
  if (clip != member.clip && member.placed) {
    member.previous_clip = member.clip;
    member.previous_phase = member.phase;
    member.blend = 1.0F;
  }
  member.clip = clip;
  member.phase = phase;
}

auto next_mode(const SiegeCrewState& state,
               const SiegeCrewFrame& frame,
               float time) -> SiegeCrewMode {
  const float push_threshold =
      state.mode == SiegeCrewMode::Push ? k_push_leave : k_push_enter;
  if (frame.movement > push_threshold) {
    return SiegeCrewMode::Push;
  }
  if (frame.loading || frame.firing ||
      time - state.last_load_time < k_load_linger_seconds) {
    return SiegeCrewMode::Load;
  }
  return SiegeCrewMode::Rest;
}

auto working_clip_phase(const Station& station,
                        const SiegeCrewFrame& frame,
                        std::size_t index,
                        float time) -> float {
  const float offset = member_offset(index);
  switch (station.clip) {
  case k_humanoid_crew_push_clip:
    return fract(frame.travelled / k_push_metres_per_cycle + offset * 0.5F);
  case k_humanoid_crew_crank_clip:
    return fract(frame.loading_time / (frame.ballista ? 0.70F : 1.00F) +
                 (index % 2U == 0U ? 0.0F : 0.5F));
  case k_humanoid_crew_heave_clip:
    return std::clamp(frame.loading_progress, 0.0F, 1.0F);
  default:
    break;
  }
  const float cycle = station.cycle > 0.0F ? station.cycle : 8.0F;
  return fract(time / cycle + offset);
}

} // namespace

auto siege_crew_size(bool ballista) noexcept -> std::size_t {
  return ballista ? 3U : 4U;
}

void advance_siege_crew(SiegeCrewState& state,
                        const SiegeCrewFrame& frame,
                        float time) {
  float dt = time - state.time;
  if (!state.initialized || dt < 0.0F || dt > 1.0F) {
    state = {};
    state.initialized = true;
    dt = 0.0F;
  }
  state.time = time;
  if (frame.loading || frame.firing) {
    state.last_load_time = time;
  }
  state.mode = next_mode(state, frame, time);

  const auto& stations = stations_for(frame.ballista, state.mode);
  const float scale = std::max(frame.engine_scale, 0.01F);
  const float blend_step = dt / k_blend_seconds;
  const float turn = 1.0F - std::exp(-dt * k_turn_rate);
  for (std::size_t i = 0; i < siege_crew_size(frame.ballista); ++i) {
    auto& member = state.members[i];
    const Station& station = stations[i];
    const float target_x = station.anchor_x * scale + station.offset_x;
    const float target_z = station.anchor_z * scale + station.offset_z;
    if (!member.placed) {
      member.x = target_x;
      member.z = target_z;
      member.yaw = station.facing;
    }
    member.blend = std::max(0.0F, member.blend - blend_step);

    const float dx = target_x - member.x;
    const float dz = target_z - member.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    float desired_yaw = station.facing;
    if (distance > k_arrive_metres) {
      const float step = std::min(distance, k_walk_speed * dt);
      member.x += dx / distance * step;
      member.z += dz / distance * step;
      member.walked += step;
      desired_yaw = std::atan2(dx, dz);
      const float walk_phase = fract(member.walked / k_walk_metres_per_cycle);
      set_clip(member, Animation::k_humanoid_walk_clip, walk_phase);
    } else {
      member.x = target_x;
      member.z = target_z;
      set_clip(member, station.clip, working_clip_phase(station, frame, i, time));
    }
    member.yaw += wrap_angle(desired_yaw - member.yaw) * (member.placed ? turn : 1.0F);
    member.placed = true;
  }
}

void submit_siege_crew(const DrawContext& ctx,
                       ISubmitter& out,
                       const SiegeCrewState& state,
                       const SiegeCrewFrame& frame,
                       bool carthage) {
  if (ctx.entity == nullptr || ctx.template_prewarm || !state.initialized) {
    return;
  }
  if (ctx.world != nullptr) {
    if (const auto* unit =
            ctx.world->try_get<Engine::Core::UnitComponent>(ctx.entity->get_id());
        unit != nullptr && unit->health <= 0) {
      return;
    }
  }
  if (ctx.distance_sq > k_far_metres * k_far_metres) {
    return;
  }
  const float scale = ctx.model.column(0).toVector3D().length();
  const float pixels =
      ctx.screen_metrics.projected_radius_px_from_distance_sq(ctx.distance_sq, scale);
  if (pixels >= 0.0F && pixels < k_min_pixels) {
    return;
  }
  const auto& rig = nation_crew_rig(carthage);
  if (!rig.valid()) {
    return;
  }

  QVector3D forward = ctx.model.column(2).toVector3D();
  forward.setY(0.0F);
  if (forward.lengthSquared() < 1.0e-8F) {
    return;
  }
  forward.normalize();
  const QVector3D right(forward.z(), 0.0F, -forward.x());
  const QVector3D origin = ctx.model.column(3).toVector3D();
  const float engine_yaw = std::atan2(forward.x(), forward.z());
  const auto owner = static_cast<std::uint32_t>(ctx.entity->get_id());
  const bool distant = pixels >= 0.0F && pixels < k_minimal_pixels;

  const auto& terrain = ctx.world_view.terrain_or_empty();
  begin_civilian_actors();
  for (std::size_t i = 0; i < siege_crew_size(frame.ballista); ++i) {
    const auto& member = state.members[i];
    if (!member.placed) {
      continue;
    }
    CivilianActor actor{};
    actor.archetype = rig.idle;
    actor.clip = member.clip;
    actor.phase = member.phase;
    actor.owner_id = owner;
    actor.instance = static_cast<std::uint16_t>(120U + i);
    actor.seed = ambient_hash(owner * 2654435761U + static_cast<std::uint32_t>(i));
    actor.distant = distant;
    if (member.blend > 0.0F && member.previous_clip != 0xFFFFU) {
      actor.blend_clip = member.previous_clip;
      actor.blend_phase = member.previous_phase;
      actor.blend_weight = member.blend * member.blend * (3.0F - 2.0F * member.blend);
    }
    // Each crew member stands on the ground under their own feet; on a slope
    // the engine's centre height would bury the uphill crew and float the rest.
    QVector3D stand = origin + right * member.x + forward * member.z;
    if (terrain.is_initialized()) {
      stand.setY(
          terrain.resolve_surface_world_y(stand.x(), stand.z(), 0.0F, stand.y()));
    }
    QMatrix4x4 world;
    world.translate(stand);
    world.rotate((engine_yaw + member.yaw) * 180.0F / k_pi, 0.0F, 1.0F, 0.0F);
    actor.world = world;
    add_civilian_actor(ctx, rig, actor);
  }
  submit_civilian_actors(out);
}

} // namespace Render::GL
