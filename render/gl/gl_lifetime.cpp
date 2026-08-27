#include "gl_lifetime.h"

#include <QCoreApplication>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>

#include <mutex>
#include <utility>
#include <vector>

#include "platform_gl.h"

namespace Render::GL {

namespace {

struct DeferredDeletes {
  std::mutex mutex;
  std::vector<std::pair<DeferredGlObject, unsigned int>> pending;
};

auto deferred() -> DeferredDeletes& {
  static DeferredDeletes state;
  return state;
}

} // namespace

auto gl_objects_can_be_released() noexcept -> bool {
  if (QCoreApplication::instance() == nullptr) {
    return false;
  }
  return QOpenGLContext::currentContext() != nullptr;
}

void defer_gl_delete(DeferredGlObject kind, unsigned int name) noexcept {
  if (name == 0U) {
    return;
  }
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  state.pending.emplace_back(kind, name);
}

void drain_deferred_gl_deletes() {
  if (!gl_objects_can_be_released()) {
    return;
  }

  std::vector<std::pair<DeferredGlObject, unsigned int>> batch;
  {
    auto& state = deferred();
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (state.pending.empty()) {
      return;
    }
    batch.swap(state.pending);
  }

  QOpenGLExtraFunctions functions(QOpenGLContext::currentContext());
  functions.initializeOpenGLFunctions();
  for (const auto& [kind, name] : batch) {
    switch (kind) {
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
