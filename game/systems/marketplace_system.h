#pragma once

#include <cstdint>
#include <vector>

#include "../core/entity_id.h"
#include "resource_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct MarketplaceTradeRates {
  int buy_price_food = 10;
  int buy_price_wood = 12;
  int buy_price_stone = 15;
  int buy_price_iron = 20;
  int sell_price_food = 5;
  int sell_price_wood = 6;
  int sell_price_stone = 8;
  int sell_price_iron = 12;
  int trade_quantity = 10;
};

struct AllyTributeRequest {
  int requester = 0;
  int giver = 0;
  ResourceType resource = ResourceType::Wood;
  int amount = 0;
};

enum class AllyTributeVerdict : std::uint8_t {
  Sent,
  Granted,
  Partial,
  RefusedShort,
  RefusedStingy,
};

struct AllyTributeAnswer {
  int requester = 0;
  int giver = 0;
  ResourceType resource = ResourceType::Wood;
  int requested = 0;
  int granted = 0;
  AllyTributeVerdict verdict = AllyTributeVerdict::Sent;
};

inline constexpr int k_max_ally_tribute = 500;

class MarketplaceSystem {
public:
  MarketplaceSystem() = default;
  ~MarketplaceSystem() = default;
  MarketplaceSystem(const MarketplaceSystem&) = delete;
  MarketplaceSystem(MarketplaceSystem&&) = delete;
  auto operator=(const MarketplaceSystem&) -> MarketplaceSystem& = delete;
  auto operator=(MarketplaceSystem&&) -> MarketplaceSystem& = delete;

  static auto instance() -> MarketplaceSystem&;

  [[nodiscard]] auto get_rates() const -> const MarketplaceTradeRates&;

  [[nodiscard]] static auto find_marketplace(const Engine::Core::World& world,
                                             int owner_id) -> Engine::Core::EntityID;

  [[nodiscard]] static auto owner_has_marketplace(const Engine::Core::World& world,
                                                  int owner_id) -> bool;

  [[nodiscard]] auto can_buy(const Engine::Core::World& world,
                             int owner_id,
                             ResourceType resource) const -> bool;
  [[nodiscard]] auto can_sell(const Engine::Core::World& world,
                              int owner_id,
                              ResourceType resource) const -> bool;

  auto buy_resource(const Engine::Core::World& world,
                    int owner_id,
                    ResourceType resource) -> bool;
  auto sell_resource(const Engine::Core::World& world,
                     int owner_id,
                     ResourceType resource) -> bool;

  auto send_to_ally(const Engine::Core::World& world,
                    int from_owner,
                    int to_owner,
                    ResourceType resource,
                    int amount) -> int;

  void queue_ally_request(const AllyTributeRequest& request);
  [[nodiscard]] auto
  take_ally_requests_for(int giver) -> std::vector<AllyTributeRequest>;

  void record_ally_answer(const AllyTributeAnswer& answer);
  [[nodiscard]] auto take_ally_answers() -> std::vector<AllyTributeAnswer>;

  void clear_ally_exchange();

private:
  MarketplaceTradeRates m_rates;
  std::vector<AllyTributeRequest> m_ally_requests;
  std::vector<AllyTributeAnswer> m_ally_answers;
};

} // namespace Game::Systems
