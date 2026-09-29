#pragma once

#include <vector>

#include "../ai_types.h"
#include "builder_ledger.h"
#include "builder_town_plan.h"

namespace Game::Systems::AI {

struct ConstructionIntent {
  const char* type = nullptr;
  float x = 0.0F;
  float z = 0.0F;
  float rotation_y = 0.0F;

  bool site_known = false;

  bool expansion = false;

  int plan_slot = -1;
};

[[nodiscard]] auto gather_construction_intents(
    const AISnapshot& snapshot,
    const AIContext& context,
    const SettlementAssessment& town,
    const std::vector<int>& blocked_plan_slots,
    int construction_counter) -> std::vector<ConstructionIntent>;

struct IntentChoice {
  const ConstructionIntent* chosen = nullptr;

  ResourceType missing_resource = ResourceType::Count;
};

[[nodiscard]] auto
choose_construction_intent(const AISnapshot& snapshot,
                           AIContext& context,
                           const std::vector<ConstructionIntent>& intents,
                           const ConstructionLedger& ledger) -> IntentChoice;

} // namespace Game::Systems::AI
