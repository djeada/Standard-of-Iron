#include "builder_ledger.h"

#include <algorithm>
#include <cstddef>

namespace Game::Systems::AI {

namespace {

constexpr float k_order_memory_seconds = 240.0F;
constexpr float k_abandoned_memory_seconds = 600.0F;
constexpr float k_abandoned_slot_embargo_seconds = 600.0F;
constexpr float k_nudge_reach = 13.0F;
constexpr float k_abandoned_radius = 4.0F;
constexpr float k_same_report_radius = 1.0F;
constexpr float k_same_report_seconds = 20.0F;
constexpr std::size_t k_remembered_abandoned_sites = 24;
constexpr std::size_t k_remembered_orders = 16;

auto distance_sq(float ax, float az, float bx, float bz) -> float {
  const float dx = ax - bx;
  const float dz = az - bz;
  return (dx * dx) + (dz * dz);
}

} // namespace

auto ConstructionLedger::is_deferred(const char* building_type,
                                     float game_time) const -> bool {
  if (building_type == nullptr) {
    return false;
  }
  const auto found = m_deferred_until.find(building_type);
  return found != m_deferred_until.end() && game_time < found->second;
}

void ConstructionLedger::defer(const char* building_type, float until_game_time) {
  if (building_type != nullptr) {
    m_deferred_until[building_type] = until_game_time;
  }
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

    defer(building_type, game_time + k_defer_seconds);
    m_last_order_repeats = 0;
  }
}

auto ConstructionLedger::blocked_plan_slots_at(float game_time) const
    -> std::vector<int> {
  std::vector<int> blocked = m_blocked_plan_slots;
  for (const auto& [slot, until] : m_plan_slot_embargo) {
    if (game_time < until) {
      blocked.push_back(slot);
    }
  }
  return blocked;
}

void ConstructionLedger::embargo_plan_slot(int plan_slot, float until_game_time) {
  if (plan_slot < 0) {
    return;
  }
  float& until = m_plan_slot_embargo[plan_slot];
  until = std::max(until, until_game_time);
}

void ConstructionLedger::note_site_order(float x,
                                         float z,
                                         int plan_slot,
                                         float game_time) {
  std::erase_if(m_site_orders, [game_time](const SiteOrder& order) {
    return game_time - order.ordered_at > k_order_memory_seconds;
  });
  m_site_orders.push_back(
      {.x = x, .z = z, .plan_slot = plan_slot, .ordered_at = game_time});
  if (m_site_orders.size() > k_remembered_orders) {
    m_site_orders.erase(m_site_orders.begin());
  }
}

void ConstructionLedger::note_unreachable_site(float observed_x,
                                               float observed_z,
                                               float game_time) {
  for (const auto& known : m_abandoned_sites) {
    if (game_time - known.noted_at < k_same_report_seconds &&
        distance_sq(known.seen_x, known.seen_z, observed_x, observed_z) <
            k_same_report_radius * k_same_report_radius) {
      return;
    }
  }

  AbandonedSite lost{.x = observed_x,
                     .z = observed_z,
                     .seen_x = observed_x,
                     .seen_z = observed_z,
                     .noted_at = game_time,
                     .until = game_time + k_abandoned_memory_seconds};

  auto nearest = m_site_orders.end();
  float nearest_sq = k_nudge_reach * k_nudge_reach;
  for (auto order = m_site_orders.begin(); order != m_site_orders.end(); ++order) {
    const float d_sq = distance_sq(order->x, order->z, observed_x, observed_z);
    if (d_sq <= nearest_sq) {
      nearest = order;
      nearest_sq = d_sq;
    }
  }
  if (nearest != m_site_orders.end()) {
    lost.x = nearest->x;
    lost.z = nearest->z;
    embargo_plan_slot(nearest->plan_slot, game_time + k_abandoned_slot_embargo_seconds);
    m_site_orders.erase(nearest);
  }

  m_abandoned_sites.push_back(lost);
  if (m_abandoned_sites.size() > k_remembered_abandoned_sites) {
    m_abandoned_sites.erase(m_abandoned_sites.begin());
  }
}

auto ConstructionLedger::site_was_abandoned(float x,
                                            float z,
                                            float game_time) const -> bool {
  return std::any_of(m_abandoned_sites.begin(),
                     m_abandoned_sites.end(),
                     [&](const AbandonedSite& site) {
                       return game_time < site.until &&
                              (distance_sq(site.x, site.z, x, z) <
                                   k_abandoned_radius * k_abandoned_radius ||
                               distance_sq(site.seen_x, site.seen_z, x, z) <
                                   k_abandoned_radius * k_abandoned_radius);
                     });
}

} // namespace Game::Systems::AI
