#include "alliance_board.h"

#include <QJsonObject>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "../util/checkpoint_values.h"
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

template <typename Request>
auto call_fields(Request& call) {
  return std::tie(call.call_id,
                  call.requester,
                  call.ally,
                  call.kind,
                  call.target,
                  call.target_owner,
                  call.target_x,
                  call.target_z);
}

template <typename Appeal>
auto appeal_fields(Appeal& appeal) {
  return std::tie(appeal.appeal_id,
                  appeal.from_ally,
                  appeal.to_owner,
                  appeal.kind,
                  appeal.resource,
                  appeal.amount,
                  appeal.target,
                  appeal.target_x,
                  appeal.target_z);
}

template <typename Answer>
auto answer_fields(Answer& answer) {
  return std::tie(answer.appeal_id,
                  answer.from_ally,
                  answer.answerer,
                  answer.accepted,
                  answer.given);
}

template <typename Row, typename Fields>
auto write_queue(const std::vector<Row>& entries, Fields fields) -> QJsonArray {
  QJsonArray rows;
  for (const auto& entry : entries) {
    rows.append(Game::CheckpointValues::write_tuple(fields(entry)));
  }
  return rows;
}

template <typename Row, typename Fields>
auto read_queue(const QJsonValue& value,
                std::vector<Row>& entries,
                Fields fields) -> bool {
  if (!value.isArray()) {
    return false;
  }
  const auto rows = value.toArray();
  if (rows.size() > static_cast<qsizetype>(k_queue_cap)) {
    return false;
  }
  for (const auto row : rows) {
    if (!Game::CheckpointValues::read_tuple(row, fields(entries.emplace_back()))) {
      return false;
    }
  }
  return true;
}

} // namespace

void AllianceBoard::queue_call(const AllyCallRequest& request) {
  push_capped(m_calls, request);
}

auto AllianceBoard::calls_for(int ally) const -> std::vector<AllyCallRequest> {
  std::vector<AllyCallRequest> calls;
  std::ranges::copy_if(m_calls, std::back_inserter(calls), [ally](const auto& call) {
    return call.ally == ally;
  });
  return calls;
}

auto AllianceBoard::remove_call(std::uint32_t call_id, int ally) -> bool {
  return std::erase_if(m_calls, [=](const auto& call) {
           return call.call_id == call_id && call.ally == ally;
         }) != 0;
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
  m_last_appeal_id = std::max(m_last_appeal_id, appeal.appeal_id);
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

auto AllianceBoard::appeal_answers_for(int ally) const
    -> std::vector<AllyAppealAnswer> {
  std::vector<AllyAppealAnswer> answers;
  std::ranges::copy_if(m_appeal_answers,
                       std::back_inserter(answers),
                       [ally](const auto& answer) { return answer.from_ally == ally; });
  return answers;
}

auto AllianceBoard::remove_appeal_answer(std::uint32_t appeal_id, int ally) -> bool {
  return std::erase_if(m_appeal_answers, [=](const auto& answer) {
           return answer.appeal_id == appeal_id && answer.from_ally == ally;
         }) != 0;
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

auto AllianceBoard::goodwill(int ally, int owner) const -> float {
  const auto found = m_goodwill.find({ally, owner});
  return found != m_goodwill.end() ? found->second : 0.0F;
}

void AllianceBoard::change_goodwill(int ally, int owner, float delta) {
  auto& value = m_goodwill[{ally, owner}];
  value = std::clamp(value + delta, -1.0F, 1.0F);
}

auto AllianceBoard::capture_checkpoint() const -> QJsonObject {
  using Game::CheckpointValues::write;
  QJsonArray goodwill;
  for (const auto& [owners, value] : m_goodwill) {
    goodwill.append(write(owners.first, owners.second, value));
  }
  return {
      {"sequences", write(m_last_call_id, m_last_appeal_id)},
      {"calls", write_queue(m_calls, call_fields<const AllyCallRequest>)},
      {"appeals", write_queue(m_open_appeals, appeal_fields<const AllyAppeal>)},
      {"answers", write_queue(m_appeal_answers, answer_fields<const AllyAppealAnswer>)},
      {"goodwill", goodwill}};
}

auto AllianceBoard::restore_checkpoint(const QJsonObject& state) -> bool {
  using Game::CheckpointValues::read;
  AllianceBoard restored;
  if (!read(state.value("sequences"),
            restored.m_last_call_id,
            restored.m_last_appeal_id) ||
      !read_queue(
          state.value("calls"), restored.m_calls, call_fields<AllyCallRequest>) ||
      !read_queue(
          state.value("appeals"), restored.m_open_appeals, appeal_fields<AllyAppeal>) ||
      !read_queue(state.value("answers"),
                  restored.m_appeal_answers,
                  answer_fields<AllyAppealAnswer>) ||
      !state.value("goodwill").isArray()) {
    return false;
  }
  for (const auto row : state.value("goodwill").toArray()) {
    int ally = 0;
    int owner = 0;
    float value = 0.0F;
    if (!read(row, ally, owner, value) || value < -1.0F || value > 1.0F ||
        !restored.m_goodwill.emplace(std::pair{ally, owner}, value).second) {
      return false;
    }
  }
  restored.m_epoch = m_epoch;
  *this = std::move(restored);
  return true;
}

void AllianceBoard::clear() {
  ++m_epoch;
  m_calls.clear();
  m_call_answers.clear();
  m_new_appeals.clear();
  m_open_appeals.clear();
  m_appeal_answers.clear();
  m_appeal_replies.clear();
  m_goodwill.clear();
  // Call IDs are per match. Appeal IDs must outlive a board reset for the UI.
  m_last_call_id = 0;
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
