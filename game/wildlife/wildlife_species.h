#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Game::Wildlife {

enum class Species : std::uint8_t {
  Sheep = 0,
  Wolf = 1,
  Bird = 2,
};

inline constexpr std::size_t k_species_count = 3U;

enum class Behavior : std::uint8_t {
  Graze = 0,
  Roam = 1,
  Flee = 2,
  Stalk = 3,
  Cruise = 4,
  Scatter = 5,
};

[[nodiscard]] constexpr auto species_index(Species species) noexcept -> std::size_t {
  return static_cast<std::size_t>(species);
}

[[nodiscard]] constexpr auto
species_name(Species species) noexcept -> std::string_view {
  switch (species) {
  case Species::Sheep:
    return "sheep";
  case Species::Wolf:
    return "wolf";
  case Species::Bird:
    return "bird";
  }
  return "sheep";
}

[[nodiscard]] constexpr auto try_parse_species(std::string_view name,
                                               Species& out) noexcept -> bool {
  if (name == "sheep" || name == "sheeps") {
    out = Species::Sheep;
    return true;
  }
  if (name == "wolf" || name == "wolves") {
    out = Species::Wolf;
    return true;
  }
  if (name == "bird" || name == "birds") {
    out = Species::Bird;
    return true;
  }
  return false;
}

[[nodiscard]] constexpr auto
behavior_name(Behavior behavior) noexcept -> std::string_view {
  switch (behavior) {
  case Behavior::Graze:
    return "graze";
  case Behavior::Roam:
    return "roam";
  case Behavior::Flee:
    return "flee";
  case Behavior::Stalk:
    return "stalk";
  case Behavior::Cruise:
    return "cruise";
  case Behavior::Scatter:
    return "scatter";
  }
  return "graze";
}

[[nodiscard]] constexpr auto try_parse_behavior(std::string_view name,
                                                Behavior& out) noexcept -> bool {
  if (name == "graze") {
    out = Behavior::Graze;
    return true;
  }
  if (name == "roam") {
    out = Behavior::Roam;
    return true;
  }
  if (name == "flee") {
    out = Behavior::Flee;
    return true;
  }
  if (name == "stalk") {
    out = Behavior::Stalk;
    return true;
  }
  if (name == "cruise") {
    out = Behavior::Cruise;
    return true;
  }
  if (name == "scatter") {
    out = Behavior::Scatter;
    return true;
  }
  return false;
}

[[nodiscard]] constexpr auto is_ground_species(Species species) noexcept -> bool {
  return species != Species::Bird;
}

} // namespace Game::Wildlife
