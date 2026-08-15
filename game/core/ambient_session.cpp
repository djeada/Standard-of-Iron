#include "ambient_session.h"

#include <cstdio>
#include <cstdlib>

namespace Game::Session {

namespace {

const AmbientServices* g_process_services = nullptr;
thread_local const AmbientServices* t_thread_services = nullptr;

} // namespace

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
