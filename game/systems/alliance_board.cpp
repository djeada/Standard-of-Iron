#include "alliance_board.h"

#include <cstddef>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

constexpr std::size_t k_queue_cap = 32;

template <typename T>
void push_capped(std::vector<T>& queue, const T& item) {
  if (queue.size() < k_queue_cap) {
    queue.push_back(item);
  }
}

template <typename T>
auto take_all(std::vector<T>& queue) -> std::vector<T> {
  std::vector<T> taken;
  taken.swap(queue);
  return taken;
}

} // namespace

void AllianceBoard::queue_call(const AllyCallRequest& request) {
  push_capped(m_calls, request);
}

auto AllianceBoard::take_calls_for(int ally) -> std::vector<AllyCallRequest> {
  std::vector<AllyCallRequest> taken;
  std::erase_if(m_calls, [&](const AllyCallRequest& request) {
    if (request.ally != ally) {
      return false;
    }
    taken.push_back(request);
    return true;
  });
  return taken;
}

void AllianceBoard::record_call_answer(const AllyCallAnswer& answer) {
  push_capped(m_call_answers, answer);
}

auto AllianceBoard::take_call_answers() -> std::vector<AllyCallAnswer> {
  return take_all(m_call_answers);
}

void AllianceBoard::record_plea(const AllyPlea& plea) {
  push_capped(m_pleas, plea);
}

auto AllianceBoard::take_pleas() -> std::vector<AllyPlea> {
  return take_all(m_pleas);
}

void AllianceBoard::clear() {
  m_last_call_id = 0;
  m_calls.clear();
  m_call_answers.clear();
  m_pleas.clear();
}

auto ally_call_kind_key(AllyCallKind kind) -> const char* {
  return kind == AllyCallKind::Attack ? "attack" : "defend";
}

auto ai_allies_of(const OwnerRegistry& owners, int owner_id) -> std::vector<int> {
  std::vector<int> allies;
  for (const int ally : owners.get_allies_of(owner_id)) {
    if (owners.is_ai(ally)) {
      allies.push_back(ally);
    }
  }
  return allies;
}

auto check_ally_call(Engine::Core::World& world,
                     const OwnerRegistry& owners,
                     int requester,
                     Engine::Core::EntityID target,
                     AllyCallKind kind) -> AllyCallProblem {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(target);
  if (unit == nullptr || unit->health <= 0) {
    return AllyCallProblem::NoTarget;
  }
  if (!Game::Units::is_building_spawn(unit->spawn_type)) {
    return AllyCallProblem::NotAStructure;
  }
  const bool friendly =
      unit->owner_id == requester || owners.are_allies(requester, unit->owner_id);
  const bool hostile = owners.are_enemies(requester, unit->owner_id);
  if ((kind == AllyCallKind::Defend && !friendly) ||
      (kind == AllyCallKind::Attack && !hostile)) {
    return AllyCallProblem::WrongSide;
  }
  if (ai_allies_of(owners, requester).empty()) {
    return AllyCallProblem::NoAiAllies;
  }
  return AllyCallProblem::None;
}

} // namespace Game::Systems
