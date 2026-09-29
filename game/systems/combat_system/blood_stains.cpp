
#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_random.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

namespace {

void prune_oldest_blood_stain(Engine::Core::World* world) {
  if (world == nullptr) {
    return;
  }

  while (true) {
    const auto blood_stains = world->entities_with<Engine::Core::BloodStainComponent>();
    if (blood_stains.size() <
        static_cast<std::size_t>(Engine::Core::Defaults::k_blood_stain_max_active)) {
      return;
    }

    auto const oldest = std::min_element(blood_stains.begin(), blood_stains.end());
    if (oldest == blood_stains.end()) {
      return;
    }

    world->destroy_entity(*oldest);
  }
}

auto blood_stain_scale(const Engine::Core::UnitComponent* unit) -> float {
  if (unit == nullptr) {
    return 1.0F;
  }
  if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return 1.65F;
  }
  if (Game::Units::is_cavalry(unit->spawn_type)) {
    return 1.25F;
  }
  if (Game::Units::is_wildlife_spawn(unit->spawn_type)) {
    return 0.42F;
  }
  return 1.0F;
}

} // namespace

void spawn_blood_stain(Engine::Core::World* world,
                       const Engine::Core::Entity* target,
                       float spread,
                       std::uint32_t variation) {
  if (world == nullptr || target == nullptr) {
    return;
  }

  auto const* transform = target->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  prune_oldest_blood_stain(world);

  auto* blood_stain = world->create_entity();
  if (blood_stain == nullptr) {
    return;
  }

  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  auto const id_seed =
      static_cast<std::uint32_t>(target->get_id()) + (variation * 2654435761U);
  auto const position_seed =
      static_cast<std::uint32_t>(std::abs(transform->position.x) * 31.0F +
                                 std::abs(transform->position.z) * 131.0F);
  float const scale = blood_stain_scale(unit);
  float const radius =
      Engine::Core::Defaults::k_blood_stain_default_radius * scale *
      (0.78F + hash_to_unit_open(id_seed * 17U + position_seed) * 0.46F);
  float const rotation = hash_to_unit_open(id_seed * 97U + position_seed * 3U) *
                         std::numbers::pi_v<float> * 2.0F;
  float const aspect_ratio =
      0.58F + hash_to_unit_open(id_seed * 53U + position_seed * 11U) * 0.72F;
  float const seed = hash_to_unit_open(id_seed * 193U + position_seed * 29U);

  float const offset_angle = hash_to_unit_open(id_seed * 311U + position_seed * 7U) *
                             std::numbers::pi_v<float> * 2.0F;
  float const offset_reach =
      spread * std::sqrt(hash_to_unit_open(id_seed * 419U + position_seed * 13U));

  blood_stain->add_component<Engine::Core::TransformComponent>(
      transform->position.x + (std::cos(offset_angle) * offset_reach),
      transform->position.y,
      transform->position.z + (std::sin(offset_angle) * offset_reach));
  blood_stain->add_component<Engine::Core::BloodStainComponent>(
      radius,
      Engine::Core::Defaults::k_blood_stain_default_lifetime,
      rotation,
      aspect_ratio,
      seed);
}

} // namespace Game::Systems::Combat
