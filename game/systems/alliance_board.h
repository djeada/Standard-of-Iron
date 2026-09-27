#pragma once

#include <cstdint>
#include <vector>

#include "../core/entity_id.h"
#include "resource_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class OwnerRegistry;

enum class AllyCallKind : std::uint8_t {
  Defend,
  Attack,
};

enum class AllyCallVerdict : std::uint8_t {
  Accepted,
  RefusedUnderThreat,
  RefusedNoArmy,
  RefusedUnwilling,
};

struct AllyCallRequest {
  std::uint32_t call_id = 0;
  int requester = 0;
  int ally = 0;
  AllyCallKind kind = AllyCallKind::Defend;
  Engine::Core::EntityID target = Engine::Core::NULL_ENTITY;
  int target_owner = 0;
  float target_x = 0.0F;
  float target_z = 0.0F;
};

struct AllyCallAnswer {
  std::uint32_t call_id = 0;
  int requester = 0;
  int ally = 0;
  AllyCallKind kind = AllyCallKind::Defend;
  Engine::Core::EntityID target = Engine::Core::NULL_ENTITY;
  AllyCallVerdict verdict = AllyCallVerdict::RefusedUnwilling;
};

inline constexpr float k_ally_pledge_seconds = 90.0F;

enum class AllyAppealKind : std::uint8_t {
  Resources,
  Defend,
  Attack,
};

struct AllyAppeal {
  std::uint32_t appeal_id = 0;
  int from_ally = 0;
  int to_owner = 0;
  AllyAppealKind kind = AllyAppealKind::Resources;
  ResourceType resource = ResourceType::Gold;
  int amount = 0;
  Engine::Core::EntityID target = Engine::Core::NULL_ENTITY;
  float target_x = 0.0F;
  float target_z = 0.0F;
};

struct AllyAppealAnswer {
  std::uint32_t appeal_id = 0;
  int from_ally = 0;
  int answerer = 0;
  bool accepted = false;
  int given = 0;
};

enum class AllyAppealFollowUp : std::uint8_t {
  Grateful,
  AwaitingAid,
  HoldAlone,
  MarchAlone,
  ManageWithout,
  AidArrived,
  AidNeverCame,
  Withdrawn,
};

struct AllyAppealReply {
  std::uint32_t appeal_id = 0;
  int from_ally = 0;
  int to_owner = 0;
  AllyAppealKind kind = AllyAppealKind::Resources;
  AllyAppealFollowUp follow_up = AllyAppealFollowUp::Withdrawn;
  ResourceType resource = ResourceType::Gold;
  int amount = 0;
};

inline constexpr float k_ally_appeal_answer_seconds = 45.0F;

class AllianceBoard {
public:
  auto next_call_id() -> std::uint32_t { return ++m_last_call_id; }

  void queue_call(const AllyCallRequest& request);
  [[nodiscard]] auto take_calls_for(int ally) -> std::vector<AllyCallRequest>;

  void record_call_answer(const AllyCallAnswer& answer);
  [[nodiscard]] auto take_call_answers() -> std::vector<AllyCallAnswer>;

  auto next_appeal_id() -> std::uint32_t { return ++m_last_appeal_id; }
  void record_appeal(const AllyAppeal& appeal);
  [[nodiscard]] auto take_new_appeals() -> std::vector<AllyAppeal>;
  [[nodiscard]] auto open_appeal(std::uint32_t appeal_id) const -> const AllyAppeal*;
  void close_appeal(std::uint32_t appeal_id);

  void record_appeal_answer(const AllyAppealAnswer& answer);
  [[nodiscard]] auto take_appeal_answers_for(int ally) -> std::vector<AllyAppealAnswer>;

  void record_appeal_reply(const AllyAppealReply& reply);
  [[nodiscard]] auto take_appeal_replies() -> std::vector<AllyAppealReply>;

  void clear();

private:
  std::uint32_t m_last_call_id = 0;
  std::vector<AllyCallRequest> m_calls;
  std::vector<AllyCallAnswer> m_call_answers;
  std::uint32_t m_last_appeal_id = 0;
  std::vector<AllyAppeal> m_new_appeals;
  std::vector<AllyAppeal> m_open_appeals;
  std::vector<AllyAppealAnswer> m_appeal_answers;
  std::vector<AllyAppealReply> m_appeal_replies;
};

[[nodiscard]] auto ally_call_kind_key(AllyCallKind kind) -> const char*;
[[nodiscard]] auto ally_appeal_kind_key(AllyAppealKind kind) -> const char*;

enum class AllyCallProblem : std::uint8_t {
  None,
  NoTarget,
  NotAStructure,
  WrongSide,
  NoAiAllies,
};

[[nodiscard]] auto ai_allies_of(const OwnerRegistry& owners,
                                int owner_id) -> std::vector<int>;

[[nodiscard]] auto check_ally_call(Engine::Core::World& world,
                                   const OwnerRegistry& owners,
                                   int requester,
                                   Engine::Core::EntityID target,
                                   AllyCallKind kind) -> AllyCallProblem;

} // namespace Game::Systems
