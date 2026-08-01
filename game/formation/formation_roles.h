#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Game::Formation {

enum class RoleTag : std::uint32_t {
  LineInfantry = 1U << 0U,
  HeavyInfantry = 1U << 1U,
  SpearInfantry = 1U << 2U,
  Ranged = 1U << 3U,
  Skirmisher = 1U << 4U,
  Cavalry = 1U << 5U,
  Elephant = 1U << 6U,
  Siege = 1U << 7U,
  Command = 1U << 8U,
  Support = 1U << 9U,
  Worker = 1U << 10U,
  Civilian = 1U << 11U,
  Caster = 1U << 12U,
  Shielded = 1U << 13U,
  Mounted = 1U << 14U,
  Expendable = 1U << 15U,
  Awakened = 1U << 16U,
  Elite = 1U << 17U
};

using RoleTagSet = std::uint32_t;

[[nodiscard]] constexpr auto to_mask(RoleTag tag) noexcept -> RoleTagSet {
  return static_cast<RoleTagSet>(tag);
}

[[nodiscard]] constexpr auto has_role(RoleTagSet set, RoleTag tag) noexcept -> bool {
  return (set & to_mask(tag)) != 0U;
}

[[nodiscard]] constexpr auto has_any_role(RoleTagSet set,
                                          RoleTagSet mask) noexcept -> bool {
  return (set & mask) != 0U;
}

[[nodiscard]] constexpr auto has_all_roles(RoleTagSet set,
                                           RoleTagSet mask) noexcept -> bool {
  return mask == 0U || (set & mask) == mask;
}

[[nodiscard]] auto role_tag_to_string(RoleTag tag) -> const char*;
[[nodiscard]] auto try_parse_role_tag(const QString& value) -> std::optional<RoleTag>;
[[nodiscard]] auto
parse_role_tag_set(const std::vector<std::string>& tags) -> RoleTagSet;
[[nodiscard]] auto role_tag_set_to_strings(RoleTagSet set) -> std::vector<std::string>;

enum class ArmyRole : std::uint8_t {
  Centre,
  Vanguard,
  LeftFlank,
  RightFlank,
  Ranged,
  Siege,
  Command,
  Reserve,
  Screen
};

[[nodiscard]] auto army_role_to_string(ArmyRole role) -> const char*;
[[nodiscard]] auto try_parse_army_role(const QString& value) -> std::optional<ArmyRole>;

} // namespace Game::Formation
