#include "builder_ledger.h"

namespace Game::Systems::AI {

auto ConstructionLedger::is_deferred(const char* building_type,
                                     float game_time) const -> bool {
  return building_type != nullptr && building_type == m_deferred_type &&
         game_time < m_deferred_until;
}

void ConstructionLedger::defer(const char* building_type, float until_game_time) {
  m_deferred_type = building_type;
  m_deferred_until = until_game_time;
}

void ConstructionLedger::note_order(const char* building_type,
                                    int building_total,
                                    float game_time,
                                    int plan_slot) {
  constexpr int k_orders_before_giving_up = 8;
  constexpr int k_slot_orders_before_giving_up = 4;

  constexpr float k_defer_seconds = 90.0F;

  if (building_total != m_last_building_total) {
    m_plan_slot_orders.clear();
  }
  if (plan_slot >= 0) {
    const int orders = ++m_plan_slot_orders[plan_slot];
    if (orders >= k_slot_orders_before_giving_up) {
      m_blocked_plan_slots.push_back(plan_slot);
      m_plan_slot_orders.erase(plan_slot);
    }
  }

  if (building_type == m_last_order_type && building_total == m_last_building_total) {
    ++m_last_order_repeats;
  } else {
    m_last_order_type = building_type;
    m_last_building_total = building_total;
    m_last_order_repeats = 1;
  }

  if (m_last_order_repeats >= k_orders_before_giving_up) {

    m_deferred_type = building_type;
    m_deferred_until = game_time + k_defer_seconds;
    m_last_order_repeats = 0;
  }
}

} // namespace Game::Systems::AI
