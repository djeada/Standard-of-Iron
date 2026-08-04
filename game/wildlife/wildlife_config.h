#pragma once

#include <cstdint>
#include <vector>

#include "wildlife_species.h"

namespace Game::Wildlife {

struct SpawnArea {
  float x{0.0F};
  float z{0.0F};
  float radius{18.0F};
};

struct SpeciesConfig {
  bool enabled{false};
  int group_count{0};
  int group_size_min{1};
  int group_size_max{1};
  float roam_radius{14.0F};
  float move_speed{1.1F};
  float flee_speed{3.2F};
  float alert_radius{9.0F};
  float aggression{0.35F};
  bool respawn{true};
  float respawn_delay{45.0F};
  float flight_height{0.0F};
  std::vector<SpawnArea> spawn_areas;

  [[nodiscard]] auto clamped_group_size(std::uint32_t roll) const noexcept -> int;
};

struct WildlifeSettings {
  bool enabled{false};
  std::uint32_t seed{0U};
  float near_simulation_radius{48.0F};
  float far_simulation_radius{96.0F};
  SpeciesConfig sheep;
  SpeciesConfig wolves;
  SpeciesConfig birds;

  [[nodiscard]] auto
  for_species(Species species) const noexcept -> const SpeciesConfig&;
  [[nodiscard]] auto for_species(Species species) noexcept -> SpeciesConfig&;
  [[nodiscard]] auto any_species_enabled() const noexcept -> bool;
};

[[nodiscard]] auto default_sheep_config() noexcept -> SpeciesConfig;
[[nodiscard]] auto default_wolf_config() noexcept -> SpeciesConfig;
[[nodiscard]] auto default_bird_config() noexcept -> SpeciesConfig;
[[nodiscard]] auto default_species_config(Species species) noexcept -> SpeciesConfig;

[[nodiscard]] auto default_settings() noexcept -> WildlifeSettings;

void sanitize(WildlifeSettings& settings) noexcept;

} // namespace Game::Wildlife
