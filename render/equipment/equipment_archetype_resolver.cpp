#include "equipment_archetype_resolver.h"

#include <QDebug>
#include <QString>

#include <utility>

namespace Render::GL::EquipmentArchetype {
namespace {

auto debug_name_for_log(std::string_view debug_name) -> QString {
  return QString::fromUtf8(debug_name.data(),
                           static_cast<qsizetype>(debug_name.size()));
}

} // namespace

auto Resolver::resolve_log_prefix() const -> std::string {
  return "resolve_" + m_domain + "_equipment_archetype:";
}

void Resolver::register_contribution(EquipmentHandle handle,
                                     Contribution contribution) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (handle == k_invalid_equipment_handle) {
    const std::string message =
        "register_" + m_domain + "_equipment_contribution: invalid handle";
    qWarning() << message.c_str();
    return;
  }
  m_contributions[handle] = contribution;
}

auto Resolver::resolve(std::string_view debug_name,
                       Render::Creature::ArchetypeId base_archetype_id,
                       std::span<const EquipmentHandle> handles)
    -> Render::Creature::ArchetypeId {
  std::lock_guard<std::mutex> const lock(m_mutex);

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
  key.debug_name = debug_name;
  key.handles.assign(handles.begin(), handles.end());
  if (const auto it = m_cache.find(key); it != m_cache.end()) {
    return it->second;
  }

  const std::string prefix = resolve_log_prefix();
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr) {
    qWarning() << prefix.c_str() << "missing base archetype"
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

    const auto contribution_it = m_contributions.find(handle);
    if (contribution_it == m_contributions.end()) {
      qWarning() << prefix.c_str() << "missing contribution for"
                 << debug_name_for_log(debug_name) << "handle" << handle;
      return base_archetype_id;
    }

    const auto& contribution = contribution_it->second;
    if (contribution.build_attachments == nullptr) {
      qWarning() << prefix.c_str() << "null attachment builder for"
                 << debug_name_for_log(debug_name) << "handle" << handle;
      return base_archetype_id;
    }

    const auto attachments = contribution.build_attachments(next_role);
    if (desc.bake_attachment_count + attachments.size() >
        Render::Creature::ArchetypeDescriptor::k_max_bake_attachments) {
      qWarning() << prefix.c_str() << "too many attachments for"
                 << debug_name_for_log(debug_name);
      return base_archetype_id;
    }

    for (const auto& attachment : attachments) {
      desc.bake_attachments[desc.bake_attachment_count++] = attachment;
    }

    if (contribution.append_role_colors != nullptr &&
        desc.extra_role_color_fn_count >=
            static_cast<std::uint8_t>(desc.extra_role_color_fns.size())) {
      qWarning() << prefix.c_str() << "too many role color callbacks for"
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
    m_cache.emplace(std::move(key), archetype_id);
  }
  return archetype_id;
}

} // namespace Render::GL::EquipmentArchetype
