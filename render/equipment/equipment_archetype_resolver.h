#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "equipment_registry.h"
#include "render/creature/archetype_registry.h"

namespace Render::GL::EquipmentArchetype {

struct Contribution {
  using BuildAttachmentsFn = std::vector<Render::Creature::StaticAttachmentSpec> (*)(
      std::uint8_t base_role_byte);

  BuildAttachmentsFn build_attachments{nullptr};
  Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn append_role_colors{nullptr};
  std::uint8_t role_count{0U};
};

class Resolver {
public:
  explicit Resolver(std::string domain)
      : m_domain(std::move(domain)) {}

  void register_contribution(EquipmentHandle handle, Contribution contribution);

  [[nodiscard]] auto
  resolve(std::string_view debug_name,
          Render::Creature::ArchetypeId base_archetype_id,
          std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId;

private:
  struct LoadoutCacheKey {
    Render::Creature::ArchetypeId base_archetype_id{
        Render::Creature::k_invalid_archetype};
    std::string debug_name;
    std::vector<EquipmentHandle> handles;

    auto operator==(const LoadoutCacheKey& other) const -> bool {
      return base_archetype_id == other.base_archetype_id &&
             debug_name == other.debug_name && handles == other.handles;
    }
  };

  struct LoadoutCacheKeyHash {
    auto operator()(const LoadoutCacheKey& key) const noexcept -> std::size_t {
      std::size_t hash =
          std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.base_archetype_id));
      hash ^= std::hash<std::string>{}(key.debug_name) + 0x9E3779B9U + (hash << 6U) +
              (hash >> 2U);
      for (const EquipmentHandle handle : key.handles) {
        hash ^= std::hash<EquipmentHandle>{}(handle) + 0x9E3779B9U + (hash << 6U) +
                (hash >> 2U);
      }
      return hash;
    }
  };

  [[nodiscard]] auto resolve_log_prefix() const -> std::string;

  std::string m_domain;
  std::mutex m_mutex;
  std::unordered_map<EquipmentHandle, Contribution> m_contributions;
  std::
      unordered_map<LoadoutCacheKey, Render::Creature::ArchetypeId, LoadoutCacheKeyHash>
          m_cache;
};

} // namespace Render::GL::EquipmentArchetype
