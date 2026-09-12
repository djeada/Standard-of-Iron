#include "gl_lifetime.h"

#include <QCoreApplication>
#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLContextGroup>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "platform_gl.h"

namespace Render::GL {

namespace {

struct PendingDelete {
  DeferredGlObject kind;
  unsigned int name;
  GlShareGroup group;
};

struct DeferredDeletes {
  std::mutex mutex;
  std::vector<PendingDelete> pending;
};

auto deferred() -> DeferredDeletes& {
  static DeferredDeletes state;
  return state;
}

struct ShareGroups {
  std::mutex mutex;
  std::unordered_map<const QOpenGLContextGroup*, GlShareGroup> ids;
  GlShareGroup next = 1;
};

auto share_groups() -> ShareGroups& {
  static ShareGroups state;
  return state;
}

auto id_for(const QOpenGLContextGroup* group) -> GlShareGroup {
  if (group == nullptr) {
    return k_unknown_share_group;
  }
  auto& state = share_groups();
  {
    const std::lock_guard<std::mutex> lock(state.mutex);
    const auto existing = state.ids.find(group);
    if (existing != state.ids.end()) {
      return existing->second;
    }
  }

  GlShareGroup assigned = k_unknown_share_group;
  {
    const std::lock_guard<std::mutex> lock(state.mutex);
    const auto inserted = state.ids.emplace(group, state.next);
    if (inserted.second) {
      ++state.next;
    }
    assigned = inserted.first->second;
  }

  QObject::connect(group, &QObject::destroyed, [assigned](QObject* dead) {
    auto& groups = share_groups();
    {
      const std::lock_guard<std::mutex> lock(groups.mutex);
      groups.ids.erase(static_cast<const QOpenGLContextGroup*>(dead));
    }
    forget_gl_share_group(assigned);
  });

  return assigned;
}

} // namespace

auto current_gl_share_group() noexcept -> GlShareGroup {
  if (QCoreApplication::instance() == nullptr) {
    return k_unknown_share_group;
  }
  auto* context = QOpenGLContext::currentContext();
  if (context == nullptr) {
    return k_unknown_share_group;
  }
  return id_for(context->shareGroup());
}

auto gl_objects_can_be_released() noexcept -> bool {
  if (QCoreApplication::instance() == nullptr) {
    return false;
  }
  return QOpenGLContext::currentContext() != nullptr;
}

auto gl_objects_can_be_released(GlShareGroup group) noexcept -> bool {
  if (!gl_objects_can_be_released()) {
    return false;
  }
  if (group == k_unknown_share_group) {
    return true;
  }
  return current_gl_share_group() == group;
}

void defer_gl_delete(DeferredGlObject kind,
                     unsigned int name,
                     GlShareGroup group) noexcept {
  if (name == 0U) {
    return;
  }
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  state.pending.push_back(PendingDelete{.kind = kind, .name = name, .group = group});
}

void forget_gl_share_group(GlShareGroup group) noexcept {
  if (group == k_unknown_share_group) {
    return;
  }
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  std::vector<PendingDelete> kept;
  kept.reserve(state.pending.size());
  for (const auto& entry : state.pending) {
    if (entry.group != group) {
      kept.push_back(entry);
    }
  }
  state.pending.swap(kept);
}

void drain_deferred_gl_deletes() {
  if (!gl_objects_can_be_released()) {
    return;
  }

  const GlShareGroup current = current_gl_share_group();

  std::vector<PendingDelete> batch;
  {
    auto& state = deferred();
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (state.pending.empty()) {
      return;
    }
    std::vector<PendingDelete> kept;
    kept.reserve(state.pending.size());
    for (const auto& entry : state.pending) {
      if (entry.group == current || entry.group == k_unknown_share_group) {
        batch.push_back(entry);
      } else {
        kept.push_back(entry);
      }
    }
    state.pending.swap(kept);
  }

  if (batch.empty()) {
    return;
  }

  QOpenGLExtraFunctions functions(QOpenGLContext::currentContext());
  functions.initializeOpenGLFunctions();
  for (const auto& entry : batch) {
    const unsigned int name = entry.name;
    switch (entry.kind) {
    case DeferredGlObject::Buffer:
      functions.glDeleteBuffers(1, &name);
      break;
    case DeferredGlObject::VertexArray:
      functions.glDeleteVertexArrays(1, &name);
      break;
    case DeferredGlObject::Texture:
      functions.glDeleteTextures(1, &name);
      break;
    }
  }
}

auto deferred_gl_delete_count() noexcept -> std::size_t {
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  return state.pending.size();
}

} // namespace Render::GL
