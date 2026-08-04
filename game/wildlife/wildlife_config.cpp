#include "wildlife_config.h"

#include <algorithm>

namespace Game::Wildlife {

namespace {

constexpr int k_max_group_count = 64;
constexpr int k_max_group_size = 64;
constexpr float k_min_roam_radius = 2.0F;
constexpr float k_max_roam_radius = 220.0F;
constexpr float k_max_speed = 24.0F;
constexpr float k_max_alert_radius = 120.0F;
constexpr float k_max_respawn_delay = 3600.0F;
constexpr float k_min_simulation_radius = 12.0F;

void sanitize_species(SpeciesConfig& config) noexcept {
  config.group_count = std::clamp(config.group_count, 0, k_max_group_count);
  config.group_size_min = std::clamp(config.group_size_min, 1, k_max_group_size);
  config.group_size_max =
      std::clamp(config.group_size_max, config.group_size_min, k_max_group_size);
  config.roam_radius =
      std::clamp(config.roam_radius, k_min_roam_radius, k_max_roam_radius);
  config.move_speed = std::clamp(config.move_speed, 0.05F, k_max_speed);
  config.flee_speed = std::clamp(config.flee_speed, config.move_speed, k_max_speed);
  config.alert_radius = std::clamp(config.alert_radius, 0.0F, k_max_alert_radius);
  config.aggression = std::clamp(config.aggression, 0.0F, 1.0F);
  config.respawn_delay = std::clamp(config.respawn_delay, 0.0F, k_max_respawn_delay);
  config.flight_height = std::clamp(config.flight_height, 0.0F, 60.0F);
  for (auto& area : config.spawn_areas) {
    area.radius = std::clamp(area.radius, 1.0F, k_max_roam_radius);
  }
}

} // namespace

auto SpeciesConfig::clamped_group_size(std::uint32_t roll) const noexcept -> int {
  int const low = std::max(1, group_size_min);
  int const high = std::max(low, group_size_max);
  int const span = (high - low) + 1;
  return low + static_cast<int>(roll % static_cast<std::uint32_t>(span));
}

auto WildlifeSettings::for_species(Species species) const noexcept
    -> const SpeciesConfig& {
  switch (species) {
  case Species::Sheep:
    return sheep;
  case Species::Wolf:
    return wolves;
  case Species::Bird:
    return birds;
  }
  return sheep;
}

auto WildlifeSettings::for_species(Species species) noexcept -> SpeciesConfig& {
  switch (species) {
  case Species::Sheep:
    return sheep;
  case Species::Wolf:
    return wolves;
  case Species::Bird:
    return birds;
  }
  return sheep;
}

auto WildlifeSettings::any_species_enabled() const noexcept -> bool {
  return (sheep.enabled && sheep.group_count > 0) ||
         (wolves.enabled && wolves.group_count > 0) ||
         (birds.enabled && birds.group_count > 0);
}

auto default_sheep_config() noexcept -> SpeciesConfig {
  SpeciesConfig config;
  config.enabled = true;
  config.group_count = 2;
  config.group_size_min = 5;
  config.group_size_max = 9;
  config.roam_radius = 13.0F;
  config.move_speed = 0.85F;
  config.flee_speed = 3.4F;
  config.alert_radius = 11.0F;
  config.aggression = 0.0F;
  config.respawn = true;
  config.respawn_delay = 60.0F;
  config.flight_height = 0.0F;
  return config;
}

auto default_wolf_config() noexcept -> SpeciesConfig {
  SpeciesConfig config;
  config.enabled = true;
  config.group_count = 1;
  config.group_size_min = 3;
  config.group_size_max = 5;
  config.roam_radius = 26.0F;
  config.move_speed = 1.6F;
  config.flee_speed = 4.6F;
  config.alert_radius = 20.0F;
  config.aggression = 0.45F;
  config.respawn = true;
  config.respawn_delay = 90.0F;
  config.flight_height = 0.0F;
  return config;
}

auto default_bird_config() noexcept -> SpeciesConfig {
  SpeciesConfig config;
  config.enabled = true;
  config.group_count = 3;
  config.group_size_min = 6;
  config.group_size_max = 12;
  config.roam_radius = 30.0F;
  config.move_speed = 4.5F;
  config.flee_speed = 9.0F;
  config.alert_radius = 12.0F;
  config.aggression = 0.0F;
  config.respawn = true;
  config.respawn_delay = 25.0F;
  config.flight_height = 7.5F;
  return config;
}

auto default_species_config(Species species) noexcept -> SpeciesConfig {
  switch (species) {
  case Species::Sheep:
    return default_sheep_config();
  case Species::Wolf:
    return default_wolf_config();
  case Species::Bird:
    return default_bird_config();
  }
  return default_sheep_config();
}

auto default_settings() noexcept -> WildlifeSettings {
  WildlifeSettings settings;
  settings.enabled = false;
  settings.sheep = default_sheep_config();
  settings.wolves = default_wolf_config();
  settings.birds = default_bird_config();
  return settings;
}

void sanitize(WildlifeSettings& settings) noexcept {
  sanitize_species(settings.sheep);
  sanitize_species(settings.wolves);
  sanitize_species(settings.birds);
  settings.near_simulation_radius =
      std::max(k_min_simulation_radius, settings.near_simulation_radius);
  settings.far_simulation_radius =
      std::max(settings.near_simulation_radius + k_min_simulation_radius,
               settings.far_simulation_radius);
}

} // namespace Game::Wildlife
