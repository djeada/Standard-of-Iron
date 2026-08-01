#include "unit_layout_state_system.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/nation_registry.h"
#include "../units/spawn_type.h"
#include "unit_layout_resolver.h"

namespace Game::Formation {

namespace {

constexpr float k_form_seconds = 1.6F;
constexpr float k_break_seconds = 0.9F;
constexpr float k_shuffle_seconds = 0.5F;
constexpr float k_disrupt_seconds = 0.7F;

auto to_phase(std::uint8_t raw) -> LayoutPhase {
  return raw <= static_cast<std::uint8_t>(LayoutPhase::Breaking)
             ? static_cast<LayoutPhase>(raw)
             : LayoutPhase::Formed;
}

auto doctrine_for(const Engine::Core::UnitComponent& unit) -> FormationDoctrineId {
  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(unit.nation_id);
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return default_doctrine_for_nation(unit.nation_id);
}

} // namespace

auto UnitLayoutStateSystem::desired_state(const Engine::Core::Entity& entity)
    -> UnitLayoutState {
  if (const auto* morale = entity.get_component<Engine::Core::MoraleComponent>();
      morale != nullptr && morale->routing) {
    return UnitLayoutState::Routing;
  }

  if (const auto* defense =
          entity.get_component<Engine::Core::DefenseFormationComponent>();
      defense != nullptr && defense->is_engaged()) {
    return UnitLayoutState::Defensive;
  }

  if (const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
      attack != nullptr && attack->in_melee_lock) {
    return UnitLayoutState::Attacking;
  }

  if (const auto* hold = entity.get_component<Engine::Core::HoldModeComponent>();
      hold != nullptr && hold->active) {
    return UnitLayoutState::Braced;
  }

  if (const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
      movement != nullptr && movement->get_has_target()) {
    return UnitLayoutState::Marching;
  }

  return UnitLayoutState::Normal;
}

auto UnitLayoutStateSystem::transition_seconds_for(UnitLayoutState from,
                                                   UnitLayoutState to) -> float {
  if (from == to) {
    return 0.0F;
  }
  if (to == UnitLayoutState::Disrupted || to == UnitLayoutState::Routing) {
    return k_disrupt_seconds;
  }
  if (from == UnitLayoutState::Defensive || from == UnitLayoutState::Braced) {
    return k_break_seconds;
  }
  if (to == UnitLayoutState::Defensive || to == UnitLayoutState::Braced) {
    return k_form_seconds;
  }
  return k_shuffle_seconds;
}

auto UnitLayoutStateSystem::is_layout_formed(const Engine::Core::Entity& entity)
    -> bool {
  const auto* layout = entity.get_component<Engine::Core::UnitLayoutStateComponent>();
  return layout == nullptr || to_phase(layout->phase) == LayoutPhase::Formed;
}

auto UnitLayoutStateSystem::formed_ratio(const Engine::Core::Entity& entity) -> float {
  const auto* layout = entity.get_component<Engine::Core::UnitLayoutStateComponent>();
  if (layout == nullptr) {
    return 1.0F;
  }
  return std::clamp(layout->transition_progress, 0.0F, 1.0F);
}

void UnitLayoutStateSystem::mark_disrupted(Engine::Core::Entity& entity) {
  auto* layout =
      Engine::Core::get_or_add_component<Engine::Core::UnitLayoutStateComponent>(
          &entity);
  if (layout == nullptr) {
    return;
  }
  layout->phase = static_cast<std::uint8_t>(LayoutPhase::Disrupted);
  layout->transition_progress = 0.0F;
  layout->transition_seconds = k_disrupt_seconds;
}

void UnitLayoutStateSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  world->for_each_entity([delta_time](Engine::Core::Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
      return;
    }

    auto* layout =
        Engine::Core::get_or_add_component<Engine::Core::UnitLayoutStateComponent>(
            &entity);
    if (layout == nullptr) {
      return;
    }

    auto const troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop.has_value()) {
      return;
    }

    auto const doctrine = doctrine_for(*unit);
    auto const wanted = desired_state(entity);
    auto const current = static_cast<UnitLayoutState>(layout->state);
    auto const wanted_layout = select_unit_layout(doctrine, *troop, wanted);

    if (layout->layout_id == 0xFFFFU) {
      layout->state = static_cast<std::uint8_t>(wanted);
      layout->layout_id = wanted_layout;
      layout->requested_layout_id = wanted_layout;
      layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
      layout->transition_progress = 1.0F;
      layout->transition_seconds = 0.0F;
      return;
    }

    if (wanted_layout != layout->requested_layout_id) {
      layout->requested_layout_id = wanted_layout;
      layout->transition_seconds = transition_seconds_for(current, wanted);
      layout->transition_progress = 0.0F;
      layout->phase = static_cast<std::uint8_t>(layout->transition_seconds > 0.0F
                                                    ? LayoutPhase::Breaking
                                                    : LayoutPhase::Formed);
      layout->state = static_cast<std::uint8_t>(wanted);
      if (layout->transition_seconds <= 0.0F) {
        layout->layout_id = wanted_layout;
        layout->transition_progress = 1.0F;
      }
      return;
    }

    auto const phase = to_phase(layout->phase);
    if (phase == LayoutPhase::Formed) {
      layout->transition_progress = 1.0F;
      return;
    }

    float const duration = std::max(0.05F, layout->transition_seconds);
    layout->transition_progress =
        std::min(1.0F, layout->transition_progress + (delta_time / duration));

    if (phase == LayoutPhase::Breaking && layout->transition_progress >= 0.5F) {
      layout->layout_id = layout->requested_layout_id;
      layout->phase = static_cast<std::uint8_t>(LayoutPhase::Forming);
      layout->transition_progress = 0.0F;
      return;
    }

    if (layout->transition_progress >= 1.0F) {
      layout->layout_id = layout->requested_layout_id;
      layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
      layout->transition_progress = 1.0F;
    }
  });
}

} // namespace Game::Formation
