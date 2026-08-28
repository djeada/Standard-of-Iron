#pragma once

#include <QVariantList>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "game/core/entity.h"

namespace App::Core {

enum class FeedbackKind : std::uint8_t {
  Damage = 0,
  Heal,
  Resource,
  Population,
  Count,
};

inline constexpr std::size_t k_feedback_kind_count =
    static_cast<std::size_t>(FeedbackKind::Count);

enum class FeedbackStyle : std::uint8_t {
  Tick = 0,
  Burst,
};

[[nodiscard]] auto feedback_kind_key(FeedbackKind kind) -> const char*;
[[nodiscard]] auto feedback_style_key(FeedbackStyle style) -> const char*;

struct WorldFeedbackTick {
  Engine::Core::EntityID anchor = Engine::Core::NULL_ENTITY;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;

  FeedbackKind kind = FeedbackKind::Damage;
  FeedbackStyle style = FeedbackStyle::Tick;

  int amount = 0;
  float severity = 0.0F;

  int resource = -1;
  int paired_resource = -1;
  int paired_amount = 0;

  bool killing_blow = false;
  bool incoming = false;
  bool outgoing = false;
  bool focused = false;

  int lane = 0;
  int hits = 1;
  float age = 0.0F;
};

class WorldFeedbackStore {
public:
  struct Limits {
    std::size_t max_pending_per_kind = 32;
    float damage_coalesce_window = 0.12F;
    float economy_coalesce_window = 0.45F;
  };

  WorldFeedbackStore() = default;
  explicit WorldFeedbackStore(Limits limits)
      : m_limits(limits) {}

  void push(WorldFeedbackTick tick);

  void update(float dt);

  [[nodiscard]] auto pop_ready() -> std::vector<WorldFeedbackTick>;

  [[nodiscard]] auto pending() const -> std::vector<WorldFeedbackTick>;

  void clear();

  [[nodiscard]] static auto priority(const WorldFeedbackTick& tick) -> float;

  [[nodiscard]] auto coalesce_window(FeedbackKind kind) const -> float;

  [[nodiscard]] static auto
  to_variant(const std::vector<WorldFeedbackTick>& ticks) -> QVariantList;

private:
  [[nodiscard]] auto merge_into_pending(const WorldFeedbackTick& tick) -> bool;
  [[nodiscard]] auto make_room_for(const WorldFeedbackTick& tick) -> bool;

  mutable std::mutex m_mutex;
  Limits m_limits;
  std::vector<WorldFeedbackTick> m_pending;
  std::uint32_t m_sequence = 0;
};

} // namespace App::Core
