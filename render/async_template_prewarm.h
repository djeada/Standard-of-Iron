#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "render/template_prewarm_catalog.h"

namespace Render::GL {

// The queue of unit templates a background prewarm pass still has to bake, and
// the handshake that lets the render thread pick items off it.
//
// The renderer starts one of these when a mission loads, drains it a few items
// per frame, and forbids render-time baking once it is empty. Keeping the
// mutex, the shared state and the "forbid baking when done" latch together in
// one small type means the invariant is stated once rather than reasserted at
// every call site.
class AsyncTemplatePrewarm {
public:
  struct Work {
    std::vector<PrewarmProfile> profiles;
    std::vector<PrewarmWorkItem> work_items;
    std::atomic<std::size_t> next_index{0};
    std::atomic<bool> cancel_requested{false};
  };
  using WorkPtr = std::shared_ptr<Work>;

  // Installs a fresh batch, replacing anything already queued.
  void start(WorkPtr work, bool forbid_runtime_bake_when_done) {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_work = std::move(work);
    m_forbid_runtime_bake_when_done = forbid_runtime_bake_when_done;
  }

  // The batch currently being drained, or nullptr when there is none.
  [[nodiscard]] auto current() const -> WorkPtr {
    std::lock_guard<std::mutex> const lock(m_mutex);
    return m_work;
  }

  // Detaches the batch so the caller can signal cancellation on it. The latch
  // is dropped too: a cancelled prewarm must not forbid baking.
  auto take() -> WorkPtr {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_forbid_runtime_bake_when_done = false;
    return std::exchange(m_work, nullptr);
  }

  // Clears `work` if it is still the active batch. Returns true when that
  // batch was the one that had asked to forbid render-time baking, which is
  // the caller's cue to turn the ban on.
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
