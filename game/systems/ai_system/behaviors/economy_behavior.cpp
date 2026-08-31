#include "economy_behavior.h"

#include <algorithm>
#include <array>
#include <utility>

#include "../../marketplace_system.h"
#include "../../nation_registry.h"
#include "../../production_service.h"
#include "systems/ai_system/ai_types.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_trade_interval = 4.0F;

constexpr int k_lots_per_visit = 6;

constexpr int k_gold_reserve = 60;

constexpr int k_gold_comfortable = 200;

constexpr int k_glut = 320;

constexpr int k_war_chest_multiple = 3;

struct Appetite {
  ResourceType type;

  int comfortable;
};

auto larder() -> std::array<Appetite, 4> {
  return {Appetite{ResourceType::Food, 260},
          Appetite{ResourceType::Wood, 220},
          Appetite{ResourceType::Stone, 180},
          Appetite{ResourceType::Iron, 140}};
}

auto buy_price(const MarketplaceTradeRates& rates, ResourceType type) -> int {
  switch (type) {
  case ResourceType::Food:
    return rates.buy_price_food;
  case ResourceType::Wood:
    return rates.buy_price_wood;
  case ResourceType::Stone:
    return rates.buy_price_stone;
  case ResourceType::Iron:
    return rates.buy_price_iron;
  default:
    return 0;
  }
}

auto sell_price(const MarketplaceTradeRates& rates, ResourceType type) -> int {
  switch (type) {
  case ResourceType::Food:
    return rates.sell_price_food;
  case ResourceType::Wood:
    return rates.sell_price_wood;
  case ResourceType::Stone:
    return rates.sell_price_stone;
  case ResourceType::Iron:
    return rates.sell_price_iron;
  default:
    return 0;
  }
}

auto war_chest(const AIContext& context) -> ResourceAmounts {

  ResourceAmounts wanted{};
  if (context.nation == nullptr) {
    return wanted;
  }

  bool found = false;
  for (const auto& troop : context.nation->available_troops) {
    if (Game::Units::is_commander_troop(troop.unit_type) ||
        troop.unit_type == Game::Units::TroopType::Builder ||
        troop.unit_type == Game::Units::TroopType::Civilian ||
        Game::Systems::recruiting_building_for(troop.unit_type) !=
            Game::Units::SpawnType::Barracks) {
      continue;
    }
    for (const auto type : k_all_resource_types) {
      if (type == ResourceType::Gold) {
        continue;
      }
      const int cost = troop.resource_costs.get(type);
      wanted.set(type, found ? std::min(wanted.get(type), cost) : cost);
    }
    found = true;
  }

  for (const auto type : k_all_resource_types) {
    wanted.set(type, wanted.get(type) * k_war_chest_multiple);
  }
  return wanted;
}

void order_trade(ResourceType type, bool purchase, std::vector<AICommand>& out) {
  AICommand trade;
  trade.type = AICommandType::TradeResource;
  trade.trade_resource = type;
  trade.trade_is_purchase = purchase;
  out.push_back(std::move(trade));
}

} // namespace

void EconomyBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  m_trade_timer += delta_time;
  if (m_trade_timer < k_trade_interval) {
    return;
  }
  m_trade_timer = 0.0F;

  if (context.marketplace_count <= 0 || !snapshot.has_resource_snapshot) {
    return;
  }

  const MarketplaceTradeRates rates;
  int purse = snapshot.resources.get(ResourceType::Gold);
  int lots = 0;

  ResourceAmounts stock;
  for (const auto type : k_all_resource_types) {
    stock.set(type, snapshot.resources.get(type));
  }

  const auto buy_toward = [&](ResourceType type, int target, int floor_gold) {
    const int price = buy_price(rates, type);
    while (lots < k_lots_per_visit && price > 0 && stock.get(type) < target &&
           purse - price >= floor_gold) {
      order_trade(type, true, out_commands);
      purse -= price;
      stock.set(type, stock.get(type) + rates.trade_quantity);
      ++lots;
    }
  };

  const ResourceAmounts recruiting = war_chest(context);
  std::array<ResourceType, 4> by_urgency{
      ResourceType::Iron, ResourceType::Wood, ResourceType::Food, ResourceType::Stone};
  std::stable_sort(by_urgency.begin(),
                   by_urgency.end(),
                   [&recruiting, &stock](ResourceType lhs, ResourceType rhs) {
                     const auto shortfall = [&](ResourceType type) {
                       return recruiting.get(type) - stock.get(type);
                     };
                     return shortfall(lhs) > shortfall(rhs);
                   });

  for (const auto type : by_urgency) {

    buy_toward(type, recruiting.get(type), k_gold_reserve);
  }

  auto wants = larder();
  std::stable_sort(
      wants.begin(), wants.end(), [&stock](const Appetite& lhs, const Appetite& rhs) {
        const float left = static_cast<float>(stock.get(lhs.type)) /
                           static_cast<float>(lhs.comfortable);
        const float right = static_cast<float>(stock.get(rhs.type)) /
                            static_cast<float>(rhs.comfortable);
        return left < right;
      });
  for (const auto& want : wants) {

    buy_toward(want.type, want.comfortable, k_gold_comfortable);
    if (lots >= k_lots_per_visit) {
      break;
    }
  }

  if (lots > 0) {
    return;
  }

  ResourceType glut = ResourceType::Count;
  int glut_stock = k_glut;
  for (const auto& want : wants) {
    const int held = stock.get(want.type);
    if (held > glut_stock && held > recruiting.get(want.type) &&
        sell_price(rates, want.type) > 0) {
      glut_stock = held;
      glut = want.type;
    }
  }
  if (glut == ResourceType::Count) {
    return;
  }
  order_trade(glut, false, out_commands);
}

auto EconomyBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }
  return context.marketplace_count > 0 && snapshot.has_resource_snapshot;
}

} // namespace Game::Systems::AI
