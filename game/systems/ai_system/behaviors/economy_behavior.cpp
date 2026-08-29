#include "economy_behavior.h"

#include <algorithm>
#include <array>
#include <utility>

#include "../../marketplace_system.h"
#include "../../nation_registry.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_trade_interval = 4.0F;

constexpr int k_lots_per_visit = 6;

constexpr int k_gold_abundant = 400;

constexpr int k_glut = 400;

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

  std::array<Appetite, 4> wants = larder();

  std::stable_sort(wants.begin(),
                   wants.end(),
                   [&snapshot](const Appetite& lhs, const Appetite& rhs) {
                     const float left =
                         static_cast<float>(snapshot.resources.get(lhs.type)) /
                         static_cast<float>(lhs.comfortable);
                     const float right =
                         static_cast<float>(snapshot.resources.get(rhs.type)) /
                         static_cast<float>(rhs.comfortable);
                     return left < right;
                   });

  int lots = 0;
  for (const auto& want : wants) {
    int stock = snapshot.resources.get(want.type);
    const int price = buy_price(rates, want.type);
    while (lots < k_lots_per_visit && stock < want.comfortable && price > 0 &&
           purse - price >= k_gold_abundant) {
      order_trade(want.type, true, out_commands);
      purse -= price;
      stock += rates.trade_quantity;
      ++lots;
    }
    if (lots >= k_lots_per_visit) {
      break;
    }
  }

  if (lots > 0 || purse >= k_gold_abundant) {
    return;
  }

  ResourceType glut = ResourceType::Count;
  int glut_stock = k_glut;
  for (const auto& want : wants) {
    const int stock = snapshot.resources.get(want.type);
    if (stock > glut_stock) {
      glut_stock = stock;
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
