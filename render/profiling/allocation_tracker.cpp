#include "allocation_tracker.h"

#if defined(SOI_PROFILE_ALLOCATIONS)

#include <cstdlib>
#include <new>

namespace {

thread_local std::uint64_t g_allocations = 0;
thread_local std::uint64_t g_allocated_bytes = 0;

auto tracked_allocate(std::size_t size) -> void* {
  void* memory = std::malloc(size == 0U ? 1U : size);
  if (memory != nullptr) {
    ++g_allocations;
    g_allocated_bytes += size;
  }
  return memory;
}

auto tracked_allocate_aligned(std::size_t size, std::size_t alignment) -> void* {
  const std::size_t rounded = ((size + alignment - 1U) / alignment) * alignment;
  void* memory = std::aligned_alloc(alignment, rounded == 0U ? alignment : rounded);
  if (memory != nullptr) {
    ++g_allocations;
    g_allocated_bytes += size;
  }
  return memory;
}

} // namespace

auto operator new(std::size_t size) -> void* {
  void* memory = tracked_allocate(size);
  if (memory == nullptr) {
    throw std::bad_alloc();
  }
  return memory;
}

auto operator new[](std::size_t size) -> void* {
  return ::operator new(size);
}

auto operator new(std::size_t size, const std::nothrow_t&) noexcept -> void* {
  return tracked_allocate(size);
}

auto operator new[](std::size_t size, const std::nothrow_t&) noexcept -> void* {
  return tracked_allocate(size);
}

auto operator new(std::size_t size, std::align_val_t alignment) -> void* {
  void* memory = tracked_allocate_aligned(size, static_cast<std::size_t>(alignment));
  if (memory == nullptr) {
    throw std::bad_alloc();
  }
  return memory;
}

auto operator new[](std::size_t size, std::align_val_t alignment) -> void* {
  return ::operator new(size, alignment);
}

auto operator new(std::size_t size,
                  std::align_val_t alignment,
                  const std::nothrow_t&) noexcept -> void* {
  return tracked_allocate_aligned(size, static_cast<std::size_t>(alignment));
}

auto operator new[](std::size_t size,
                    std::align_val_t alignment,
                    const std::nothrow_t&) noexcept -> void* {
  return tracked_allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory) noexcept {
  std::free(memory);
}

void operator delete[](void* memory) noexcept {
  std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
  std::free(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
  std::free(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept {
  std::free(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
  std::free(memory);
}

void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept {
  std::free(memory);
}

namespace Render::Profiling {

auto allocation_tracking_available() noexcept -> bool {
  return true;
}

auto thread_allocation_count() noexcept -> std::uint64_t {
  return g_allocations;
}

auto thread_allocated_bytes() noexcept -> std::uint64_t {
  return g_allocated_bytes;
}

void reset_thread_allocations() noexcept {
  g_allocations = 0;
  g_allocated_bytes = 0;
}

} // namespace Render::Profiling

#else

namespace Render::Profiling {

auto allocation_tracking_available() noexcept -> bool {
  return false;
}

auto thread_allocation_count() noexcept -> std::uint64_t {
  return 0U;
}

auto thread_allocated_bytes() noexcept -> std::uint64_t {
  return 0U;
}

void reset_thread_allocations() noexcept {
}

} // namespace Render::Profiling

#endif
