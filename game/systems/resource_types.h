#pragma once

#include <QString>
#include <QStringView>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Game::Systems {

enum class ResourceType : std::uint8_t {
  Gold = 0,
  Food,
  Wood,
  Stone,
  Iron,
  Count,
};

inline constexpr std::size_t k_resource_type_count =
    static_cast<std::size_t>(ResourceType::Count);

inline constexpr std::array<ResourceType, k_resource_type_count> k_all_resource_types =
    {
        ResourceType::Gold,
        ResourceType::Food,
        ResourceType::Wood,
        ResourceType::Stone,
        ResourceType::Iron,
};

[[nodiscard]] constexpr auto resource_type_index(ResourceType type) -> std::size_t {
  return static_cast<std::size_t>(type);
}

[[nodiscard]] constexpr auto resource_type_key(ResourceType type) -> const char* {
  switch (type) {
  case ResourceType::Gold:
    return "gold";
  case ResourceType::Food:
    return "food";
  case ResourceType::Wood:
    return "wood";
  case ResourceType::Stone:
    return "stone";
  case ResourceType::Iron:
    return "iron";
  case ResourceType::Count:
    break;
  }
  return "unknown";
}

[[nodiscard]] inline auto resource_type_from_key(QStringView key,
                                                 ResourceType& out) -> bool {
  for (ResourceType const type : k_all_resource_types) {
    if (key == QLatin1String(resource_type_key(type))) {
      out = type;
      return true;
    }
  }
  return false;
}

struct ResourceAmounts {
  [[nodiscard]] auto get(ResourceType type) const -> int {
    return values[resource_type_index(type)];
  }

  void set(ResourceType type, int amount) {
    values[resource_type_index(type)] = std::max(0, amount);
  }

  void add(ResourceType type, int delta) { set(type, get(type) + delta); }

  [[nodiscard]] auto empty() const -> bool {
    return std::all_of(
        values.begin(), values.end(), [](int amount) { return amount <= 0; });
  }

  [[nodiscard]] auto can_cover(const ResourceAmounts& required) const -> bool {
    return std::all_of(k_all_resource_types.begin(),
                       k_all_resource_types.end(),
                       [this, &required](ResourceType type) {
                         return get(type) >= required.get(type);
                       });
  }

  std::array<int, k_resource_type_count> values{};
};

using ResourceTypeMask = std::array<bool, k_resource_type_count>;

struct ResourceOverlay {
  [[nodiscard]] auto has(ResourceType type) const -> bool {
    return specified[resource_type_index(type)];
  }

  [[nodiscard]] auto get(ResourceType type) const -> int { return amounts.get(type); }

  void set(ResourceType type, int amount) {
    amounts.set(type, amount);
    specified[resource_type_index(type)] = true;
  }

  [[nodiscard]] auto empty() const -> bool {
    return std::none_of(
        specified.begin(), specified.end(), [](bool declared) { return declared; });
  }

  void apply_to(ResourceAmounts& target) const {
    for (ResourceType const type : k_all_resource_types) {
      if (has(type)) {
        target.set(type, amounts.get(type));
      }
    }
  }

  ResourceAmounts amounts;
  ResourceTypeMask specified{};
};

struct OwnerResourceState {
  int owner_id = 0;
  ResourceAmounts amounts;
};

} // namespace Game::Systems
