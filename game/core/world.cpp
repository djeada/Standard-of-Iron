#include "world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeinfo>

#if defined(__GNUC__) && __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define SOI_HAS_CXA_DEMANGLE 1
#endif
#include <type_traits>
#include <utility>
#include <vector>

#include "component.h"
#include "core/entity.h"
#include "core/system.h"
#include "death_sequence.h"
#include "nav_profile.h"
#include "world_creature_presentation.h"
#include "world_motion_presentation.h"
#include "world_render_snapshot.h"

namespace Engine::Core {

namespace {

World::EntityDestroyedHook g_entity_destroyed_hook = nullptr;

} // namespace

void World::set_entity_destroyed_hook(EntityDestroyedHook hook) {
  g_entity_destroyed_hook = hook;
}

auto World::HandleTable::bind(EntityID entity_id, Registry* registry) -> Entity* {
  const std::uint32_t index = Handle::index_of(entity_id);
  const std::size_t page_index = index / k_page_size;
  while (m_pages.size() <= page_index) {
    m_pages.push_back(std::make_unique<Page>());
  }
  Entity& handle = (*m_pages[page_index])[index % k_page_size];
  handle = Entity(entity_id, registry);
  return &handle;
}

auto World::HandleTable::find(std::uint32_t index) const -> Entity* {
  const std::size_t page_index = index / k_page_size;
  if (page_index >= m_pages.size()) {
    return nullptr;
  }
  return &(*m_pages[page_index])[index % k_page_size];
}

World::World()
    : World(true, false) {
}

namespace {
std::atomic<std::uint64_t> g_next_world_instance_id{1};
} // namespace

World::World(bool presentation_enabled, bool render_snapshot)
    : m_instance_id(g_next_world_instance_id.fetch_add(1, std::memory_order_relaxed))
    , m_presentation_enabled(presentation_enabled)
    , m_is_render_snapshot(render_snapshot) {
  m_registry.set_component_change_callback([this](EntityID entity_id,
                                                  ComponentTypeId type_id,
                                                  std::type_index component_type,
                                                  bool added) {
    this->on_component_changed(entity_id, type_id, component_type, added);
  });
  if (!m_is_render_snapshot) {
    std::atomic_store_explicit(&m_render_snapshot,
                               std::shared_ptr<World>(new World(false, true)),
                               std::memory_order_release);
  }
}

World::~World() = default;

void World::shutdown_systems() {
  while (!m_systems.empty()) {
    m_systems.pop_back();
    if (!m_system_phases.empty()) {
      m_system_phases.pop_back();
    }
  }
}

auto World::resolve(EntityID entity_id) const -> Entity* {
  if (!m_registry.is_alive(entity_id)) {
    return nullptr;
  }
  Entity* handle = m_handles.find(Handle::index_of(entity_id));
  if (handle == nullptr || handle->get_id() != entity_id) {
    return nullptr;
  }
  return handle;
}

void World::on_component_changed(EntityID entity_id,
                                 ComponentTypeId type_id,
                                 std::type_index component_type,
                                 bool added) {
  (void)type_id;
  if (m_component_observers.empty()) {
    return;
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

auto World::collect_entities_with_type(ComponentTypeId type_id,
                                       const std::source_location& where)
    -> std::vector<Entity*> {
  const EntityLock lock(*this);
  ++m_query_counters.collects;
  const std::span<const EntityID> dense = m_registry.entities_with(type_id);
  if (dense.empty()) {
    m_system_profiler.note_collect_call_site(where.file_name(), where.line(), 0);
    return {};
  }

  std::vector<Entity*> result;
  result.reserve(dense.size());
  for (const EntityID id : dense) {
    if (auto* entity = resolve(id)) {
      result.push_back(entity);
    }
  }
  m_query_counters.collected_entities += result.size();
  m_system_profiler.note_collect_call_site(
      where.file_name(), where.line(), result.size());
  return result;
}

auto World::entities_with(ComponentTypeId type_id) const -> std::span<const EntityID> {
  const EntityLock lock(*this);
  return m_registry.entities_with(type_id);
}

void World::resolve_entities_into(std::span<const EntityID> ids,
                                  std::vector<Entity*>& output) const {
  const EntityLock lock(*this);
  output.clear();
  if (output.capacity() < ids.size()) {
    output.reserve(ids.size());
  }
  for (const EntityID id : ids) {
    if (Entity* entity = resolve(id)) {
      output.push_back(entity);
    }
  }
}

auto World::create_entity() -> Entity* {
  const EntityLock lock(*this);
  const EntityID id = m_registry.create_entity();
  return m_handles.bind(id, &m_registry);
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const EntityLock lock(*this);
  const EntityID id = m_registry.create_entity_with_id(entity_id);
  if (id == NULL_ENTITY) {
    return nullptr;
  }
  return m_handles.bind(id, &m_registry);
}

void World::destroy_entity(EntityID entity_id) {
  const EntityLock lock(*this);

  if (m_registry.is_alive(entity_id)) {
    if (!m_is_render_snapshot && g_entity_destroyed_hook != nullptr) {
      g_entity_destroyed_hook(*this, entity_id);
    }
    m_registry.destroy_entity(entity_id);
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const EntityLock lock(*this);

  if (!m_is_render_snapshot) {
    const auto observers = m_entity_destroyed_observers;
    std::vector<EntityID> live;
    live.reserve(m_registry.entity_count());
    const std::size_t slot_total = m_registry.slot_count();
    for (std::size_t index = 1; index < slot_total; ++index) {
      const EntityID id = m_registry.entity_at_index(static_cast<std::uint32_t>(index));
      if (id != NULL_ENTITY) {
        live.push_back(id);
      }
    }
    for (const EntityID id : live) {
      if (g_entity_destroyed_hook != nullptr) {
        g_entity_destroyed_hook(*this, id);
      }
      for (const auto& observer : observers) {
        observer.callback(id);
      }
    }
  }

  m_registry.clear();
  m_deferred.clear();
  m_spatial_index.clear();

  if (!m_is_render_snapshot) {

    ++m_content_epoch;
  }

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const EntityLock lock(*this);
  return resolve(entity_id);
}

auto World::is_alive(EntityID entity_id) const -> bool {
  const EntityLock lock(*this);
  return m_registry.is_alive(entity_id);
}

auto World::entity_count() const -> std::size_t {
  const EntityLock lock(*this);
  return m_registry.entity_count();
}

void World::add_system(std::unique_ptr<System> system) {
  const SystemPhase phase = system != nullptr ? system->phase() : SystemPhase::Combat;
  add_system(std::move(system), phase);
}

void World::add_system(std::unique_ptr<System> system, SystemPhase phase) {
  m_systems.push_back(std::move(system));
  m_system_phases.push_back(phase);
}

auto World::plan_phase_schedule(SystemPhase phase) const
    -> std::vector<std::vector<std::size_t>> {
  std::vector<SystemAccess> declared;
  std::vector<std::size_t> phase_slots;
  for (std::size_t slot = 0; slot < m_systems.size(); ++slot) {
    if (m_system_phases[slot] != phase || m_systems[slot] == nullptr) {
      continue;
    }
    declared.push_back(m_systems[slot]->access());
    phase_slots.push_back(slot);
  }

  auto batches = plan_phase_batches(declared);
  for (auto& batch : batches) {
    for (std::size_t& index : batch) {
      index = phase_slots[index];
    }
  }
  return batches;
}

namespace {

auto demangled_system_name(const std::type_info& type) -> const std::string& {
  static std::map<std::string, std::string> cache;
  const std::string key = type.name();
  auto existing = cache.find(key);
  if (existing != cache.end()) {
    return existing->second;
  }

  std::string readable = key;
#if defined(SOI_HAS_CXA_DEMANGLE)
  int status = 0;
  char* raw = abi::__cxa_demangle(key.c_str(), nullptr, nullptr, &status);
  if (status == 0 && raw != nullptr) {
    readable = raw;
  }
  std::free(raw);
#endif

  const std::size_t last_scope = readable.rfind("::");
  if (last_scope != std::string::npos) {
    readable = readable.substr(last_scope + 2);
  }
  return cache.emplace(key, std::move(readable)).first->second;
}

} // namespace

auto World::system_display_name(const System& system) -> const char* {
  return demangled_system_name(typeid(system)).c_str();
}

auto World::current_query_counters() const -> SystemProfiler::QueryCounters {
  SystemProfiler::QueryCounters counters = m_query_counters;
  const WorldSpatialIndex::Stats& spatial = m_spatial_index.stats();
  counters.spatial_queries = spatial.queries;
  counters.spatial_candidates = spatial.candidates_examined;
  return counters;
}

void World::verify_system_access(std::size_t slot, System& system, float delta_time) {
  const SystemAccess declared = system.access();
  m_access_recorder.clear();
  {
    const ScopedAccessRecording recording(&m_access_recorder);
    system.update(this, delta_time);
  }

  if (declared.exclusive) {
    return;
  }

  auto declares = [&](ComponentTypeId type_id, bool write) {
    const auto& list = write ? declared.writes : declared.reads;
    if (std::find(list.begin(), list.end(), type_id) != list.end()) {
      return true;
    }

    return !write &&
           std::find(declared.writes.begin(), declared.writes.end(), type_id) !=
               declared.writes.end();
  };

  const auto report = [&](const std::vector<std::uint8_t>& touched, bool write) {
    for (std::size_t type_id = 0; type_id < touched.size(); ++type_id) {
      if (touched[type_id] == 0U) {
        continue;
      }
      const auto id = static_cast<ComponentTypeId>(type_id);
      if (declares(id, write)) {
        continue;
      }
      m_access_violations.push_back(
          {.system_name = system_display_name(system), .type_id = id, .write = write});
    }
  };

  report(m_access_recorder.writes(), true);
  report(m_access_recorder.reads(), false);
  (void)slot;
}

void World::update(float delta_time) {
  const bool profiling = m_system_profiler.enabled();
  const auto update_entered = std::chrono::steady_clock::now();
  const EntityLock lock(*this);
  const auto lock_acquired = std::chrono::steady_clock::now();
  ++m_tick_id;
  if (m_presentation_enabled) {
    begin_motion_presentation_frame(*this, delta_time);
  }

  const NavTickScope nav_tick;
  const auto tick_started = std::chrono::steady_clock::now();
  if (profiling) {
    m_system_profiler.begin_tick(m_tick_id, m_registry.entity_count());
  }

  SystemPhase current_phase =
      m_system_phases.empty() ? SystemPhase::Input : m_system_phases.front();

  for (std::size_t slot = 0; slot < m_systems.size(); ++slot) {
    System& system = *m_systems[slot];

    const SystemPhase slot_phase = m_system_phases[slot];
    if (slot_phase != current_phase) {
      m_deferred.apply(*this);
      current_phase = slot_phase;
    }

    if (m_verify_system_access) {
      verify_system_access(slot, system, delta_time);
      continue;
    }

    if (!profiling) {
      system.update(this, delta_time);
      continue;
    }

    const SystemProfiler::QueryCounters queries_before = current_query_counters();
    const auto started = std::chrono::steady_clock::now();
    system.update(this, delta_time);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    m_system_profiler.record_system(
        slot,
        system_display_name(system),
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()),
        current_query_counters() - queries_before);
  }

  m_deferred.apply(*this);

  const auto systems_ended = std::chrono::steady_clock::now();
  if (m_presentation_enabled) {
    finalize_motion_presentation_frame(*this, delta_time);
    publish_creature_presentation_frame(*this);
  }
  publish_movement_trace_frame(*this, delta_time);
  const auto presentation_ended = std::chrono::steady_clock::now();
  if (!m_is_render_snapshot &&
      m_render_snapshots_requested.load(std::memory_order_acquire)) {
    publish_render_snapshot();
  }

  if (profiling) {
    const auto micros = [](auto from, auto to) -> std::uint64_t {
      return static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::microseconds>(to - from).count());
    };
    const auto update_left = std::chrono::steady_clock::now();
    m_system_profiler.end_tick(
        {.total_us = micros(update_entered, update_left),
         .systems_us = micros(tick_started, systems_ended),
         .presentation_us = micros(systems_ended, presentation_ended),
         .publication_us = micros(presentation_ended, update_left),
         .lock_wait_us = micros(update_entered, lock_acquired)});
  }
}

auto World::acquire_render_snapshot() const -> std::shared_ptr<World> {
  return std::atomic_load_explicit(&m_render_snapshot, std::memory_order_acquire);
}

void World::ensure_render_snapshot() {
  if (m_is_render_snapshot) {
    return;
  }
  request_render_snapshots();
  if (m_render_publish_revision.load(std::memory_order_acquire) != 0) {
    return;
  }
  const EntityLock lock(*this);
  if (m_render_publish_revision.load(std::memory_order_acquire) == 0) {
    publish_render_snapshot();
  }
}

void World::publish_render_snapshot() {
  auto const published = acquire_render_snapshot();

  auto free_buffer = [&](std::size_t index) {
    auto& candidate = m_render_snapshot_buffers[index];
    if (candidate == nullptr) {
      candidate = std::shared_ptr<World>(new World(false, true));
      return true;
    }
    return candidate != published && candidate.use_count() <= 1;
  };

  std::size_t buffer_index = m_render_snapshot_buffers.size();
  for (std::size_t offset = 0; offset < m_render_snapshot_buffers.size(); ++offset) {
    const std::size_t candidate =
        (m_next_render_snapshot_buffer + offset) % m_render_snapshot_buffers.size();
    if (free_buffer(candidate)) {
      buffer_index = candidate;
      break;
    }
  }

  if (buffer_index == m_render_snapshot_buffers.size()) {

    ++m_render_publication_stats.skipped_publications;
    return;
  }

  m_next_render_snapshot_buffer =
      (buffer_index + 1U) % m_render_snapshot_buffers.size();
  ++m_render_publication_stats.publications;
  auto snapshot = m_render_snapshot_buffers[buffer_index];
  snapshot->m_render_unit_ids.clear();
  snapshot->m_render_building_ids.clear();
  snapshot->m_render_other_ids.clear();
  std::size_t const slot_count = m_registry.slot_count();
  if (snapshot->m_render_snapshot_epoch != m_content_epoch) {

    snapshot->m_render_entity_signatures.assign(slot_count, 0U);
    snapshot->m_render_snapshot_epoch = m_content_epoch;
  }
  if (snapshot->m_render_entity_signatures.size() < slot_count) {
    snapshot->m_render_entity_signatures.resize(slot_count, 0U);
  }
  snapshot->m_render_effects_frame = m_render_effects_frame;
  m_render_publish_revision.fetch_add(1, std::memory_order_release);
  snapshot->m_render_unit_ids.reserve(entities_with<UnitComponent>().size());
  snapshot->m_render_building_ids.reserve(entities_with<BuildingComponent>().size());
  snapshot->m_render_other_ids.reserve(entities_with<RenderableComponent>().size());

  for (std::size_t index = 1; index < slot_count; ++index) {
    EntityID const source_id =
        m_registry.entity_at_index(static_cast<std::uint32_t>(index));
    EntityID const snapshot_id =
        snapshot->m_registry.entity_at_index(static_cast<std::uint32_t>(index));
    if (source_id == NULL_ENTITY) {
      if (snapshot_id != NULL_ENTITY) {
        snapshot->destroy_entity(snapshot_id);
      }
      snapshot->m_render_entity_signatures[index] = 0U;
      continue;
    }
    Entity const& source = *resolve(source_id);
    Entity* destination = snapshot->resolve(source_id);

    bool const stable = render_entity_is_stable(source);
    std::uint64_t signature = k_render_signature_unstable;
    bool reusable = false;
    if (destination != nullptr && stable) {
      signature = render_entity_signature(source);
      if (signature == k_render_signature_unstable) {
        signature = 1ULL;
      }
      reusable = snapshot->m_render_entity_signatures[index] == signature;
    }

    if (!reusable) {
      if (destination == nullptr) {
        if (snapshot_id != NULL_ENTITY) {
          snapshot->destroy_entity(snapshot_id);
        }
        destination = snapshot->create_entity_with_id(source_id);
        if (destination == nullptr) {
          continue;
        }
      }
      m_render_publication_stats.revisioned_components_skipped +=
          copy_render_components(source, *destination);
      snapshot->m_render_entity_signatures[index] = signature;
      ++m_render_publication_stats.entities_copied;
    } else {
      ++m_render_publication_stats.entities_reused;
    }

    if (!destination->has_component<RenderableComponent>() ||
        destination->has_component<PendingRemovalComponent>()) {
      continue;
    }
    if (destination->has_component<UnitComponent>()) {
      snapshot->m_render_unit_ids.push_back(source_id);
    } else if (destination->has_component<BuildingComponent>()) {
      snapshot->m_render_building_ids.push_back(source_id);
    } else {
      snapshot->m_render_other_ids.push_back(source_id);
    }
  }
  std::atomic_store_explicit(
      &m_render_snapshot, std::move(snapshot), std::memory_order_release);
}

auto World::collect_units_matching(int owner_id,
                                   bool owned) const -> std::vector<Entity*> {
  const EntityLock lock(*this);
  const auto unit_ids = m_registry.entities_with<UnitComponent>();
  std::vector<Entity*> result;
  result.reserve(unit_ids.size());
  for (const EntityID id : unit_ids) {
    const auto* unit = m_registry.try_get<UnitComponent>(id);
    if (unit == nullptr || (unit->owner_id == owner_id) != owned) {
      continue;
    }
    if (Entity* entity = resolve(id)) {
      result.push_back(entity);
    }
  }
  return result;
}

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units_matching(owner_id, true);
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units_matching(owner_id, false);
}

auto World::get_next_entity_id() const -> EntityID {
  const EntityLock lock(*this);
  return Handle::make(static_cast<std::uint32_t>(m_registry.slot_count()), 0);
}

void World::set_next_entity_id(EntityID next_id) {
  const EntityLock lock(*this);
  m_registry.reserve_indices_below(Handle::index_of(next_id));
}

auto World::add_component_observer(ComponentObserverCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_component_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_entity_destroyed_observer(EntityDestroyedCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_entity_destroyed_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_world_cleared_observer(WorldClearedCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_world_cleared_observers.push_back({handle, std::move(callback)});
  return handle;
}

void World::remove_component_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_component_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_entity_destroyed_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_entity_destroyed_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_world_cleared_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_world_cleared_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

} // namespace Engine::Core
