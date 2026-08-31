#pragma once

#include <cstddef>
#include <cstdint>

namespace Render::Profiling {

[[nodiscard]] auto allocation_tracking_available() noexcept -> bool;

[[nodiscard]] auto thread_allocation_count() noexcept -> std::uint64_t;

[[nodiscard]] auto thread_allocated_bytes() noexcept -> std::uint64_t;

void reset_thread_allocations() noexcept;

class ThreadAllocationScope {
public:
  ThreadAllocationScope() noexcept
      : m_allocations(thread_allocation_count())
      , m_bytes(thread_allocated_bytes()) {}

  ThreadAllocationScope(const ThreadAllocationScope&) = delete;
  auto operator=(const ThreadAllocationScope&) -> ThreadAllocationScope& = delete;
  ThreadAllocationScope(ThreadAllocationScope&&) = delete;
  auto operator=(ThreadAllocationScope&&) -> ThreadAllocationScope& = delete;
  ~ThreadAllocationScope() = default;

  [[nodiscard]] auto allocations() const noexcept -> std::uint64_t {
    return thread_allocation_count() - m_allocations;
  }

  [[nodiscard]] auto bytes() const noexcept -> std::uint64_t {
    return thread_allocated_bytes() - m_bytes;
  }

private:
  std::uint64_t m_allocations;
  std::uint64_t m_bytes;
};

} // namespace Render::Profiling
