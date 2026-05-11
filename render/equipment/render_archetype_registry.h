#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Render::GL {

class RenderArchetypeRegistry {
public:
  static auto instance() -> RenderArchetypeRegistry &;

  struct Entry {
    std::string name;
    std::function<void()> warm_fn;
  };

  void register_archetype(std::string name, std::function<void()> warm_fn);
  auto warm_all() -> std::size_t;

  [[nodiscard]] auto entries() const -> const std::vector<Entry> &;
  [[nodiscard]] auto size() const -> std::size_t;

private:
  RenderArchetypeRegistry() = default;
  RenderArchetypeRegistry(const RenderArchetypeRegistry &) = delete;
  RenderArchetypeRegistry(RenderArchetypeRegistry &&) = delete;
  auto operator=(const RenderArchetypeRegistry &) -> RenderArchetypeRegistry & =
                                                         delete;
  auto
  operator=(RenderArchetypeRegistry &&) -> RenderArchetypeRegistry & = delete;

  std::vector<Entry> m_entries;
};

} // namespace Render::GL
