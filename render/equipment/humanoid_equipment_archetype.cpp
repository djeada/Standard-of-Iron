#include "humanoid_equipment_archetype.h"

#include <QDebug>
#include <QString>

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Render::GL {
namespace {

struct LoadoutCacheKey {
  Render::Creature::ArchetypeId base_archetype_id{
      Render::Creature::k_invalid_archetype};
  std::vector<EquipmentHandle> handles;

  auto operator==(const LoadoutCacheKey& other) const -> bool {
    return base_archetype_id == other.base_archetype_id && handles == other.handles;
  }
};

struct LoadoutCacheKeyHash {
  auto operator()(const LoadoutCacheKey& key) const noexcept -> std::size_t {
    std::size_t hash =
        std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.base_archetype_id));
    for (const EquipmentHandle handle : key.handles) {
      hash ^= std::hash<EquipmentHandle>{}(handle) + 0x9E3779B9U + (hash << 6U) +
              (hash >> 2U);
    }
    return hash;
  }
};

auto contribution_registry()
    -> std::unordered_map<EquipmentHandle, HumanoidEquipmentContribution>& {
  static std::unordered_map<EquipmentHandle, HumanoidEquipmentContribution> registry;
  return registry;
}

auto archetype_cache() -> std::unordered_map<LoadoutCacheKey,
                                             Render::Creature::ArchetypeId,
                                             LoadoutCacheKeyHash>& {
  static std::
      unordered_map<LoadoutCacheKey, Render::Creature::ArchetypeId, LoadoutCacheKeyHash>
          cache;
  return cache;
}

auto resolver_mutex() -> std::mutex& {
  static std::mutex mutex;
  return mutex;
}

auto debug_name_for_log(std::string_view debug_name) -> QString {
  return QString::fromUtf8(debug_name.data(),
                           static_cast<qsizetype>(debug_name.size()));
}

} // namespace

void register_humanoid_equipment_contribution(
    EquipmentHandle handle, HumanoidEquipmentContribution contribution) {
  std::lock_guard<std::mutex> const lock(resolver_mutex());
  if (handle == k_invalid_equipment_handle) {
    qWarning() << "register_humanoid_equipment_contribution: invalid handle";
    return;
  }
  contribution_registry()[handle] = contribution;
}

auto resolve_humanoid_equipment_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId {
  std::lock_guard<std::mutex> const lock(resolver_mutex());

  bool has_equipment = false;
  for (const EquipmentHandle handle : handles) {
    if (handle != k_invalid_equipment_handle) {
      has_equipment = true;
      break;
    }
  }
  if (!has_equipment) {
    return base_archetype_id;
  }

  LoadoutCacheKey key{};
  key.base_archetype_id = base_archetype_id;
  key.handles.assign(handles.begin(), handles.end());
  if (const auto it = archetype_cache().find(key); it != archetype_cache().end()) {
    return it->second;
  }

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr) {
    qWarning() << "resolve_humanoid_equipment_archetype: missing base archetype"
               << static_cast<unsigned>(base_archetype_id);
    return Render::Creature::k_invalid_archetype;
  }

  Render::Creature::ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = debug_name;
  std::uint8_t next_role = desc.role_count;

  for (const EquipmentHandle handle : handles) {
    if (handle == k_invalid_equipment_handle) {
      continue;
    }

    const auto contribution_it = contribution_registry().find(handle);
    if (contribution_it == contribution_registry().end()) {
      qWarning() << "resolve_humanoid_equipment_archetype: missing contribution for"
                 << debug_name_for_log(debug_name) << "handle" << handle;
      return base_archetype_id;
    }

    const auto& contribution = contribution_it->second;
    if (contribution.build_attachments == nullptr) {
      qWarning() << "resolve_humanoid_equipment_archetype: null attachment builder for"
                 << debug_name_for_log(debug_name) << "handle" << handle;
      return base_archetype_id;
    }

    const auto attachments = contribution.build_attachments(next_role);
    if (desc.bake_attachment_count + attachments.size() >
        Render::Creature::ArchetypeDescriptor::k_max_bake_attachments) {
      qWarning() << "resolve_humanoid_equipment_archetype: too many attachments for"
                 << debug_name_for_log(debug_name);
      return base_archetype_id;
    }

    for (const auto& attachment : attachments) {
      desc.bake_attachments[desc.bake_attachment_count++] = attachment;
    }

    if (contribution.append_role_colors != nullptr &&
        desc.extra_role_color_fn_count >=
            static_cast<std::uint8_t>(desc.extra_role_color_fns.size())) {
      qWarning() << "resolve_humanoid_equipment_archetype: too many role color "
                    "callbacks for"
                 << debug_name_for_log(debug_name);
      return base_archetype_id;
    }

    desc.role_count =
        static_cast<std::uint8_t>(desc.role_count + contribution.role_count);
    next_role = desc.role_count;
    desc.append_extra_role_colors_fn(contribution.append_role_colors);
  }

  const auto archetype_id = registry.register_archetype(desc);
  if (archetype_id != Render::Creature::k_invalid_archetype) {
    archetype_cache().emplace(std::move(key), archetype_id);
  }
  return archetype_id;
}

} // namespace Render::GL
