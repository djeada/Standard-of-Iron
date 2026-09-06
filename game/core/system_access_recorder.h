#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "component_registry.h"

#if !defined(SOI_VERIFY_SYSTEM_ACCESS)
#if defined(NDEBUG)
#define SOI_VERIFY_SYSTEM_ACCESS 0
#else
#define SOI_VERIFY_SYSTEM_ACCESS 1
#endif
#endif

namespace Engine::Core {

class SystemAccessRecorder {
public:
  void clear() {
    m_reads.clear();
    m_writes.clear();
  }

  void note(ComponentTypeId type_id, bool mutable_access) {
    auto& target = mutable_access ? m_writes : m_reads;
    if (type_id >= target.size()) {
      target.resize(static_cast<std::size_t>(type_id) + 1U, 0U);
    }
    target[type_id] = 1U;
  }

  [[nodiscard]] auto touched(ComponentTypeId type_id,
                             bool mutable_access) const -> bool {
    const auto& target = mutable_access ? m_writes : m_reads;
    return type_id < target.size() && target[type_id] != 0U;
  }

  [[nodiscard]] auto reads() const -> const std::vector<std::uint8_t>& {
    return m_reads;
  }

  [[nodiscard]] auto writes() const -> const std::vector<std::uint8_t>& {
    return m_writes;
  }

private:
  std::vector<std::uint8_t> m_reads;
  std::vector<std::uint8_t> m_writes;
};

namespace Detail {

[[nodiscard]] auto active_access_recorder() -> SystemAccessRecorder*&;

inline void note_component_access(ComponentTypeId type_id, bool mutable_access) {
#if SOI_VERIFY_SYSTEM_ACCESS
  if (auto* recorder = active_access_recorder()) {
    recorder->note(type_id, mutable_access);
  }
#else
  (void)type_id;
  (void)mutable_access;
#endif
}

} // namespace Detail

class ScopedAccessRecording {
public:
  explicit ScopedAccessRecording(SystemAccessRecorder* recorder)
      : m_previous(Detail::active_access_recorder()) {
    Detail::active_access_recorder() = recorder;
  }

  ScopedAccessRecording(const ScopedAccessRecording&) = delete;
  ScopedAccessRecording(ScopedAccessRecording&&) = delete;
  auto operator=(const ScopedAccessRecording&) -> ScopedAccessRecording& = delete;
  auto operator=(ScopedAccessRecording&&) -> ScopedAccessRecording& = delete;

  ~ScopedAccessRecording() { Detail::active_access_recorder() = m_previous; }

private:
  SystemAccessRecorder* m_previous;
};

} // namespace Engine::Core
