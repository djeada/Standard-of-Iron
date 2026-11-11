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

  [[nodiscard]] auto blocked_plan_slots_at(float game_time) const -> std::vector<int>;

  void embargo_plan_slot(int plan_slot, float until_game_time);

  void note_site_order(float x, float z, int plan_slot, float game_time);

  void note_unreachable_site(float observed_x, float observed_z, float game_time);

  [[nodiscard]] auto
  site_was_abandoned(float x, float z, float game_time) const -> bool;

private:
  struct SiteOrder {
    float x = 0.0F;
    float z = 0.0F;
    int plan_slot = -1;
    float ordered_at = 0.0F;
  };

  struct AbandonedSite {
    float x = 0.0F;
    float z = 0.0F;
    float seen_x = 0.0F;
    float seen_z = 0.0F;
    float noted_at = 0.0F;
    float until = 0.0F;
  };

  const char* m_last_order_type = nullptr;
  int m_last_order_repeats = 0;
  int m_last_building_total = -1;

  std::map<std::string, float> m_deferred_until;

  std::map<int, int> m_plan_slot_orders;
  std::vector<int> m_blocked_plan_slots;

  std::map<int, float> m_plan_slot_embargo;
  std::vector<SiteOrder> m_site_orders;
  std::vector<AbandonedSite> m_abandoned_sites;
};

} // namespace Game::Systems::AI
