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

struct HomeSmokeAnchor {
  QVector3D vent{0.0F, 0.0F, 0.0F};
  QVector3D smoke_tint{0.30F, 0.28F, 0.26F};
  float plume_radius{0.55F};

  QVector3D doorstep{0.0F, 0.0F, 0.0F};
  QVector3D outward{0.0F, 0.0F, 1.0F};

  QVector3D cloth_top{0.0F, 0.0F, 0.0F};
  float cloth_span{0.0F};
  bool cloth_indigo{false};

  bool cloth_at_door{false};

  QVector3D lamp{0.0F, 0.0F, 0.0F};

  QVector3D window{0.0F, 0.0F, 0.0F};
  float shutter_width{0.0F};
  float shutter_height{0.0F};

  QVector3D line_a{0.0F, 0.0F, 0.0F};
  QVector3D line_b{0.0F, 0.0F, 0.0F};

  bool line_pole{false};
  int max_laundry{0};

  [[nodiscard]] auto valid() const noexcept -> bool { return plume_radius > 0.0F; }
};

void register_home_smoke_anchor(bool carthage, HomeSmokeAnchor anchor);
[[nodiscard]] auto home_smoke_anchor(bool carthage) -> const HomeSmokeAnchor&;

[[nodiscard]] auto home_bowl_archetype(bool carthage) -> Render::Creature::ArchetypeId;

struct HearthCharacter {
  float size{1.0F};
  float density{1.0F};
  float warmth{0.5F};
  float speed{1.0F};
  float time_offset{0.0F};
  float puff_rate{0.0F};
  float puff_depth{0.0F};
  float puff_phase{0.0F};
};
[[nodiscard]] auto hearth_character(std::uint64_t id) -> HearthCharacter;

struct LampState {
  float strength{0.0F};
  QVector3D color{1.0F, 0.72F, 0.38F};
};

enum class DoorstepActivity : std::uint8_t {
  Stand,
  Sweep,
  Scrub,
  Sit,
  Errand,
  Talk,
  Count
};

struct DoorstepVisit {
  float t{-1.0F};
  float duration{0.0F};
  float out_metres{1.5F};
  float lateral{0.0F};
  float errand_metres{0.0F};
  DoorstepActivity activity{DoorstepActivity::Stand};
  int residents{1};
  [[nodiscard]] auto active() const noexcept -> bool { return t >= 0.0F; }
  [[nodiscard]] auto step_seconds() const noexcept -> float;
};

struct HomeActivity {
  void begin_frame(Engine::Core::World* world,
                   float time,
                   Engine::Core::World* identity = nullptr);
  [[nodiscard]] auto inhabited(std::uint64_t id) const -> bool;

  [[nodiscard]] auto hearth_intensity(std::uint64_t id, float time) const -> float;

  [[nodiscard]] auto meal_bias() const noexcept -> float;
  [[nodiscard]] auto gag(std::uint64_t id, float time) -> float;

  [[nodiscard]] auto night() const noexcept -> float { return night_factor; }
  void set_night(float factor) noexcept { night_factor = factor; }
  [[nodiscard]] auto lamp_state(std::uint64_t id, float time) const -> LampState;

  [[nodiscard]] auto doorstep_plan(std::uint64_t id, float time) const -> DoorstepVisit;

  [[nodiscard]] auto doorstep_visit(std::uint64_t id, float time) const -> float;

  [[nodiscard]] auto curtain_parting(std::uint64_t id, float time) const -> float;

  [[nodiscard]] auto shutter_angle(std::uint64_t id, int window) const -> float;

  float cooldown_seconds{600.0F};
  int remaining_plumes{0};
  int max_plumes{0};
  int remaining_actors{0};
  float night_factor{0.0F};
  bool actors_allowed{false};

  bool hens{true};

  int remaining_ambient_actors{0};
  std::vector<std::uint64_t> homes;
  Engine::Core::World* world{nullptr};
  float previous_time{0.0F};
  float frame_time{0.0F};
  float gag_started{0.0F};
  std::uint64_t gag_home{0};
  bool gag_seen{false};
};

void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

namespace Spill {
inline constexpr float k_emerge_end = 1.6F;
inline constexpr float k_trip_end = 2.3F;
inline constexpr float k_sprawl_end = 4.2F;
inline constexpr float k_rise_end = 5.0F;
inline constexpr float k_stare_end = 6.2F;
inline constexpr float k_return_end = 8.4F;
inline constexpr float k_sequence_end = 9.5F;

inline constexpr float k_walk_metres_per_cycle = 0.66F;
inline constexpr float k_walk_out_metres = 2.4F;
} // namespace Spill

namespace Doorstep {
inline constexpr float k_period = 96.0F;
inline constexpr float k_visit_min = 12.0F;
inline constexpr float k_visit_max = 34.0F;
inline constexpr float k_walk_speed = 0.70F;
inline constexpr float k_sit_down_seconds = 1.2F;
inline constexpr float k_companion_delay = 0.9F;
} // namespace Doorstep
} // namespace Render::GL
