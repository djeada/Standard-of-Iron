#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "render/template_prewarm_catalog.h"

namespace Render::GL {

class AsyncTemplatePrewarm {
public:
  struct Work {
    std::vector<PrewarmProfile> profiles;
    std::vector<PrewarmWorkItem> work_items;
    std::atomic<std::size_t> next_index{0};
    std::atomic<bool> cancel_requested{false};
  };
  using WorkPtr = std::shared_ptr<Work>;

  void start(WorkPtr work, bool forbid_runtime_bake_when_done) {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_work = std::move(work);
    m_forbid_runtime_bake_when_done = forbid_runtime_bake_when_done;
  }

  [[nodiscard]] auto current() const -> WorkPtr {
    std::lock_guard<std::mutex> const lock(m_mutex);
    return m_work;
  }

  auto take() -> WorkPtr {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_forbid_runtime_bake_when_done = false;
    return std::exchange(m_work, nullptr);
  }

  auto finish(const WorkPtr& work) -> bool {
    std::lock_guard<std::mutex> const lock(m_mutex);
    if (m_work != work) {
      return false;
    }
    m_work.reset();
    return std::exchange(m_forbid_runtime_bake_when_done, false);
  }

  void clear_forbid_runtime_bake() {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_forbid_runtime_bake_when_done = false;
  }

private:
  mutable std::mutex m_mutex;
  WorkPtr m_work;
  bool m_forbid_runtime_bake_when_done{false};
};

} // namespace Render::GL
