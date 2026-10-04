#pragma once

#include <map>
#include <string>
#include <vector>

namespace Game::Systems::AI {

class ConstructionLedger {
public:
  [[nodiscard]] auto is_deferred(const char* building_type,
                                 float game_time) const -> bool;

  void defer(const char* building_type, float until_game_time);

  void note_order(const char* building_type,
                  int building_total,
                  float game_time,
                  int plan_slot = -1);

  [[nodiscard]] auto blocked_plan_slots() const noexcept -> const std::vector<int>& {
    return m_blocked_plan_slots;
  }

private:
  const char* m_last_order_type = nullptr;
  int m_last_order_repeats = 0;
  int m_last_building_total = -1;

  std::map<std::string, float> m_deferred_until;

  std::map<int, int> m_plan_slot_orders;
  std::vector<int> m_blocked_plan_slots;
};

} // namespace Game::Systems::AI
