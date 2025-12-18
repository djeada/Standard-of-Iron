#include "production_behavior.h"
#include "../../../core/ownership_constants.h"
#include "../../nation_registry.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <utility>
#include <vector>

namespace Game::Systems::AI {

void ProductionBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                 float delta_time,
                                 std::vector<AICommand> &outCommands) {
  m_productionTimer += delta_time;
  if (m_productionTimer < 1.5F) {
    return;
  }
  m_productionTimer = 0.0F;

  static int const exec_counter = 0;

  auto &nation_registry = Game::Systems::NationRegistry::instance();
  const auto *nation = nation_registry.get_nation_for_player(context.player_id);

  if (nation == nullptr) {

    static int const log_counter = 0;
    return;
  }

  bool produce_ranged = true;

  if (context.barracks_under_threat || context.state == AIState::Defending) {
    produce_ranged = (context.melee_count > context.ranged_count);
  } else {

    float const ranged_ratio =
        (context.total_units > 0)
            ? static_cast<float>(context.ranged_count) / context.total_units
            : 0.0F;
    produce_ranged = (ranged_ratio < 0.6F);
  }

  const Game::Systems::TroopType *troop_type =
      produce_ranged ? nation->get_best_ranged_troop()
                     : nation->get_best_melee_troop();

  if (troop_type == nullptr) {
    troop_type = produce_ranged ? nation->get_best_melee_troop()
                                : nation->get_best_ranged_troop();
  }

  if (troop_type == nullptr) {
    static int const log_counter = 0;
    return;
  }

  for (const auto &entity : snapshot.friendly_units) {
    if (!entity.is_building ||
        entity.spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    if (Game::Core::isNeutralOwner(entity.owner_id)) {
      continue;
    }

    static int const log_counter = 0;

    if (!entity.production.has_component) {
      continue;
    }

    const auto &prod = entity.production;

    if (prod.produced_count >= prod.max_units) {
      continue;
    }

    const int max_queue_size = 5;
    int const total_in_queue = (prod.in_progress ? 1 : 0) + prod.queue_size;
    if (total_in_queue >= max_queue_size) {
      continue;
    }

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.building_id = entity.id;
    command.product_type = troop_type->unit_type;
    outCommands.push_back(std::move(command));

    m_productionCounter++;
  }
}

auto ProductionBehavior::should_execute(
    const AISnapshot &snapshot, const AIContext &context) const -> bool {
  (void)snapshot;

  return context.total_units < context.max_troops_per_player;
}

} 
