#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat {
namespace Detail {
namespace {

std::atomic<std::uint64_t> g_formation_cache_epoch{1};

} // namespace

auto formation_cache_epoch() -> std::uint64_t {
  return g_formation_cache_epoch.load(std::memory_order_acquire);
}

void GeometryCaches::clear() {
  ++layout_generation;
  layout.clear();
  spatial.clear();
  slot_presence.clear();
  contact.clear();
  contact_slots.clear();
}

} // namespace Detail

void invalidate_layout_cache() {
  Detail::g_formation_cache_epoch.fetch_add(1, std::memory_order_acq_rel);
  Detail::caches().clear();
}

auto formation_cache_generation() -> std::uint64_t {
  return Detail::formation_cache_epoch();
}

auto contact_stats() -> ContactStats {
  return Detail::caches().stats;
}

void reset_contact_stats() {
  Detail::caches().stats = {};
}

} // namespace Game::Systems::FormationCombat
