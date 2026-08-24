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

constexpr float k_work_form_seconds = 1.1F;
constexpr float k_work_break_seconds = 0.8F;

auto to_phase(std::uint8_t raw) -> LayoutPhase {
  return raw <= static_cast<std::uint8_t>(LayoutPhase::Breaking)
             ? static_cast<LayoutPhase>(raw)
             : LayoutPhase::Formed;
}

[[nodiscard]] auto holds_defensive_layout(const Engine::Core::Entity& entity,
                                          const Engine::Core::UnitComponent& unit,
                                          Game::Units::TroopType troop) -> bool {
  const auto* guard = entity.get_component<Engine::Core::GuardModeComponent>();
  if (guard == nullptr || !guard->active) {
    return false;
  }
  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(unit.nation_id);
  if (nation == nullptr || !nation->defensive_unit_layout.has_value()) {
    return false;
  }
  return nation->defensive_unit_layout->is_eligible_troop(troop);
}

[[nodiscard]] auto is_working_on_site(const Engine::Core::Entity& entity) -> bool {
  const auto* builder =
      entity.get_component<Engine::Core::BuilderProductionComponent>();
  if (builder == nullptr) {
    return false;
  }

  return builder->in_progress && builder->at_construction_site;
}

[[nodiscard]] auto transition_seconds_for(const Engine::Core::UnitComponent* unit,
                                          UnitLayoutState from,
                                          UnitLayoutState to) -> float {
  const auto* nation =
      unit != nullptr
          ? Game::Systems::NationRegistry::instance().get_nation(unit->nation_id)
          : nullptr;
  const auto* profile = nation != nullptr && nation->defensive_unit_layout.has_value()
                            ? &nation->defensive_unit_layout.value()
                            : nullptr;
  if (to == UnitLayoutState::Defensive && profile != nullptr) {
    return profile->form_seconds;
  }
  if (from == UnitLayoutState::Defensive && profile != nullptr) {
    return profile->break_seconds;
  }
  return UnitLayoutStateSystem::transition_seconds_for(from, to);
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

  if (is_working_on_site(entity)) {
    return UnitLayoutState::Working;
  }

  if (const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
      unit != nullptr) {
    if (auto const troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
        troop.has_value() && holds_defensive_layout(entity, *unit, *troop)) {
      return UnitLayoutState::Defensive;
    }
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
  if (from == UnitLayoutState::Working) {
    return k_work_break_seconds;
  }
  if (to == UnitLayoutState::Working) {
    return k_work_form_seconds;
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

auto UnitLayoutStateSystem::layout_blend(const Engine::Core::Entity& entity)
    -> LayoutBlend {
  const auto* layout = entity.get_component<Engine::Core::UnitLayoutStateComponent>();
  if (layout == nullptr) {
    return {};
  }

  LayoutBlend blend;
  float const progress = std::clamp(layout->transition_progress, 0.0F, 1.0F);
  auto const phase = to_phase(layout->phase);
  blend.formed_ratio = phase == LayoutPhase::Breaking ? 1.0F - progress : progress;
  if (phase != LayoutPhase::Formed) {
    blend.blend_from = layout->previous_layout_id;
    blend.blend_ratio = progress;
  }
  return blend;
}

auto UnitLayoutStateSystem::formed_ratio(const Engine::Core::Entity& entity) -> float {
  return layout_blend(entity).formed_ratio;
}

void UnitLayoutStateSystem::mark_disrupted(Engine::Core::Entity& entity) {
  auto* layout =
      Engine::Core::get_or_add_component<Engine::Core::UnitLayoutStateComponent>(
          &entity);
  if (layout == nullptr) {
    return;
  }
  layout->phase = static_cast<std::uint8_t>(LayoutPhase::Disrupted);
  layout->previous_layout_id = layout->layout_id;
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

    if (wanted == UnitLayoutState::Defensive) {
      if (auto* transform = entity.get_component<Engine::Core::TransformComponent>()) {
        if (current != UnitLayoutState::Defensive || !transform->has_desired_yaw) {
          transform->desired_yaw = transform->rotation.y;
          transform->has_desired_yaw = true;
        }
      }
    }

    if (layout->layout_id == k_invalid_layout) {
      if (wanted != UnitLayoutState::Defensive) {
        layout->state = static_cast<std::uint8_t>(wanted);
        layout->layout_id = wanted_layout;
        layout->requested_layout_id = wanted_layout;
        layout->previous_layout_id = wanted_layout;
        layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
        layout->transition_progress = 1.0F;
        layout->transition_seconds = 0.0F;
        return;
      }

      auto const normal_layout =
          select_unit_layout(doctrine, *troop, UnitLayoutState::Normal);
      layout->state = static_cast<std::uint8_t>(UnitLayoutState::Normal);
      layout->layout_id = normal_layout;
      layout->requested_layout_id = normal_layout;
      layout->previous_layout_id = normal_layout;
      layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
      layout->transition_progress = 1.0F;
      layout->transition_seconds = 0.0F;
    }

    if (wanted_layout != layout->requested_layout_id) {
      layout->previous_layout_id = layout->layout_id;
      layout->requested_layout_id = wanted_layout;
      layout->transition_seconds =
          Game::Formation::transition_seconds_for(unit, current, wanted);
      layout->transition_progress = 0.0F;
      layout->state = static_cast<std::uint8_t>(wanted);
      if (layout->transition_seconds <= 0.0F) {
        layout->layout_id = wanted_layout;
        layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
        layout->transition_progress = 1.0F;
      } else if (current == UnitLayoutState::Defensive ||
                 current == UnitLayoutState::Braced) {
        layout->phase = static_cast<std::uint8_t>(LayoutPhase::Breaking);
      } else {
        layout->layout_id = wanted_layout;
        layout->phase = static_cast<std::uint8_t>(LayoutPhase::Forming);
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

    if (layout->transition_progress >= 1.0F) {
      layout->layout_id = layout->requested_layout_id;
      layout->phase = static_cast<std::uint8_t>(LayoutPhase::Formed);
      layout->transition_progress = 1.0F;
    }
  });
}

auto UnitLayoutStateSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     MovementComponent,
                                     AttackComponent,
                                     BuilderProductionComponent,
                                     GuardModeComponent,
                                     HoldModeComponent,
                                     MoraleComponent>{},
                               Writes<UnitLayoutStateComponent>{});
}

} // namespace Game::Formation
