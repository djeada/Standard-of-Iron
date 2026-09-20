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
class ISubmitter;

// Where a nation's house vents its hearth and where its door is. Authored per
// house model so smoke leaves through an opening the model actually has --
// a Roman domus vents through its roof tiles, a Punic house through the
// rooftop bread oven. Neither gets a chimney.
struct HomeSmokeAnchor {
  QVector3D vent{0.0F, 0.0F, 0.0F};
  QVector3D smoke_tint{0.30F, 0.28F, 0.26F};
  float plume_radius{0.55F};
  // Outside the doorway, and the direction that leads away from the house.
  QVector3D doorstep{0.0F, 0.0F, 0.0F};
  QVector3D outward{0.0F, 0.0F, 1.0F};

  [[nodiscard]] auto valid() const noexcept -> bool { return plume_radius > 0.0F; }
};

void register_home_smoke_anchor(bool carthage, HomeSmokeAnchor anchor);
[[nodiscard]] auto home_smoke_anchor(bool carthage) -> const HomeSmokeAnchor&;

// The civilian rig carrying a bowl, derived once from nation_civilian_rig().
[[nodiscard]] auto home_bowl_archetype(bool carthage) -> Render::Creature::ArchetypeId;

// Owned by a renderer, never stored in the simulation or save/replay state.
struct HomeActivity {
  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto inhabited(std::uint64_t id) const -> bool;
  // Rising intensity in [0,1]; zero when this house is not currently smoking.
  [[nodiscard]] auto hearth_intensity(std::uint64_t id, float time) const -> float;
  [[nodiscard]] auto gag(std::uint64_t id, float time) -> float;

  float cooldown_seconds{600.0F};
  int remaining_plumes{0};
  int max_plumes{0};
  bool actors_allowed{false};
  std::vector<std::uint64_t> homes;
  Engine::Core::World* world{nullptr};
  float previous_time{0.0F};
  float frame_time{0.0F};
  float gag_started{0.0F};
  std::uint64_t gag_home{0};
  bool gag_seen{false};
};

void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

// The soup-spill sequence, as one readable timeline. Beat boundaries are in
// seconds from the moment the shared slot is claimed.
namespace Spill {
inline constexpr float k_emerge_end = 1.6F;
inline constexpr float k_trip_end = 2.3F;
inline constexpr float k_sprawl_end = 4.2F;
inline constexpr float k_rise_end = 5.0F;
inline constexpr float k_stare_end = 6.2F;
inline constexpr float k_return_end = 8.4F;
inline constexpr float k_sequence_end = 9.5F;
inline constexpr float k_walk_metres_per_cycle = 1.35F;
inline constexpr float k_walk_out_metres = 2.4F;
} // namespace Spill
} // namespace Render::GL
