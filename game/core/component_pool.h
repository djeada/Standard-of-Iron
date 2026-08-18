#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace Engine::Core {

class Component;

namespace Detail {

template <typename T>
class ComponentPool {
public:
  static constexpr std::size_t k_chunk_capacity = 256;

  static auto instance() -> ComponentPool& {
    static ComponentPool s_pool;
    return s_pool;
  }

  template <typename... Args>
  auto construct(Args&&... args) -> T* {
    void* storage = acquire();

    try {
      return new (storage) T(std::forward<Args>(args)...);
    } catch (...) {
      release(storage);
      throw;
    }
  }

  void destroy(T* component) {
    if (component == nullptr) {
      return;
    }
    component->~T();
    release(component);
  }

private:
  struct alignas(T) Chunk {
    std::byte storage[sizeof(T) * k_chunk_capacity];
  };

  auto acquire() -> void* {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_free_slots.empty()) {
      auto& chunk = m_chunks.emplace_back(std::make_unique<Chunk>());
      m_free_slots.reserve(m_free_slots.size() + k_chunk_capacity);

      for (std::size_t i = k_chunk_capacity; i-- > 0;) {
        m_free_slots.push_back(static_cast<void*>(chunk->storage + (i * sizeof(T))));
      }
    }
    void* slot = m_free_slots.back();
    m_free_slots.pop_back();
    return slot;
  }

  void release(void* slot) {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_free_slots.push_back(slot);
  }

  std::mutex m_mutex;
  std::vector<std::unique_ptr<Chunk>> m_chunks;
  std::vector<void*> m_free_slots;
};

struct PooledComponentDeleter {
  void (*release)(Component*) = nullptr;

  void operator()(Component* component) const {
    if (component != nullptr && release != nullptr) {
      release(component);
    }
  }
};

template <typename T>
void release_to_pool(Component* component) {
  ComponentPool<T>::instance().destroy(static_cast<T*>(component));
}

} // namespace Detail

using ComponentPtr = std::unique_ptr<Component, Detail::PooledComponentDeleter>;

} // namespace Engine::Core
