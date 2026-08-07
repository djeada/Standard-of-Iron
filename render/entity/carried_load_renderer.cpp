#include "carried_load_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/resource_types.h"
#include "../geom/stone.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../submission_visibility.h"
#include "../submitter.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

using Game::Systems::ResourceType;

constexpr float k_far_distance_sq = 120.0F * 120.0F;
constexpr float k_detail_distance_sq = 46.0F * 46.0F;
constexpr float k_cull_radius = 0.8F;

constexpr float k_carry_height = 1.00F;
constexpr float k_carry_forward = 0.30F;
constexpr int k_max_visible_carriers = 8;

constexpr QVector3D k_timber{0.47F, 0.32F, 0.18F};
constexpr QVector3D k_timber_cut{0.72F, 0.56F, 0.34F};
constexpr QVector3D k_stone_light{0.76F, 0.73F, 0.66F};
constexpr QVector3D k_stone_dark{0.44F, 0.42F, 0.38F};
constexpr QVector3D k_ore{0.30F, 0.29F, 0.31F};
constexpr QVector3D k_ore_rust{0.44F, 0.27F, 0.16F};
constexpr QVector3D k_basket{0.55F, 0.43F, 0.25F};

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
  for (ResourceType const type :
       {ResourceType::Wood, ResourceType::Stone, ResourceType::Iron}) {
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
  constexpr float k_radius = 0.062F;
  constexpr float k_half_length = 0.40F;
  constexpr std::array<std::array<float, 2>, 3> k_bundle{
      {{-0.066F, 0.0F}, {0.066F, 0.0F}, {0.0F, 0.112F}}};

  Mesh* const cube = get_unit_cube();
  for (std::size_t i = 0; i < k_bundle.size(); ++i) {
    auto const log_seed = static_cast<std::uint32_t>(seed + 17U + (i * 41U));
    float const skew = jitter(log_seed, 0.035F);
    float const half_length = k_half_length + jitter(log_seed * 3U, 0.045F);
    float const y = k_carry_height + k_bundle.at(i).at(1);
    float const z = k_carry_forward + k_bundle.at(i).at(0);
    QVector3D const left(-half_length, y - skew, z);
    QVector3D const right(half_length, y + skew, z);

    out.mesh(get_unit_cylinder(10),
             Render::Geom::cylinder_between(frame, left, right, k_radius),
             tint(k_timber, 0.84F + (rand01(log_seed * 5U) * 0.32F)));

    if (!detailed) {
      continue;
    }
    for (const auto& end : {left, right}) {
      put(out,
          cube,
          frame,
          end,
          QVector3D(0.013F, k_radius * 0.84F, k_radius * 0.84F),
          QVector3D(),
          tint(k_timber_cut, 0.94F + (rand01(log_seed * 7U) * 0.12F)));
    }
  }
}

void draw_block(ISubmitter& out,
                const QMatrix4x4& frame,
                std::uint32_t seed,
                bool detailed) {
  Mesh* const cube = get_unit_cube();
  put(out,
      cube,
      frame,
      QVector3D(0.0F, k_carry_height - 0.02F, k_carry_forward + 0.02F),
      QVector3D(0.170F, 0.110F, 0.130F),
      QVector3D(0.0F, jitter(seed, 7.0F), 0.0F),
      tint(k_stone_light, 0.84F + (rand01(seed * 3U) * 0.20F)));

  if (!detailed) {
    return;
  }
  Mesh* const stone = Render::Geom::Stone::get();
  put(out,
      (stone != nullptr) ? stone : cube,
      frame,
      QVector3D(
          jitter(seed * 5U, 0.05F), k_carry_height + 0.14F, k_carry_forward + 0.01F),
      QVector3D(0.072F, 0.052F, 0.064F),
      QVector3D(0.0F, rand01(seed * 11U) * 360.0F, 0.0F),
      tint(k_stone_dark, 1.05F + (rand01(seed * 13U) * 0.20F)));
}

void draw_ore_basket(ISubmitter& out,
                     const QMatrix4x4& frame,
                     std::uint32_t seed,
                     bool detailed) {
  Mesh* const basket = get_unit_tapered_cylinder(0.72F, 1.0F, 10);
  QMatrix4x4 body = frame;
  body.translate(0.0F, k_carry_height - 0.04F, k_carry_forward);
  body.scale(0.175F, 0.125F, 0.155F);
  out.mesh(basket, body, tint(k_basket, 0.90F + (rand01(seed) * 0.22F)));

  Mesh* const cube = get_unit_cube();
  put(out,
      cube,
      frame,
      QVector3D(0.0F, k_carry_height + 0.085F, k_carry_forward),
      QVector3D(0.180F, 0.018F, 0.160F),
      QVector3D(),
      tint(k_basket, 0.70F));

  if (!detailed) {
    return;
  }
  Mesh* const stone = Render::Geom::Stone::get();
  Mesh* const fallback = get_unit_sphere();
  constexpr std::array<std::array<float, 2>, 3> k_lumps{
      {{-0.072F, -0.040F}, {0.068F, 0.032F}, {0.000F, 0.076F}}};
  for (std::size_t i = 0; i < k_lumps.size(); ++i) {
    auto const lump_seed = static_cast<std::uint32_t>(seed + 61U + (i * 37U));
    float const radius = 0.058F + (rand01(lump_seed) * 0.020F);
    QVector3D const base = (rand01(lump_seed * 3U) > 0.6F) ? k_ore_rust : k_ore;
    put(out,
        (stone != nullptr) ? stone : fallback,
        frame,
        QVector3D(k_lumps.at(i).at(0),
                  k_carry_height + 0.10F + (radius * 0.45F),
                  k_carry_forward + k_lumps.at(i).at(1)),
        QVector3D(radius, radius * 0.78F, radius * 0.92F),
        QVector3D(0.0F, rand01(lump_seed * 5U) * 360.0F, 0.0F),
        tint(base, 0.98F + (rand01(lump_seed * 7U) * 0.26F)));
  }
}

void draw_load(ISubmitter& out,
               const QMatrix4x4& frame,
               ResourceType type,
               std::uint32_t seed,
               bool detailed) {
  switch (type) {
  case ResourceType::Stone:
    draw_block(out, frame, seed, detailed);
    return;
  case ResourceType::Iron:
    draw_ore_basket(out, frame, seed, detailed);
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
       world->get_entities_with<Engine::Core::ResourceCarryComponent>()) {
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
