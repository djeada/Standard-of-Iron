#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "wildlife_config.h"
#include "wildlife_species.h"
#include "wildlife_terrain_probe.h"
#include "wildlife_threats.h"

namespace Game::Wildlife {

enum class BirdTier : std::uint8_t {
  Near = 0,
  Far = 1,
  Dormant = 2,
};

struct Bird {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float yaw{0.0F};
  float target_x{0.0F};
  float target_z{0.0F};
  float altitude{0.0F};
  float target_altitude{0.0F};
  float speed{0.0F};
  float phase{0.0F};

  float glide{0.0F};

  float bank{0.0F};
  float think_timer{0.0F};
  float state_timer{0.0F};

  float slot_lateral{0.0F};
  float slot_trail{0.0F};

  std::uint32_t rng_state{1U};
  std::uint16_t flock{0U};
  Behavior behavior{Behavior::Cruise};
  BirdTier tier{BirdTier::Near};
  std::uint8_t tint{0U};
};

struct Flock {
  float home_x{0.0F};
  float home_z{0.0F};
  float roam_radius{30.0F};
  float target_x{0.0F};
  float target_z{0.0F};
  float retarget_timer{0.0F};
  float alarm_timer{0.0F};
  std::uint32_t rng_state{1U};

  float heading_x{1.0F};
  float heading_z{0.0F};
  float respite_timer{0.0F};
  float airborne_seconds{0.0F};
  bool airborne{true};
};

struct BirdPopulation {
  std::vector<Flock> flocks;
  std::vector<Bird> birds;
};

struct BirdFrame {
  std::vector<Bird> birds;
  std::uint64_t revision{0U};
};

struct BirdFlockStats {
  std::uint64_t near_updates{0U};
  std::uint64_t far_updates{0U};
  std::uint64_t dormant_skips{0U};
  std::uint64_t scatter_events{0U};
  std::uint64_t flyovers_launched{0U};
  std::uint64_t flyovers_finished{0U};

  void reset() noexcept { *this = BirdFlockStats{}; }
};

class BirdFlockManager {
public:
  BirdFlockManager();
  ~BirdFlockManager() = default;
  BirdFlockManager(const BirdFlockManager&) = delete;
  BirdFlockManager(BirdFlockManager&&) = delete;
  auto operator=(const BirdFlockManager&) -> BirdFlockManager& = delete;
  auto operator=(BirdFlockManager&&) -> BirdFlockManager& = delete;

  static auto instance() -> BirdFlockManager&;

  void set_terrain_probe(ITerrainProbe* probe) noexcept;

  void configure(const SpeciesConfig& config,
                 std::uint32_t seed,
                 float near_simulation_radius,
                 float far_simulation_radius);
  void reset();

  [[nodiscard]] auto is_enabled() const noexcept -> bool { return m_enabled; }
  [[nodiscard]] auto config() const noexcept -> const SpeciesConfig& {
    return m_config;
  }

  void set_focus(float world_x, float world_z) noexcept;
  void clear_focus() noexcept;

  void update(float delta_time, const ThreatField& threats);

  [[nodiscard]] auto birds() const noexcept -> const std::vector<Bird>& {
    return m_population.birds;
  }
  [[nodiscard]] auto flocks() const noexcept -> const std::vector<Flock>& {
    return m_population.flocks;
  }
  [[nodiscard]] auto stats() const noexcept -> const BirdFlockStats& { return m_stats; }

  [[nodiscard]] auto frame() const -> std::shared_ptr<const BirdFrame>;

  [[nodiscard]] auto snapshot() const -> BirdPopulation;
  void restore(const BirdPopulation& population);

private:
  void spawn_population();
  void spawn_flock(std::uint16_t index);
  void update_flock(Flock& flock, float delta_time);
  void update_bird(Bird& bird, Flock& flock, float delta_time);
  void think(Bird& bird, Flock& flock, const ThreatField& threats);
  void integrate(Bird& bird, float delta_time);

  void arm_flyover(Flock& flock, bool first_arm);
  void launch_flyover(Flock& flock, std::uint16_t index);
  void update_flyover(Flock& flock, std::uint16_t index, float delta_time);
  void land_flyover(Flock& flock, std::uint16_t index);
  [[nodiscard]] auto flyover_span() const -> float;

  [[nodiscard]] auto tier_for(float world_x, float world_z) const noexcept -> BirdTier;
  [[nodiscard]] auto ground_height(float world_x, float world_z) const -> float;
  void clamp_to_bounds(float& world_x, float& world_z) const;

  void publish_frame();

  SpeciesConfig m_config{};
  BirdPopulation m_population{};
  BirdFlockStats m_stats{};
  ITerrainProbe* m_terrain{nullptr};
  std::uint32_t m_seed{1U};
  float m_near_radius{48.0F};
  float m_far_radius{96.0F};
  float m_focus_x{0.0F};
  float m_focus_z{0.0F};
  bool m_has_focus{false};
  bool m_enabled{false};

  mutable std::mutex m_frame_mutex;
  std::shared_ptr<const BirdFrame> m_published_frame;
  std::uint64_t m_revision{0U};
};

} // namespace Game::Wildlife
