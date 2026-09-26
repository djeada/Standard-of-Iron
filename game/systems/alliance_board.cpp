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

void AllianceBoard::record_appeal(const AllyAppeal& appeal) {
  if (m_open_appeals.size() >= k_queue_cap) {
    return;
  }
  m_open_appeals.push_back(appeal);
  push_capped(m_new_appeals, appeal);
}

auto AllianceBoard::take_new_appeals() -> std::vector<AllyAppeal> {
  return take_all(m_new_appeals);
}

auto AllianceBoard::open_appeal(std::uint32_t appeal_id) const -> const AllyAppeal* {
  for (const auto& appeal : m_open_appeals) {
    if (appeal.appeal_id == appeal_id) {
      return &appeal;
    }
  }
  return nullptr;
}

void AllianceBoard::close_appeal(std::uint32_t appeal_id) {
  std::erase_if(m_open_appeals, [appeal_id](const AllyAppeal& appeal) {
    return appeal.appeal_id == appeal_id;
  });
}

void AllianceBoard::record_appeal_answer(const AllyAppealAnswer& answer) {
  push_capped(m_appeal_answers, answer);
}

auto AllianceBoard::take_appeal_answers_for(int ally) -> std::vector<AllyAppealAnswer> {
  std::vector<AllyAppealAnswer> taken;
  std::erase_if(m_appeal_answers, [&](const AllyAppealAnswer& answer) {
    if (answer.from_ally != ally) {
      return false;
    }
    taken.push_back(answer);
    return true;
  });
  return taken;
}

void AllianceBoard::record_appeal_reply(const AllyAppealReply& reply) {
  push_capped(m_appeal_replies, reply);
}

auto AllianceBoard::take_appeal_replies() -> std::vector<AllyAppealReply> {
  return take_all(m_appeal_replies);
}

void AllianceBoard::clear() {
  m_last_call_id = 0;
  m_calls.clear();
  m_call_answers.clear();
  m_last_appeal_id = 0;
  m_new_appeals.clear();
  m_open_appeals.clear();
  m_appeal_answers.clear();
  m_appeal_replies.clear();
}

auto ally_call_kind_key(AllyCallKind kind) -> const char* {
  return kind == AllyCallKind::Attack ? "attack" : "defend";
}

auto ally_appeal_kind_key(AllyAppealKind kind) -> const char* {
  switch (kind) {
  case AllyAppealKind::Defend:
    return "defend";
  case AllyAppealKind::Attack:
    return "attack";
  case AllyAppealKind::Resources:
    break;
  }
  return "resources";
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
