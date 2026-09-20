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

// The nation's civilian rig wearing the field props. Derived once from
// nation_civilian_rig(), never authored here.
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

// Owned by a renderer, never stored in the simulation or save/replay state.
struct FarmActivity {
  // One eligible field, resolved once per frame from the render snapshot.
  struct Field {
    std::uint64_t id{0};
    float growth{0.0F};
    int harvests{0};
  };

  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto field(std::uint64_t id) const -> const Field*;
  // `may_start` is false while the sleeper is about to move: the sequence
  // then waits for a window where it can stay put for its whole length.
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

// Field-local geometry shared with the crop generator. A lane is the strip a
// worker walks and works along; lanes 0 and 1 run between the crop rows and
// are kept clear of stalks, lane 2 is the headland outside the crop by the
// gate, which needs no clearing.
struct FarmLane {
  QVector3D from;
  QVector3D to;
  // Unit direction, in field space, from the lane into the standing crop.
  QVector3D crop;
};
inline constexpr int k_farm_lane_count = 3;
inline constexpr int k_farm_headland_lane = 2;
[[nodiscard]] auto farm_activity_lane(bool rows_along_x, int index) -> FarmLane;
[[nodiscard]] auto farm_activity_clearing(const QVector3D& point,
                                          bool rows_along_x) -> bool;
// Where cut sheaves are stood up in a stook beside the gate, and where the
// gatherer stands to set one down.
[[nodiscard]] auto farm_activity_stook_anchor() -> QVector3D;
[[nodiscard]] auto farm_activity_stook_drop() -> QVector3D;

// Which workers a field fields, most first. Fewer than three is common, so a
// row of farms does not read as the same three people copied along it.
struct FarmRoster {
  std::array<int, 3> worker{};
  int count{0};
};
[[nodiscard]] auto farm_activity_roster(std::uint64_t field_id) -> FarmRoster;

// A lane worker's schedule is a loop of three bouts -- work, straighten up,
// a few steps along the row -- every duration and stride of which hangs off
// the entity hash, so no two fields keep time together. This is the pure
// sampling function; submission turns it into a place on the lane.
struct FarmWorkerBeat {
  enum class Kind : std::uint8_t {
    Work,
    Pause,
    Step
  };
  Kind kind{Kind::Work};
  float elapsed{0.0F};  // seconds into this bout phase
  float duration{0.0F}; // its full length
  float advance{0.0F};  // metres walked since the loop began, all bouts
  float walked{0.0F};   // metres walked within this step, zero otherwise
  float stroke_cycle{0.0F};
  float previous_phase{0.0F}; // where the outgoing clip was, for a cross-fade
  float start_fraction{0.0F}; // where along the out-and-back loop it began
  float face_tilt{0.0F};      // degrees turned from the row into the crop
  bool hatted{true};
};
[[nodiscard]] auto farm_worker_beat(std::uint64_t field_id,
                                    int worker,
                                    bool ripe,
                                    float time) -> FarmWorkerBeat;
// True when the worker stays on the same spot for the next `seconds`.
[[nodiscard]] auto farm_worker_stays_put(
    std::uint64_t field_id, int worker, bool ripe, float time, float seconds) -> bool;

// The gatherer's loop on a ripe field: bundle at the row end, lift the sheaf,
// carry it to the stook, set it down, walk back. `trip_metres` is the world
// distance from bundling spot to stook, so the walk beats last as long as the
// walk takes.
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
  float bundle_z{0.0F}; // field-local z of the bundling spot on the headland
  bool hatted{true};
};
[[nodiscard]] auto farm_gatherer_beat(std::uint64_t field_id,
                                      float trip_metres,
                                      float time) -> FarmGathererBeat;

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
