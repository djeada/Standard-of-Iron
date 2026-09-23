#include "ambient_people.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>

#include "animation/clip_manifest.h"
#include "building_render_common.h"
#include "building_state.h"
#include "civilian_actor.h"
#include "home_activity.h"
#include "registry.h"

namespace Render::GL {
namespace {

constexpr float k_pi = 3.14159265F;
constexpr float k_walk_speed = 0.55F;
constexpr float k_metres_per_cycle = 0.66F;
constexpr float k_far_metres = 120.0F;
constexpr float k_min_pixels = 26.0F;
constexpr std::size_t k_max_route = 20;

auto roll(std::uint32_t seed, std::uint32_t salt) -> float {
  return static_cast<float>(ambient_hash(seed ^ (salt * 0x9e3779b9U)) & 0xffffU) /
         65536.0F;
}

auto yaw_of(const QVector3D& direction) -> float {
  if ((direction.x() * direction.x()) + (direction.z() * direction.z()) < 1e-8F) {
    return 0.0F;
  }
  return std::atan2(direction.x(), direction.z()) * 180.0F / k_pi;
}

auto fract(float value) -> float {
  return value - std::floor(value);
}

struct Pose {
  QVector3D position;
  float yaw{0.0F};
  std::uint16_t clip{Animation::k_humanoid_idle_clip};
  float phase{0.0F};
  bool dwelling{false};
  float dwell_time{0.0F};
  std::uint16_t blend_clip{0xFFFFU};
  float blend_phase{0.0F};
  float blend_weight{0.0F};
  std::uint16_t overlay_clip{0xFFFFU};
  float overlay_phase{0.0F};
};

constexpr float k_idle_cycle_seconds = 8.0F;
constexpr float k_ramp_fraction = 0.16F;
constexpr float k_turn_metres = 0.30F;
constexpr float k_turn_seconds = 0.55F;
constexpr float k_blend_seconds = 0.30F;

auto smooth(float t) -> float {
  const float c = std::clamp(t, 0.0F, 1.0F);
  return c * c * (3.0F - 2.0F * c);
}

auto lerp_yaw(float from, float to, float t) -> float {
  const float delta = std::remainder(to - from, 360.0F);
  return from + delta * std::clamp(t, 0.0F, 1.0F);
}

auto eased_travel(float s) -> float {
  const float r = k_ramp_fraction;
  const float v = 1.0F / (1.0F - r);
  if (s < r) {
    return 0.5F * v * s * s / r;
  }
  if (s > 1.0F - r) {
    const float left = 1.0F - s;
    return 1.0F - (0.5F * v * left * left / r);
  }
  return 0.5F * v * r + v * (s - r);
}

auto walk_route(const std::array<QVector3D, k_max_route>& points,
                std::size_t count,
                const QVector3D& facing,
                float time,
                std::uint32_t seed,
                bool porter) -> Pose {
  std::array<std::size_t, (k_max_route * 2) - 2> order{};
  std::size_t stops = 0;
  for (std::size_t p = 0; p < count; ++p) {
    order[stops++] = p;
  }
  for (std::size_t p = count - 2; p >= 1 && p < count; --p) {
    order[stops++] = p;
  }
  const float walk_speed = k_walk_speed * (0.92F + roll(seed, 17U) * 0.16F);
  auto leg_heading = [&](std::size_t leg) {
    QVector3D heading = points[order[(leg + 1) % stops]] - points[order[leg]];
    heading.setY(0.0F);
    return yaw_of(heading);
  };
  auto leg_time = [&](std::size_t leg) {
    const float length =
        (points[order[(leg + 1) % stops]] - points[order[leg]]).length();
    return length / (walk_speed * (1.0F - 0.5F * k_ramp_fraction * 2.0F)) + 1e-4F;
  };
  float cycle = 0.0F;
  const float far_dwell = 5.0F + roll(seed, 11U) * 6.0F;
  const float near_dwell = 2.5F + roll(seed, 13U) * 4.0F;
  for (std::size_t leg = 0; leg < stops; ++leg) {
    cycle += leg_time(leg);
  }
  cycle += far_dwell + near_dwell;
  float t = std::fmod(time + roll(seed, 3U) * cycle, std::max(cycle, 0.01F));
  float walked = 0.0F;
  const std::uint16_t walk_clip = Animation::k_humanoid_walk_clip;
  auto apply_carry = [&](Pose& pose) {
    if (porter) {
      pose.overlay_clip = Animation::k_humanoid_resource_carry_clip;
      pose.overlay_phase = fract(time / 1.8F);
    }
  };
  for (std::size_t leg = 0; leg < stops; ++leg) {
    const QVector3D& from = points[order[leg]];
    const QVector3D& to = points[order[(leg + 1) % stops]];
    const float length = (to - from).length();
    const float seconds = leg_time(leg);
    const float heading = leg_heading(leg);
    const std::size_t started_at = order[leg];
    const bool from_dwell = started_at == 0 || started_at == count - 1;
    if (t < seconds) {
      const float travelled = eased_travel(t / seconds) * length;
      Pose pose;
      pose.position = from + (to - from) * (travelled / std::max(length, 1e-4F));
      const float previous =
          from_dwell ? yaw_of(facing - from) : leg_heading((leg + stops - 1) % stops);
      pose.yaw = lerp_yaw(previous, heading, smooth(travelled / k_turn_metres));
      pose.clip = walk_clip;
      pose.phase = fract((walked + travelled) / k_metres_per_cycle);
      if (from_dwell && t < k_blend_seconds) {
        pose.blend_clip = Animation::k_humanoid_idle_clip;
        pose.blend_phase = 0.0F;
        pose.blend_weight = 1.0F - smooth(t / k_blend_seconds);
      }
      apply_carry(pose);
      return pose;
    }
    t -= seconds;
    walked += length;
    const std::size_t arrived = order[(leg + 1) % stops];
    const bool far_end = arrived == count - 1;
    const bool near_end = arrived == 0;
    if (!far_end && !near_end) {
      continue;
    }
    const float dwell = far_end ? far_dwell : near_dwell;
    if (t < dwell) {
      Pose pose;
      pose.position = to;
      const float stand = yaw_of(facing - to);
      const float next = leg_heading((leg + 1) % stops);
      pose.yaw = lerp_yaw(heading, stand, smooth(t / k_turn_seconds));
      pose.yaw = lerp_yaw(
          pose.yaw, next, smooth((t - (dwell - k_turn_seconds)) / k_turn_seconds));
      pose.clip = Animation::k_humanoid_idle_clip;
      pose.phase = fract(t / k_idle_cycle_seconds + roll(seed, 5U));
      pose.dwelling = far_end;
      pose.dwell_time = t;
      if (t < k_blend_seconds) {
        pose.blend_clip = walk_clip;
        pose.blend_phase = fract(walked / k_metres_per_cycle);
        pose.blend_weight = 1.0F - smooth(t / k_blend_seconds);
      }
      apply_carry(pose);
      return pose;
    }
    t -= dwell;
  }
  return Pose{points[0], 0.0F, Animation::k_humanoid_idle_clip, 0.0F};
}

} // namespace

void submit_ambient_people(const DrawContext& ctx,
                           ISubmitter& out,
                           bool carthage,
                           std::span<const AmbientPerson> people,
                           std::span<const WalkSurface> surfaces) {
  auto* activity = ctx.home_activity;
  if (people.empty() || activity == nullptr || !activity->actors_allowed ||
      activity->remaining_ambient_actors <= 0 || ctx.entity == nullptr ||
      ctx.template_prewarm || ctx.submission_visibility == nullptr) {
    return;
  }
  if (resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
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
  const QVector3D centre = ctx.model.column(3).toVector3D();
  const auto* fog = ctx.submission_visibility->snapshot();
  if (ctx.submission_fog_mode != SubmissionFogMode::Ignore && fog != nullptr &&
      fog->initialized && !fog->is_visible_world(centre.x(), centre.z())) {
    return;
  }
  const auto& civilian_rig = nation_civilian_rig(carthage);
  const auto& priest_rig = nation_priest_rig(carthage);
  if (!civilian_rig.valid()) {
    return;
  }

  const auto owner = static_cast<std::uint32_t>(ctx.entity->get_id());
  const float time = ctx.animation_time;
  const QMatrix4x4 to_local = surfaces.empty() ? QMatrix4x4{} : ctx.model.inverted();
  begin_civilian_actors();
  std::uint16_t instance = 40;
  for (std::size_t i = 0; i < people.size(); ++i) {
    if (activity->remaining_ambient_actors <= 0) {
      break;
    }
    const AmbientPerson& person = people[i];
    if (person.route.empty()) {
      continue;
    }
    const std::uint32_t seed =
        ambient_hash(owner * 747796405U + static_cast<std::uint32_t>(i));
    std::array<QVector3D, k_max_route> world_route{};
    const std::size_t count = std::min(person.route.size(), k_max_route);
    for (std::size_t p = 0; p < count; ++p) {
      world_route[p] = ctx.model.map(person.route[p]);
    }
    const QVector3D facing = ctx.model.map(person.facing);
    const float linger_turn = 18.0F * std::sin(time * 0.35F + roll(seed, 7U) * 6.0F);

    const bool walks =
        (person.role == AmbientRole::Stroll || person.role == AmbientRole::Porter) &&
        count >= 2;
    Pose pose;
    AmbientRole stance = person.role;
    float stance_time = time;
    if (walks) {
      pose = walk_route(
          world_route, count, facing, time, seed, person.role == AmbientRole::Porter);
      stance = pose.dwelling ? person.linger : AmbientRole::Stroll;
      stance_time = pose.dwell_time;
    } else {
      pose.position = world_route[0];
    }
    if (stance != AmbientRole::Stroll && stance != AmbientRole::Porter) {
      if (!walks) {
        pose.yaw = yaw_of(facing - pose.position);
      }
      switch (stance) {
      case AmbientRole::Weave:
        pose.clip = Animation::k_humanoid_idle_weave_clip;
        pose.phase = fract(stance_time / 2.8F + roll(seed, 1U));
        break;
      case AmbientRole::Squat:
        pose.clip = Animation::k_humanoid_idle_squat_clip;
        pose.phase = fract(stance_time / 3.0F + roll(seed, 1U));
        break;
      case AmbientRole::Kneel:
        pose.clip = Animation::k_humanoid_showcase_rest_kneel_clip;
        pose.phase = 0.6F;
        break;
      case AmbientRole::Haggle: {
        const float cycle = fract(stance_time / 7.5F + roll(seed, 1U)) * 7.5F;
        if (cycle > 5.4F) {
          pose.clip = Animation::k_humanoid_taunt_dismissive_clip;
          pose.phase = std::clamp((cycle - 5.4F) / 2.1F, 0.0F, 1.0F);
        } else {
          pose.clip = Animation::k_humanoid_idle_clip;
          pose.phase = fract(cycle / k_idle_cycle_seconds);
        }
        break;
      }
      default:
        pose.clip = Animation::k_humanoid_idle_clip;
        pose.phase = fract(stance_time / k_idle_cycle_seconds + roll(seed, 1U));
        pose.yaw += linger_turn;
        break;
      }
    }

    const auto& rig = person.priest && priest_rig.valid() ? priest_rig : civilian_rig;
    if (!surfaces.empty()) {
      QVector3D local = to_local.map(pose.position);
      float floor = 0.0F;
      for (const WalkSurface& surface : surfaces) {
        if (local.x() >= surface.min_x && local.x() <= surface.max_x &&
            local.z() >= surface.min_z && local.z() <= surface.max_z) {
          floor = std::max(floor, surface.top);
        }
      }
      local.setY(floor);
      pose.position = ctx.model.map(local);
    }

    QMatrix4x4 world;
    world.translate(pose.position);
    world.rotate(pose.yaw, 0.0F, 1.0F, 0.0F);
    add_civilian_actor(ctx,
                       rig,
                       CivilianActor{
                           .archetype = rig.idle,
                           .clip = pose.clip,
                           .phase = pose.phase,
                           .world = world,
                           .owner_id = owner,
                           .instance = instance++,
                           .seed = seed,
                           .distant = pixels >= 0.0F && pixels < 60.0F,
                           .blend_clip = pose.blend_clip,
                           .blend_phase = pose.blend_phase,
                           .blend_weight = pose.blend_weight,
                           .overlay_clip = pose.overlay_clip,
                           .overlay_phase = pose.overlay_phase,
                       });
    --activity->remaining_ambient_actors;
  }
  submit_civilian_actors(out);
}

} // namespace Render::GL
