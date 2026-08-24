#pragma once

#include <QVariantList>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "game/core/entity.h"

namespace App::Core {

struct CombatHitFeedback {
  Engine::Core::EntityID target = Engine::Core::NULL_ENTITY;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  int damage = 0;
  float damage_ratio = 0.0F;
  bool killing_blow = false;
  bool incoming = false;
  bool outgoing = false;
  bool focused = false;
  int lane = 0;
  int hits = 1;
  float age = 0.0F;
};

class CombatFeedbackStore {
public:
  struct Limits {
    std::size_t max_pending = 32;
    float coalesce_window = 0.12F;
  };

  CombatFeedbackStore() = default;
  explicit CombatFeedbackStore(Limits limits)
      : m_limits(limits) {}

  void push(CombatHitFeedback hit);

  void update(float dt);

  [[nodiscard]] auto pop_ready() -> std::vector<CombatHitFeedback>;

  [[nodiscard]] auto pending() const -> std::vector<CombatHitFeedback>;

  void clear();

  [[nodiscard]] static auto priority(const CombatHitFeedback& hit) -> float;

  [[nodiscard]] static auto
  to_variant(const std::vector<CombatHitFeedback>& hits) -> QVariantList;

private:
  mutable std::mutex m_mutex;
  Limits m_limits;
  std::vector<CombatHitFeedback> m_pending;
  std::uint32_t m_sequence = 0;
};

} // namespace App::Core
