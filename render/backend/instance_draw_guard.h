#pragma once

#include <cstddef>

namespace Render::GL::BackendPipelines {

class InstanceDrawGuard {
public:
  explicit InstanceDrawGuard(const char* tag);

  [[nodiscard]] auto clamp(std::size_t requested, std::size_t resident) -> std::size_t;

  [[nodiscard]] auto tag() const -> const char* { return m_tag; }

  [[nodiscard]] auto overflow_count() const -> std::size_t { return m_overflows; }

private:
  const char* m_tag;
  std::size_t m_overflows{0};
  bool m_reported{false};
};

} // namespace Render::GL::BackendPipelines
