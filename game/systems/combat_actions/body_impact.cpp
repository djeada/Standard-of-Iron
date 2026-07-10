#include "body_impact.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../combat_system/combat_utils.h"

namespace Game::Systems::CombatActions {

namespace {

struct LocalBodySample {
  Engine::Core::Entity* entity{nullptr};
  QVector3D world_position{0.0F, 0.0F, 0.0F};
  float forward{0.0F};
  float right{0.0F};
  float distance{0.0F};
  float radius{0.0F};
};

[[nodiscard]] auto
make_local_body_sample(Engine::Core::Entity& attacker,
                       Engine::Core::Entity& target) -> LocalBodySample {
  LocalBodySample sample;
  sample.entity = &target;

  auto* attacker_transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    sample.entity = nullptr;
    return sample;
  }

  float const yaw =
      attacker_transform->rotation.y * (std::numbers::pi_v<float> / 180.0F);
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const right(-forward.z(), 0.0F, forward.x());
  QVector3D const origin(attacker_transform->position.x,
                         attacker_transform->position.y,
                         attacker_transform->position.z);
  sample.world_position = QVector3D(target_transform->position.x,
                                    target_transform->position.y,
                                    target_transform->position.z);
  QVector3D const to_target = sample.world_position - origin;
  sample.forward = QVector3D::dotProduct(to_target, forward);
  sample.right = QVector3D::dotProduct(to_target, right);
  sample.distance =
      std::sqrt(sample.forward * sample.forward + sample.right * sample.right);
  sample.radius = Game::Systems::Combat::combat_radius(&target);
  return sample;
}

[[nodiscard]] auto
body_contact_score(const LocalBodySample& sample,
                   const CombatActionDefinition& definition) -> float {
  if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
      !std::isfinite(sample.right) || !std::isfinite(sample.distance)) {
    return std::numeric_limits<float>::infinity();
  }

  float const sample_radius =
      std::isfinite(sample.radius) ? std::max(0.0F, sample.radius) : 0.0F;
  float const target_radius = std::min(sample_radius, definition.hit_shape.radius);
  float const reach = definition.hit_shape.reach + target_radius;
  float const lateral_limit = definition.hit_shape.radius + target_radius;
  if (sample.forward < -target_radius || sample.forward > reach ||
      std::abs(sample.right) > lateral_limit) {
    return std::numeric_limits<float>::infinity();
  }

  return std::max(0.0F, sample.forward) + std::abs(sample.right) * 0.35F;
}

} // namespace

auto find_body_impact_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids) -> BodyImpactContact {
  BodyImpactContact contact;
  contact.attacker_id = attacker.get_id();

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr || definition.weapon_family != WeaponFamily::Mount) {
    return contact;
  }

  auto consider = [&](Engine::Core::Entity* candidate,
                      float& best_score,
                      BodyImpactContact& best_contact) {
    if (candidate == nullptr || candidate == &attacker ||
        !Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, candidate, false)) {
      return;
    }
    if (std::find(ignored_target_ids.begin(),
                  ignored_target_ids.end(),
                  candidate->get_id()) != ignored_target_ids.end()) {
      return;
    }

    auto const sample = make_local_body_sample(attacker, *candidate);
    float const sample_radius =
        std::isfinite(sample.radius) ? std::max(0.0F, sample.radius) : 0.0F;
    float const target_radius = std::min(sample_radius, definition.hit_shape.radius);
    float const reach = definition.hit_shape.reach + target_radius;
    float const lateral_limit = definition.hit_shape.radius + target_radius;
    if (sample.entity == nullptr || sample.forward < -target_radius ||
        sample.forward > reach || std::abs(sample.right) > lateral_limit) {
      return;
    }

    float const score = body_contact_score(sample, definition);
    if (!std::isfinite(score) || score >= best_score) {
      return;
    }

    best_score = score;
    best_contact.target_id = candidate->get_id();
    best_contact.contact_point = sample.world_position;
    best_contact.distance = sample.distance;
    best_contact.local_forward = sample.forward;
    best_contact.local_right = sample.right;
  };

  float best_score = std::numeric_limits<float>::infinity();
  if (target_hint_id != 0) {
    consider(world.get_entity(target_hint_id), best_score, contact);
    if (contact.target_id != 0) {
      return contact;
    }
  }

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    consider(candidate, best_score, contact);
  }

  return contact;
}

} // namespace Game::Systems::CombatActions
