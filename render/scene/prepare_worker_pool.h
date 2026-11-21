#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Render {

class PrepareWorkerPool {
public:
  using Job = std::function<void(std::size_t)>;

  PrepareWorkerPool();
  ~PrepareWorkerPool();

  PrepareWorkerPool(const PrepareWorkerPool&) = delete;
  auto operator=(const PrepareWorkerPool&) -> PrepareWorkerPool& = delete;

  [[nodiscard]] auto worker_count() const noexcept -> std::size_t {
    return m_workers.size();
  }

  void run(std::size_t job_count, const Job& job);

private:
  void worker_loop();
  void drain();

  std::vector<std::thread> m_workers;
  std::mutex m_mutex;
  std::condition_variable m_start;
  std::condition_variable m_done;
  const Job* m_job{nullptr};
  std::size_t m_job_count{0};
  std::atomic<std::size_t> m_next{0};
  std::size_t m_generation{0};
  std::size_t m_idle_workers{0};
  bool m_stop{false};
};

} // namespace Render
