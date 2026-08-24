#include "carried_load_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "animation/hold_pose_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/resource_types.h"
#include "render/geom/stone.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/submission_visibility.h"
#include "render/submitter.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

using Game::Systems::ResourceType;

constexpr float k_far_distance_sq = 120.0F * 120.0F;
constexpr float k_detail_distance_sq = 46.0F * 46.0F;
constexpr float k_cull_radius = 0.8F;

constexpr float k_grip_height =
    HumanProportions::SHOULDER_Y + Animation::k_resource_carry_load_y_from_shoulder;
constexpr float k_grip_forward = Animation::k_resource_carry_load_z;
constexpr float k_hand_span = 0.19F;
constexpr int k_max_visible_carriers = 8;

constexpr QVector3D k_timber{0.47F, 0.32F, 0.18F};
constexpr QVector3D k_timber_cut{0.72F, 0.56F, 0.34F};
constexpr QVector3D k_stone_light{0.76F, 0.73F, 0.66F};
constexpr QVector3D k_stone_dark{0.44F, 0.42F, 0.38F};
constexpr QVector3D k_ore{0.24F, 0.25F, 0.28F};
constexpr QVector3D k_ore_light{0.39F, 0.40F, 0.43F};
constexpr QVector3D k_ore_rust{0.53F, 0.29F, 0.14F};
constexpr QVector3D k_straw{0.84F, 0.68F, 0.32F};
constexpr QVector3D k_straw_dark{0.62F, 0.47F, 0.20F};
constexpr QVector3D k_grain_head{0.92F, 0.78F, 0.40F};
constexpr QVector3D k_twine{0.36F, 0.24F, 0.12F};
constexpr QVector3D k_fleece{0.86F, 0.83F, 0.76F};
constexpr QVector3D k_fleece_shade{0.68F, 0.64F, 0.57F};
constexpr QVector3D k_meat{0.62F, 0.24F, 0.22F};
constexpr QVector3D k_hoof{0.24F, 0.21F, 0.19F};

auto rand01(std::uint32_t seed) -> float {
  std::uint32_t value = seed + 0x9E3779B9U;
  value ^= value >> 15U;
  value *= 0x85EBCA6BU;
  value ^= value >> 13U;
  value *= 0xC2B2AE35U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / static_cast<float>(0xFFFFFF);
}

auto jitter(std::uint32_t seed, float amount) -> float {
  return ((rand01(seed) * 2.0F) - 1.0F) * amount;
}

auto tint(const QVector3D& color, float amount) -> QVector3D {
  return {std::clamp(color.x() * amount, 0.0F, 1.0F),
          std::clamp(color.y() * amount, 0.0F, 1.0F),
          std::clamp(color.z() * amount, 0.0F, 1.0F)};
}

auto heaviest_carried(const Engine::Core::ResourceCarryComponent& carry)
    -> ResourceType {
  ResourceType heaviest = ResourceType::Wood;
  int best = -1;
  for (ResourceType const type : {ResourceType::Wood,
                                  ResourceType::Stone,
                                  ResourceType::Iron,
                                  ResourceType::Food}) {
    int const amount = carry.amounts.get(type);
    if (amount > best) {
      best = amount;
      heaviest = type;
    }
  }
  return heaviest;
}

void put(ISubmitter& out,
         Mesh* mesh,
         const QMatrix4x4& frame,
         const QVector3D& offset,
         const QVector3D& half_extent,
         const QVector3D& rotation_degrees,
         const QVector3D& color) {
  if (mesh == nullptr) {
    return;
  }
  QMatrix4x4 model = frame;
  model.translate(offset);
  if (rotation_degrees.y() != 0.0F) {
    model.rotate(rotation_degrees.y(), 0.0F, 1.0F, 0.0F);
  }
  if (rotation_degrees.x() != 0.0F) {
    model.rotate(rotation_degrees.x(), 1.0F, 0.0F, 0.0F);
  }
  if (rotation_degrees.z() != 0.0F) {
    model.rotate(rotation_degrees.z(), 0.0F, 0.0F, 1.0F);
  }
  model.scale(half_extent);
  out.mesh(mesh, model, color);
}

void draw_log(ISubmitter& out,
              const QMatrix4x4& frame,
              std::uint32_t seed,
              bool detailed) {
  constexpr float k_radius = 0.085F;
  constexpr float k_half_length = k_hand_span + 0.25F;
  constexpr std::array<std::array<float, 2>, 3> k_bundle{
      {{-0.09F, 0.0F}, {0.09F, 0.0F}, {0.0F, 0.145F}}};

  for (std::size_t i = 0; i < k_bundle.size(); ++i) {
    auto const log_seed = static_cast<std::uint32_t>(seed + 17U + (i * 41U));
    float const skew = jitter(log_seed, 0.035F);
    float const half_length = k_half_length + jitter(log_seed * 3U, 0.045F);
    float const y = k_grip_height + k_bundle.at(i).at(1);
    float const z = k_grip_forward + k_bundle.at(i).at(0);
    QVector3D const left(-half_length, y - skew, z);
    QVector3D const right(half_length, y + skew, z);

    out.mesh(get_unit_cylinder(10),
             Render::Geom::cylinder_between(frame, left, right, k_radius),
             tint(k_timber, 0.84F + (rand01(log_seed * 5U) * 0.32F)));

    if (!detailed) {
      continue;
    }
    QVector3D const end_axis = (right - left).normalized() * 0.014F;
    QVector3D const cut_color =
        tint(k_timber_cut, 0.94F + (rand01(log_seed * 7U) * 0.12F));
    out.mesh(get_unit_cylinder(10),
             Render::Geom::cylinder_between(
                 frame, left - end_axis, left + end_axis, k_radius * 0.88F),
             cut_color);
    out.mesh(get_unit_cylinder(10),
             Render::Geom::cylinder_between(
                 frame, right - end_axis, right + end_axis, k_radius * 0.88F),
             cut_color);
  }
}

void draw_block(ISubmitter& out,
                const QMatrix4x4& frame,
                std::uint32_t seed,
                bool detailed) {
  Mesh* const cube = get_unit_cube();
  Mesh* const stone = Render::Geom::Stone::get();
  put(out,
      (stone != nullptr) ? stone : cube,
      frame,
      QVector3D(0.0F, k_grip_height + 0.015F, k_grip_forward),
      QVector3D(0.235F, 0.165F, 0.185F),
      QVector3D(jitter(seed * 5U, 8.0F), jitter(seed, 12.0F), 0.0F),
      tint(k_stone_light, 0.84F + (rand01(seed * 3U) * 0.20F)));

  if (!detailed) {
    return;
  }
  put(out,
      (stone != nullptr) ? stone : cube,
      frame,
      QVector3D(jitter(seed * 7U, 0.07F),
                k_grip_height + 0.17F,
                k_grip_forward + jitter(seed * 11U, 0.045F)),
      QVector3D(0.105F, 0.072F, 0.088F),
      QVector3D(0.0F, rand01(seed * 11U) * 360.0F, 0.0F),
      tint(k_stone_dark, 1.05F + (rand01(seed * 13U) * 0.20F)));
}

void draw_iron_ore(ISubmitter& out,
                   const QMatrix4x4& frame,
                   std::uint32_t seed,
                   bool detailed) {
  Mesh* const stone = Render::Geom::Stone::get();
  Mesh* const cube = get_unit_cube();
  put(out,
      (stone != nullptr) ? stone : cube,
      frame,
      QVector3D(0.0F, k_grip_height + 0.025F, k_grip_forward),
      QVector3D(0.245F, 0.16F, 0.18F),
      QVector3D(jitter(seed * 3U, 8.0F), jitter(seed, 14.0F), 0.0F),
      tint(k_ore, 0.90F + (rand01(seed) * 0.18F)));

  if (!detailed) {
    return;
  }
  constexpr std::array<std::array<float, 3>, 4> k_veins{{{-0.13F, 0.10F, -0.08F},
                                                         {0.12F, 0.08F, 0.07F},
                                                         {-0.05F, 0.17F, 0.04F},
                                                         {0.08F, -0.07F, -0.09F}}};
  for (std::size_t i = 0; i < k_veins.size(); ++i) {
    auto const vein_seed = static_cast<std::uint32_t>(seed + 61U + (i * 37U));
    float const radius = 0.04F + (rand01(vein_seed) * 0.022F);
    QVector3D const vein_color = (i % 2U == 0U) ? k_ore_rust : k_ore_light;
    put(out,
        (stone != nullptr) ? stone : cube,
        frame,
        QVector3D(k_veins.at(i).at(0),
                  k_grip_height + k_veins.at(i).at(1),
                  k_grip_forward + k_veins.at(i).at(2)),
        QVector3D(radius * 1.55F, radius * 0.58F, radius),
        QVector3D(0.0F, rand01(vein_seed * 5U) * 360.0F, 0.0F),
        tint(vein_color, 0.96F + (rand01(vein_seed * 7U) * 0.22F)));
  }
}

void draw_sheaf(ISubmitter& out,
                const QMatrix4x4& frame,
                std::uint32_t seed,
                bool detailed) {

  Mesh* const cylinder = get_unit_cylinder(10);
  constexpr float k_half_length = k_hand_span + 0.115F;
  QVector3D const left(-k_half_length, k_grip_height + 0.02F, k_grip_forward);
  QVector3D const right(k_half_length, k_grip_height + 0.06F, k_grip_forward);
  out.mesh(cylinder,
           Render::Geom::cylinder_between(frame, left, right, 0.105F),
           tint(k_straw, 0.92F + (rand01(seed) * 0.16F)));
  out.mesh(cylinder,
           Render::Geom::cylinder_between(
               frame,
               QVector3D(-0.02F, k_grip_height + 0.04F, k_grip_forward),
               QVector3D(0.02F, k_grip_height + 0.04F, k_grip_forward),
               0.112F),
           k_twine);
  if (!detailed) {
    return;
  }
  Mesh* const cube = get_unit_cube();
  constexpr std::array<std::array<float, 3>, 5> k_heads{{{0.30F, 0.10F, -0.06F},
                                                         {0.34F, 0.02F, 0.05F},
                                                         {0.36F, 0.12F, 0.03F},
                                                         {-0.32F, 0.06F, -0.05F},
                                                         {-0.35F, 0.11F, 0.04F}}};
  for (std::size_t i = 0; i < k_heads.size(); ++i) {
    auto const head_seed = static_cast<std::uint32_t>(seed + 23U + (i * 29U));
    put(out,
        cube,
        frame,
        QVector3D(k_heads.at(i).at(0),
                  k_grip_height + k_heads.at(i).at(1),
                  k_grip_forward + k_heads.at(i).at(2)),
        QVector3D(0.055F, 0.018F, 0.018F),
        QVector3D(0.0F, 0.0F, jitter(head_seed, 18.0F)),
        tint(k_grain_head, 0.92F + (rand01(head_seed) * 0.16F)));
  }
  put(out,
      cube,
      frame,
      QVector3D(0.0F, k_grip_height - 0.07F, k_grip_forward),
      QVector3D(0.30F, 0.02F, 0.06F),
      QVector3D(),
      k_straw_dark);
}

void draw_carcass(ISubmitter& out,
                  const QMatrix4x4& frame,
                  std::uint32_t seed,
                  bool detailed) {

  Mesh* const cylinder = get_unit_cylinder(10);
  Mesh* const cube = get_unit_cube();

  constexpr float k_half_length = k_hand_span + 0.10F;
  float const sag = jitter(seed, 0.018F);
  QVector3D const left(-k_half_length, k_grip_height + 0.02F + sag, k_grip_forward);
  QVector3D const right(k_half_length, k_grip_height + 0.03F - sag, k_grip_forward);

  out.mesh(cylinder,
           Render::Geom::cylinder_between(frame, left, right, 0.115F),
           tint(k_fleece, 0.92F + (rand01(seed) * 0.14F)));

  put(out,
      cube,
      frame,
      QVector3D(k_half_length + 0.055F, k_grip_height - 0.035F, k_grip_forward + 0.01F),
      QVector3D(0.062F, 0.050F, 0.052F),
      QVector3D(0.0F, 0.0F, jitter(seed * 3U, 22.0F)),
      tint(k_fleece_shade, 0.96F));

  if (!detailed) {
    return;
  }

  put(out,
      cube,
      frame,
      QVector3D(-k_half_length - 0.030F, k_grip_height + 0.035F, k_grip_forward),
      QVector3D(0.034F, 0.034F, 0.036F),
      QVector3D(),
      k_meat);

  constexpr std::array<std::array<float, 2>, 4> k_legs{
      {{-0.11F, -0.05F}, {-0.05F, 0.06F}, {0.07F, -0.06F}, {0.12F, 0.05F}}};
  for (std::size_t i = 0; i < k_legs.size(); ++i) {
    auto const leg_seed = static_cast<std::uint32_t>(seed + 47U + (i * 31U));
    QVector3D const root(
        k_legs.at(i).at(0), k_grip_height - 0.02F, k_grip_forward + k_legs.at(i).at(1));
    QVector3D const hoof(k_legs.at(i).at(0) + jitter(leg_seed, 0.02F),
                         k_grip_height - 0.145F,
                         k_grip_forward + (k_legs.at(i).at(1) * 1.45F));
    out.mesh(cylinder,
             Render::Geom::cylinder_between(frame, root, hoof, 0.017F),
             tint(k_fleece_shade, 0.90F + (rand01(leg_seed) * 0.18F)));
    put(out, cube, frame, hoof, QVector3D(0.018F, 0.014F, 0.018F), QVector3D(), k_hoof);
  }
}

void draw_load(ISubmitter& out,
               const QMatrix4x4& frame,
               ResourceType type,
               Engine::Core::CarriedFoodForm food_form,
               std::uint32_t seed,
               bool detailed) {
  switch (type) {
  case ResourceType::Stone:
    draw_block(out, frame, seed, detailed);
    return;
  case ResourceType::Iron:
    draw_iron_ore(out, frame, seed, detailed);
    return;
  case ResourceType::Food:
    if (food_form == Engine::Core::CarriedFoodForm::Meat) {
      draw_carcass(out, frame, seed, detailed);
    } else {
      draw_sheaf(out, frame, seed, detailed);
    }
    return;
  default:
    draw_log(out, frame, seed, detailed);
    return;
  }
}

} // namespace

auto submit_carried_loads(Engine::Core::World* world,
                          ISubmitter& out,
                          const SubmissionVisibilityPolicy* visibility,
                          const Camera* camera) -> CarriedLoadSubmitStats {
  CarriedLoadSubmitStats stats;
  if (world == nullptr) {
    return stats;
  }

  QVector3D const camera_position =
      (camera != nullptr) ? camera->get_position() : QVector3D();

  for (auto* hauler :
       world->collect_entities_with<Engine::Core::ResourceCarryComponent>()) {
    if (hauler == nullptr) {
      continue;
    }
    const auto* carry = hauler->get_component<Engine::Core::ResourceCarryComponent>();
    const auto* transform = hauler->get_component<Engine::Core::TransformComponent>();
    if (carry == nullptr || transform == nullptr || carry->empty()) {
      continue;
    }
    const auto* unit = hauler->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->health <= 0) {
      continue;
    }

    QVector3D const origin(
        transform->position.x, transform->position.y, transform->position.z);
    float distance_sq = 0.0F;
    if (camera != nullptr) {
      distance_sq = (origin - camera_position).lengthSquared();
      if (distance_sq > k_far_distance_sq) {
        continue;
      }
    }
    if (visibility != nullptr &&
        !visibility->accepts_sphere(
            origin, k_cull_radius, SubmissionFogMode::VisibleOnly)) {
      continue;
    }

    stats.haulers += 1U;

    ResourceType const type = heaviest_carried(*carry);
    bool const detailed = distance_sq <= k_detail_distance_sq;

    float const unit_yaw = transform->rotation.y;
    QVector3D const body_scale(std::max(transform->scale.x, 0.01F),
                               std::max(transform->scale.y, 0.01F),
                               std::max(transform->scale.z, 0.01F));

    auto submit_at = [&](float local_x, float local_z, float local_yaw, int slot) {
      QMatrix4x4 frame;
      frame.translate(origin.x(), origin.y(), origin.z());
      frame.rotate(unit_yaw, 0.0F, 1.0F, 0.0F);
      frame.translate(local_x, 0.0F, local_z);
      frame.rotate(local_yaw, 0.0F, 1.0F, 0.0F);
      frame.scale(body_scale);
      draw_load(out,
                frame,
                type,
                carry->food_form,
                static_cast<std::uint32_t>((hauler->get_id() * 131U) +
                                           (static_cast<std::uint32_t>(slot) * 29U)),
                detailed);
      stats.loads += 1U;
    };

    const auto* formation =
        hauler->get_component<Engine::Core::FormationPresentationComponent>();
    if (formation == nullptr || formation->soldiers.empty()) {
      submit_at(0.0F, 0.0F, 0.0F, 0);
      continue;
    }

    int carried = 0;
    for (const auto& soldier : formation->soldiers) {
      if (!soldier.alive) {
        continue;
      }
      if (carried >= k_max_visible_carriers) {
        break;
      }
      submit_at(soldier.local_x,
                soldier.local_z,
                soldier.local_yaw,
                static_cast<int>(soldier.slot_index));
      ++carried;
    }
    if (carried == 0) {
      submit_at(0.0F, 0.0F, 0.0F, 0);
    }
  }

  return stats;
}

} // namespace Render::GL
