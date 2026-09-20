#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "civilian_actor.h"
#include "render/creature/spec.h"

namespace Engine::Core {
class World;
}
namespace Render::GL {
struct DrawContext;
struct HumanoidVariant;
class ISubmitter;

// The nation's civilian rig wearing the field props. Derived once from
// nation_civilian_rig(), never authored here.
struct FarmWorkerVisual {
  Render::Creature::ArchetypeId hatted_tending{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId hatted_reaping{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId hatted_gathering{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId napping{Render::Creature::k_invalid_archetype};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return hatted_tending != Render::Creature::k_invalid_archetype &&
           hatted_reaping != Render::Creature::k_invalid_archetype;
  }
};

[[nodiscard]] auto farm_worker_visual(bool carthage) -> const FarmWorkerVisual&;

// Owned by a renderer, never stored in the simulation or save/replay state.
struct FarmActivity {
  // One eligible field, resolved once per frame from the render snapshot.
  struct Field {
    std::uint64_t id{0};
    float growth{0.0F};
  };

  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto field(std::uint64_t id) const -> const Field*;
  [[nodiscard]] auto gag(std::uint64_t id, int stage, float time) -> float;

  float cooldown_seconds{600.0F};
  int remaining{0};
  int workers_per_field{0};
  std::vector<Field> fields;
  std::vector<std::uint64_t> claimed;
  Engine::Core::World* world{nullptr};
  float previous_time{0.0F};
  float frame_time{0.0F};
  float gag_started{0.0F};
  std::uint64_t gag_field{0};
  int gag_stage{0};
  bool gag_seen{false};
};

void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);
[[nodiscard]] auto farm_activity_anchor(int index) -> QVector3D;
[[nodiscard]] auto farm_activity_clearing(const QVector3D& point) -> bool;
[[nodiscard]] auto farm_activity_growth_stage(float growth) -> int;

// The lazy-farmer sequence, as one readable timeline. Beat boundaries are in
// seconds from the moment the slot is claimed.
namespace Nap {
inline constexpr float k_sit_down_end = 1.2F;
inline constexpr float k_asleep_end = 4.0F;
inline constexpr float k_get_up_end = 4.7F;
inline constexpr float k_pretend_end = 7.2F;
inline constexpr float k_sequence_end = 9.0F;
inline constexpr float k_approach_start = 0.8F;
inline constexpr float k_approach_end = 3.6F;
inline constexpr float k_return_start = 7.2F;
inline constexpr float k_walk_metres_per_cycle = 1.35F;
} // namespace Nap
} // namespace Render::GL
