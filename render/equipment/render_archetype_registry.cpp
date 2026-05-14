#include "render_archetype_registry.h"

namespace Render::GL {

auto RenderArchetypeRegistry::instance() -> RenderArchetypeRegistry& {
  static RenderArchetypeRegistry s_instance;
  return s_instance;
}

void RenderArchetypeRegistry::register_archetype(std::string name,
                                                 std::function<void()> warm_fn) {
  m_entries.push_back({std::move(name), std::move(warm_fn)});
}

auto RenderArchetypeRegistry::warm_all() -> std::size_t {
  for (const auto& entry : m_entries) {
    entry.warm_fn();
  }
  return m_entries.size();
}

auto RenderArchetypeRegistry::entries() const -> const std::vector<Entry>& {
  return m_entries;
}

auto RenderArchetypeRegistry::size() const -> std::size_t {
  return m_entries.size();
}

} // namespace Render::GL
