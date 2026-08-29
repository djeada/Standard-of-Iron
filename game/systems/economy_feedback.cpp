#include "economy_feedback.h"

#include "../core/event_manager.h"

namespace Game::Systems {

void publish_resource_feedback(int owner_id,
                               Engine::Core::EntityID anchor,
                               ResourceType type,
                               int amount) {
  if (owner_id <= 0 || amount == 0) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::EconomyFeedbackEvent::make_resource(
          owner_id, anchor, type, amount));
}

void publish_resource_feedback_at(
    int owner_id, float x, float y, float z, ResourceType type, int amount) {
  if (owner_id <= 0 || amount == 0) {
    return;
  }
  auto event = Engine::Core::EconomyFeedbackEvent::make_resource(
      owner_id, Engine::Core::NULL_ENTITY, type, amount);
  event.at(x, y, z);
  Engine::Core::EventManager::instance().publish(event);
}

void publish_resource_bundle(int owner_id,
                             Engine::Core::EntityID anchor,
                             const ResourceAmounts& amounts,
                             int sign) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = amounts.get(type);
    if (amount > 0) {
      publish_resource_feedback(owner_id, anchor, type, sign * amount);
    }
  }
}

void publish_resource_bundle_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& amounts, int sign) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = amounts.get(type);
    if (amount > 0) {
      publish_resource_feedback_at(owner_id, x, y, z, type, sign * amount);
    }
  }
}

void publish_trade_feedback(int owner_id,
                            Engine::Core::EntityID anchor,
                            ResourceType spent_type,
                            int spent_amount,
                            ResourceType gained_type,
                            int gained_amount) {
  if (owner_id <= 0 || (spent_amount == 0 && gained_amount == 0)) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::EconomyFeedbackEvent::make_trade(
          owner_id, anchor, spent_type, spent_amount, gained_type, gained_amount));
}

void publish_population_feedback(int owner_id,
                                 Engine::Core::EntityID anchor,
                                 int amount) {
  if (owner_id <= 0 || amount == 0) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::EconomyFeedbackEvent::make_reserve(owner_id, anchor, amount));
}

} // namespace Game::Systems
