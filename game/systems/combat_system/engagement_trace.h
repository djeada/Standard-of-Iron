#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "../../core/entity.h"

namespace Game::Systems::Combat {

enum class EngagementOutcome : std::uint8_t {

  Engaged,

  NoCombatRole,

  Busy,

  HoldingTarget,

  Suppressed,

  NoCandidateInRange,

  CandidateUnreachable,

  AssistedAlly,

  Retaliated,
};

enum class CommandSource : std::uint8_t {

  Auto,

  PlayerOrder,

  AIOrder,
};

struct EngagementRecord {

  Engine::Core::EntityID candidate_id{0};

  Engine::Core::EntityID target_id{0};

  float acquisition_range{0.0F};

  EngagementOutcome outcome{EngagementOutcome::NoCandidateInRange};

  CommandSource source{CommandSource::Auto};
};

[[nodiscard]] auto
engagement_outcome_key(EngagementOutcome outcome) -> std::string_view;

[[nodiscard]] auto command_source_key(CommandSource source) -> std::string_view;

[[nodiscard]] auto
command_source_of(const Engine::Core::Entity* entity) -> CommandSource;

class EngagementTrace {
public:
  [[nodiscard]] static auto instance() -> EngagementTrace&;

  [[nodiscard]] auto enabled() const -> bool { return m_enabled; }

  void set_enabled(bool enabled);

  void record(const Engine::Core::Entity* entity, const EngagementRecord& record);

  [[nodiscard]] auto
  find(Engine::Core::EntityID entity_id) const -> const EngagementRecord*;

  void clear();

private:
  EngagementTrace();

  bool m_enabled{false};
  bool m_log_to_console{false};
  std::unordered_map<Engine::Core::EntityID, EngagementRecord> m_records;
};

void note_engagement(const Engine::Core::Entity* entity,
                     const EngagementRecord& record);

} // namespace Game::Systems::Combat
