#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace App::Core {

class DeferredPresentationQueue {
public:
  using Job = std::function<void()>;

  void post(Job job);

  auto drain() -> std::size_t;

  [[nodiscard]] auto pending() const -> std::size_t;

  [[nodiscard]] auto drained_jobs() const -> std::uint64_t;

private:
  mutable std::mutex m_mutex;
  std::vector<Job> m_pending;
  std::vector<Job> m_running;
  std::uint64_t m_drained_jobs = 0;
};

} // namespace App::Core
