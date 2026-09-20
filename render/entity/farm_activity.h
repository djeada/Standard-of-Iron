#pragma once

#include <QVector3D>

#include <array>
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

struct FarmWorkerVisual {
  Render::Creature::ArchetypeId hatted_tending{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId hatted_reaping{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId hatted_gathering{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId bare_gathering{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId napping{Render::Creature::k_invalid_archetype};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return hatted_tending != Render::Creature::k_invalid_archetype &&
           hatted_reaping != Render::Creature::k_invalid_archetype;
  }
};

[[nodiscard]] auto farm_worker_visual(bool carthage) -> const FarmWorkerVisual&;

struct FarmActivity {

  struct Field {
    std::uint64_t id{0};
    float growth{0.0F};
    int harvests{0};
  };

  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto field(std::uint64_t id) const -> const Field*;

  [[nodiscard]] auto
  gag(std::uint64_t id, int stage, float time, bool may_start = true) -> float;

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
[[nodiscard]] auto farm_activity_growth_stage(float growth) -> int;

struct FarmLane {
  QVector3D from;
  QVector3D to;

  QVector3D crop;
};
inline constexpr int k_farm_lane_count = 3;
inline constexpr int k_farm_headland_lane = 2;
[[nodiscard]] auto farm_activity_lane(bool rows_along_x, int index) -> FarmLane;
[[nodiscard]] auto farm_activity_clearing(const QVector3D& point,
                                          bool rows_along_x) -> bool;

[[nodiscard]] auto farm_activity_stook_anchor() -> QVector3D;
[[nodiscard]] auto farm_activity_stook_drop() -> QVector3D;

struct FarmRoster {
  std::array<int, 3> worker{};
  int count{0};
};
[[nodiscard]] auto farm_activity_roster(std::uint64_t field_id) -> FarmRoster;

struct FarmWorkerBeat {
  enum class Kind : std::uint8_t {
    Work,
    Pause,
    Step
  };
  Kind kind{Kind::Work};
  float elapsed{0.0F};
  float duration{0.0F};
  float advance{0.0F};
  float walked{0.0F};
  float stroke_cycle{0.0F};
  float previous_phase{0.0F};
  float start_fraction{0.0F};
  float face_tilt{0.0F};
  bool hatted{true};
};
[[nodiscard]] auto farm_worker_beat(std::uint64_t field_id,
                                    int worker,
                                    bool ripe,
                                    float time) -> FarmWorkerBeat;

[[nodiscard]] auto farm_worker_stays_put(
    std::uint64_t field_id, int worker, bool ripe, float time, float seconds) -> bool;

struct FarmGathererBeat {
  enum class Kind : std::uint8_t {
    Bundle,
    Lift,
    Carry,
    Drop,
    Return
  };
  Kind kind{Kind::Bundle};
  float elapsed{0.0F};
  float duration{0.0F};
  float walked{0.0F};
  float bundle_z{0.0F};
  bool hatted{true};
};
[[nodiscard]] auto farm_gatherer_beat(std::uint64_t field_id,
                                      float trip_metres,
                                      float time) -> FarmGathererBeat;

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
