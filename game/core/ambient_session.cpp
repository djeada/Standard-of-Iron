#include "ambient_session.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace Game::Session {

namespace {

const AmbientServices* g_process_services = nullptr;
thread_local const AmbientServices* t_thread_services = nullptr;

std::mutex g_world_services_mutex;
std::unordered_map<const Engine::Core::World*, const AmbientServices*> g_world_services;

std::atomic<const Engine::Core::World*> g_solo_world{nullptr};
std::atomic<const AmbientServices*> g_solo_services{nullptr};

void refresh_solo_world_locked() {
  if (g_world_services.size() == 1) {
    const auto it = g_world_services.begin();
    g_solo_services.store(it->second, std::memory_order_relaxed);
    g_solo_world.store(it->first, std::memory_order_release);
    return;
  }
  g_solo_world.store(nullptr, std::memory_order_release);
  g_solo_services.store(nullptr, std::memory_order_relaxed);
}

} // namespace

void bind_world_services(const Engine::Core::World& world,
                         const AmbientServices* services) {
  const std::lock_guard<std::mutex> lock(g_world_services_mutex);
  g_world_services[&world] = services;
  refresh_solo_world_locked();
}

void unbind_world_services(const Engine::Core::World& world) {
  const std::lock_guard<std::mutex> lock(g_world_services_mutex);
  g_world_services.erase(&world);
  refresh_solo_world_locked();
}

auto services_for_or_null(const Engine::Core::World& world) -> const AmbientServices* {
  if (g_solo_world.load(std::memory_order_acquire) == &world) {
    if (const auto* solo = g_solo_services.load(std::memory_order_relaxed)) {
      return solo;
    }
  }
  const std::lock_guard<std::mutex> lock(g_world_services_mutex);
  const auto it = g_world_services.find(&world);
  return it != g_world_services.end() ? it->second : nullptr;
}

auto services_for(const Engine::Core::World& world) -> const AmbientServices& {
  if (const auto* owned = services_for_or_null(world)) {
    return *owned;
  }
  return ambient_services();
}

auto ambient_services_or_null() -> const AmbientServices* {
  return t_thread_services != nullptr ? t_thread_services : g_process_services;
}

auto ambient_services() -> const AmbientServices& {
  if (const auto* bound = ambient_services_or_null()) {
    return *bound;
  }
  std::fputs("No Game::Session::SessionContext is active on this thread or "
             "process. Construct one and make it active (ScopedSession) before "
             "using per-match services such as TerrainService::instance().\n",
             stderr);
  std::abort();
}

auto set_ambient_services(const AmbientServices* services) -> const AmbientServices* {
  const auto* previous = g_process_services;
  g_process_services = services;
  return previous;
}

auto set_thread_ambient_services(const AmbientServices* services)
    -> const AmbientServices* {
  const auto* previous = t_thread_services;
  t_thread_services = services;
  return previous;
}

void unbind_ambient_services(const AmbientServices* services) {
  if (t_thread_services == services) {
    t_thread_services = nullptr;
  }
  if (g_process_services == services) {
    g_process_services = nullptr;
  }
}

} // namespace Game::Session
