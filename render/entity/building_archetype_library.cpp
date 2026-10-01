#include "building_archetype_library.h"

#include <QDebug>

#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "building_archetype_catalog.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr std::array<BuildingState, 3> k_states{
    BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed};

auto state_slug(BuildingState state) -> std::string_view {
  switch (state) {
  case BuildingState::Damaged:
    return "damaged";
  case BuildingState::Destroyed:
    return "destroyed";
  case BuildingState::Normal:
  default:
    return "normal";
  }
}

auto catalog_entry(std::string_view name) -> const BuildingArchetypeCatalogEntry* {
  for (const auto& entry : building_archetype_catalog()) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

auto build_set(const BuildingArchetypeCatalogEntry& entry) -> BuildingArchetypeSet {
  BuildingArchetypeSet set;
  for (std::size_t i = 0; i < k_states.size(); ++i) {
    const BuildingState state = k_states[i];

    set.states[i].debug_name = building_mesh_key(entry.name, state);
    set.states[i].merged_full =
        StaticMeshLibrary::instance().find(set.states[i].debug_name);
    if (set.states[i].merged_full == nullptr) {
      Render::Creature::report_missing_preloaded_asset(
          "baked building mesh " + building_mesh_key(entry.name, state));
    }
  }
  return set;
}

struct RequestedSets {
  std::mutex mutex;
  std::unordered_map<std::string, std::unique_ptr<BuildingArchetypeSet>> sets;
};

auto requested_sets() -> RequestedSets& {
  static RequestedSets requested;
  return requested;
}

} // namespace

auto building_mesh_key(std::string_view catalog_name,
                       BuildingState state) -> std::string {
  std::string key(catalog_name);
  key += '/';
  key += state_slug(state);
  return key;
}

auto building_archetype_set(std::string_view catalog_name)
    -> const BuildingArchetypeSet& {
  auto& requested = requested_sets();
  const std::lock_guard<std::mutex> lock(requested.mutex);
  auto& slot = requested.sets[std::string(catalog_name)];
  if (slot == nullptr) {
    const auto* entry = catalog_entry(catalog_name);
    if (entry == nullptr) {
      qCritical() << "building_archetype_set: no catalog building named"
                  << QString::fromUtf8(catalog_name.data(),
                                       static_cast<qsizetype>(catalog_name.size()));
      slot = std::make_unique<BuildingArchetypeSet>();
    } else {
      slot = std::make_unique<BuildingArchetypeSet>(build_set(*entry));
    }
  }
  return *slot;
}

void request_nation_buildings(std::string_view nation_slug) {
  std::string prefix(nation_slug);
  prefix += '_';
  for (const auto& entry : building_archetype_catalog()) {
    if (entry.name.starts_with(prefix)) {
      (void)building_archetype_set(entry.name);
    }
  }
}

auto requested_building_meshes()
    -> std::vector<std::shared_ptr<const MergedBuildingMesh>> {
  auto& requested = requested_sets();
  const std::lock_guard<std::mutex> lock(requested.mutex);
  std::vector<std::shared_ptr<const MergedBuildingMesh>> out;
  for (const auto& [_, set] : requested.sets) {
    for (const RenderArchetype& archetype : set->states) {
      if (archetype.merged_full != nullptr) {
        out.push_back(archetype.merged_full);
      }
    }
  }
  return out;
}

auto building_source_parts(std::string_view mesh_key) -> const RenderArchetype* {
  static std::mutex mutex;
  static std::unordered_map<std::string, std::unique_ptr<RenderArchetype>> parts;
  const std::lock_guard<std::mutex> lock(mutex);
  auto& slot = parts[std::string(mesh_key)];
  if (slot == nullptr) {
    const auto slash = mesh_key.rfind('/');
    if (slash == std::string_view::npos) {
      return nullptr;
    }
    const auto* entry = catalog_entry(mesh_key.substr(0, slash));
    if (entry == nullptr) {
      return nullptr;
    }
    for (const BuildingState state : k_states) {
      if (state_slug(state) == mesh_key.substr(slash + 1)) {
        slot = std::make_unique<RenderArchetype>(
            build_building_archetype(entry->build(state), state));
      }
    }
  }
  return slot.get();
}

auto submit_baked_source_parts(ISubmitter& out,
                               const RenderInstance& instance) -> bool {
  if (instance.archetype == nullptr || instance.archetype->merged_full == nullptr) {
    return false;
  }
  const RenderArchetype* source = building_source_parts(instance.archetype->debug_name);
  if (source == nullptr) {
    return false;
  }
  RenderInstance expanded = instance;
  expanded.archetype = source;
  submit_render_instance(out, expanded);
  return true;
}

auto bake_building_meshes() -> std::vector<StaticMeshPackEntry> {
  std::vector<StaticMeshPackEntry> out;
  for (const auto& entry : building_archetype_catalog()) {
    for (const BuildingState state : k_states) {
      const RenderArchetype archetype =
          build_building_archetype(entry.build(state), state);
      out.push_back(StaticMeshPackEntry{
          .name = building_mesh_key(entry.name, state),
          .mesh = build_merged_building_mesh(
              archetype.lods[static_cast<std::size_t>(RenderArchetypeLod::Full)]),
      });
    }
  }
  return out;
}

} // namespace Render::GL
