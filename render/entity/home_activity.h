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
  // Top edge of the hanging cloth, and how wide the run of strips is. A Punic
  // house hangs a valance under its roof awning; a domus curtains its door.
  QVector3D cloth_top{0.0F, 0.0F, 0.0F};
  float cloth_span{0.0F};
  bool cloth_indigo{false};
  // A door curtain parts when a resident passes through it; a valance does not.
  bool cloth_at_door{false};
  // Where a lamp sits inside the doorway at night.
  QVector3D lamp{0.0F, 0.0F, 0.0F};
  // Windows on both side walls at (+-window.x, window.y, +-window.z), each
  // with two leaves hinged at its outer edges. Zero width means no shutters.
  QVector3D window{0.0F, 0.0F, 0.0F};
  float shutter_width{0.0F};
  float shutter_height{0.0F};
  // A washing line the model has somewhere to string: between two porch
  // columns, or across a flat roof. Coincident ends mean no line.
  QVector3D line_a{0.0F, 0.0F, 0.0F};
  QVector3D line_b{0.0F, 0.0F, 0.0F};
  // Draw a pole at line_b (a roof has nothing to tie the far end to).
  bool line_pole{false};
  int max_laundry{0};

  [[nodiscard]] auto valid() const noexcept -> bool { return plume_radius > 0.0F; }
};

void register_home_smoke_anchor(bool carthage, HomeSmokeAnchor anchor);
[[nodiscard]] auto home_smoke_anchor(bool carthage) -> const HomeSmokeAnchor&;

// The civilian rig carrying a bowl, derived once from nation_civilian_rig().
[[nodiscard]] auto home_bowl_archetype(bool carthage) -> Render::Creature::ArchetypeId;

// What one hearth's plume is like, hashed from the house so a street of
// plumes is a street of different fires: a big damp one, a thin quick one, a
// warm dry-wood one. The shader varies shape from the same seed via the vent
// position; this side owns size, density, colour and clock.
struct HearthCharacter {
  float size{1.0F};        // Plume radius multiplier.
  float density{1.0F};     // Intensity multiplier.
  float warmth{0.5F};      // 0 = cool damp grey, 1 = warm dry-wood brown.
  float speed{1.0F};       // Rise clock multiplier.
  float time_offset{0.0F}; // Seconds added to the noise clock.
  float puff_rate{0.0F};   // Slow stoking pulse, Hz.
  float puff_depth{0.0F};  // How much of the intensity the pulse takes.
  float puff_phase{0.0F};
};
[[nodiscard]] auto hearth_character(std::uint64_t id) -> HearthCharacter;

// An oil lamp's state at one instant: eased in with the dusk, on its own
// schedule and with its own flicker. Strength is zero for a dark household.
struct LampState {
  float strength{0.0F};
  QVector3D color{1.0F, 0.72F, 0.38F};
};

// What a resident does when they step outside.
enum class DoorstepActivity : std::uint8_t {
  Stand,  // Shifts their weight, looks about.
  Sweep,  // Works at something in front of the door.
  Scrub,  // Kneels at the threshold.
  Sit,    // Sits on the step.
  Errand, // Walks along the front of the house and back.
  Talk,   // Two of them, facing each other.
  Count
};

struct DoorstepVisit {
  float t{-1.0F}; // Seconds into the visit, or -1 when nobody is out.
  float duration{0.0F};
  float out_metres{1.5F};
  float lateral{0.0F}; // Where they settle, across the door.
  float errand_metres{0.0F};
  DoorstepActivity activity{DoorstepActivity::Stand};
  int residents{1};
  [[nodiscard]] auto active() const noexcept -> bool { return t >= 0.0F; }
  [[nodiscard]] auto step_seconds() const noexcept -> float;
};

// Owned by a renderer, never stored in the simulation or save/replay state.
struct HomeActivity {
  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto inhabited(std::uint64_t id) const -> bool;
  // Rising intensity in [0,1]; zero when this house is not currently smoking.
  [[nodiscard]] auto hearth_intensity(std::uint64_t id, float time) const -> float;
  // How much longer hearths stay lit right now: meal-times at dawn and dusk,
  // fewer at midday, banked overnight. Derived from the night factor.
  [[nodiscard]] auto meal_bias() const noexcept -> float;
  [[nodiscard]] auto gag(std::uint64_t id, float time) -> float;
  // 0 in daylight, 1 at full dark. Drives the lamps, the shutters and the
  // meal-time bias.
  [[nodiscard]] auto night() const noexcept -> float { return night_factor; }
  void set_night(float factor) noexcept { night_factor = factor; }
  [[nodiscard]] auto lamp_state(std::uint64_t id, float time) const -> LampState;
  // Ordinary ambient, far more common than the gag: somebody steps outside for
  // a while. Which cycles, how long, what they do and whether a second
  // resident joins all vary per house and per visit.
  [[nodiscard]] auto doorstep_plan(std::uint64_t id, float time) const -> DoorstepVisit;
  // Seconds into the current visit, or -1.
  [[nodiscard]] auto doorstep_visit(std::uint64_t id, float time) const -> float;
  // 0..1: how far the door curtain is pushed aside by someone passing through.
  [[nodiscard]] auto curtain_parting(std::uint64_t id, float time) const -> float;
  // Degrees a shutter leaf stands open, per window (0-3). Closes with the dusk.
  [[nodiscard]] auto shutter_angle(std::uint64_t id, int window) const -> float;

  float cooldown_seconds{600.0F};
  int remaining_plumes{0};
  int max_plumes{0};
  int remaining_actors{0};
  float night_factor{0.0F};
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

// A resident stepping outside for a while. Ordinary ambient, not the gag.
namespace Doorstep {
inline constexpr float k_period = 96.0F;
inline constexpr float k_visit_min = 12.0F;
inline constexpr float k_visit_max = 34.0F;
inline constexpr float k_walk_speed = 0.70F; // Metres per second.
inline constexpr float k_sit_down_seconds = 1.2F;
inline constexpr float k_companion_delay = 0.9F;
} // namespace Doorstep
} // namespace Render::GL
