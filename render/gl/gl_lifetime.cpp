#include "render/gl/gl_lifetime.h"

#include <QCoreApplication>
#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLContextGroup>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "render/gl/platform_gl.h"

namespace Render::GL {

namespace {

struct PendingDelete {
  DeferredGlObject kind;
  unsigned int name;
  GlShareGroup group;
  void* sync = nullptr;
};

struct DeferredDeletes {
  std::mutex mutex;
  std::vector<PendingDelete> pending;

  std::unordered_set<GlShareGroup> dead_groups;
};

struct Census {
  std::array<std::atomic<std::uint64_t>, k_gl_object_kind_count> created{};
  std::array<std::atomic<std::uint64_t>, k_gl_object_kind_count> deleted{};
};

auto census() -> Census& {
  static Census state;
  return state;
}

void note_deleted(DeferredGlObject kind, std::size_t count = 1) noexcept {
  census().deleted[static_cast<std::size_t>(kind)].fetch_add(count,
                                                             std::memory_order_relaxed);
}

auto deferred() -> DeferredDeletes& {
  static DeferredDeletes state;
  return state;
}

struct ShareGroups {
  std::mutex mutex;
  std::unordered_map<const QOpenGLContextGroup*, GlShareGroup> ids;
  GlShareGroup next = 1;
  std::atomic<std::uint64_t> generation{0};
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

  QObject::connect(group, &QObject::destroyed, [assigned, group]() {
    auto& groups = share_groups();
    {
      const std::lock_guard<std::mutex> lock(groups.mutex);
      groups.ids.erase(group);
    }
    groups.generation.fetch_add(1, std::memory_order_acq_rel);
    forget_gl_share_group(assigned);
  });

  return assigned;
}

void delete_now(QOpenGLExtraFunctions& functions,
                DeferredGlObject kind,
                unsigned int name,
                void* sync) {
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
  case DeferredGlObject::Program:
    functions.glDeleteProgram(name);
    break;
  case DeferredGlObject::Shader:
    functions.glDeleteShader(name);
    break;
  case DeferredGlObject::Framebuffer:
    functions.glDeleteFramebuffers(1, &name);
    break;
  case DeferredGlObject::Renderbuffer:
    functions.glDeleteRenderbuffers(1, &name);
    break;
  case DeferredGlObject::Sampler:
    functions.glDeleteSamplers(1, &name);
    break;
  case DeferredGlObject::Query:
    functions.glDeleteQueries(1, &name);
    break;
  case DeferredGlObject::Sync:
    functions.glDeleteSync(static_cast<GLsync>(sync));
    break;
  }
  note_deleted(kind);
}

void enqueue(const PendingDelete& entry) noexcept {
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  if (entry.group != k_unknown_share_group && state.dead_groups.contains(entry.group)) {
    note_deleted(entry.kind);
    return;
  }
  state.pending.push_back(entry);
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
  struct CachedGroup {
    const QOpenGLContextGroup* group = nullptr;
    std::uint64_t generation = 0;
    GlShareGroup id = k_unknown_share_group;
  };
  thread_local CachedGroup cached;
  const QOpenGLContextGroup* group = context->shareGroup();
  const std::uint64_t generation =
      share_groups().generation.load(std::memory_order_acquire);
  if (group != nullptr && cached.group == group && cached.generation == generation) {
    return cached.id;
  }
  const GlShareGroup id = id_for(group);
  cached = CachedGroup{.group = group, .generation = generation, .id = id};
  return id;
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
  enqueue(PendingDelete{.kind = kind, .name = name, .group = group});
}

void forget_gl_share_group(GlShareGroup group) noexcept {
  if (group == k_unknown_share_group) {
    return;
  }
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  state.dead_groups.insert(group);
  std::vector<PendingDelete> kept;
  kept.reserve(state.pending.size());
  for (const auto& entry : state.pending) {
    if (entry.group != group) {
      kept.push_back(entry);
    } else {
      note_deleted(entry.kind);
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
    delete_now(functions, entry.kind, entry.name, entry.sync);
  }
}

void release_gl_object(DeferredGlObject kind,
                       unsigned int name,
                       GlShareGroup group) noexcept {
  if (name == 0U) {
    return;
  }
  if (!gl_objects_can_be_released(group)) {
    defer_gl_delete(kind, name, group);
    return;
  }
  QOpenGLExtraFunctions functions(QOpenGLContext::currentContext());
  functions.initializeOpenGLFunctions();
  delete_now(functions, kind, name, nullptr);
}

void release_gl_sync(void* sync, GlShareGroup group) noexcept {
  if (sync == nullptr) {
    return;
  }
  if (!gl_objects_can_be_released(group)) {
    enqueue(PendingDelete{
        .kind = DeferredGlObject::Sync, .name = 0U, .group = group, .sync = sync});
    return;
  }
  QOpenGLExtraFunctions functions(QOpenGLContext::currentContext());
  functions.initializeOpenGLFunctions();
  delete_now(functions, DeferredGlObject::Sync, 0U, sync);
}

auto gl_object_kind_name(DeferredGlObject kind) noexcept -> const char* {
  switch (kind) {
  case DeferredGlObject::Buffer:
    return "buffer";
  case DeferredGlObject::VertexArray:
    return "vertex_array";
  case DeferredGlObject::Texture:
    return "texture";
  case DeferredGlObject::Program:
    return "program";
  case DeferredGlObject::Shader:
    return "shader";
  case DeferredGlObject::Framebuffer:
    return "framebuffer";
  case DeferredGlObject::Renderbuffer:
    return "renderbuffer";
  case DeferredGlObject::Sampler:
    return "sampler";
  case DeferredGlObject::Query:
    return "query";
  case DeferredGlObject::Sync:
    return "sync";
  }
  return "unknown";
}

auto gl_object_census(DeferredGlObject kind) noexcept -> GlObjectCensus {
  const auto index = static_cast<std::size_t>(kind);
  auto& state = census();
  return GlObjectCensus{
      .created = state.created[index].load(std::memory_order_relaxed),
      .deleted = state.deleted[index].load(std::memory_order_relaxed),
  };
}

void note_gl_object_created(DeferredGlObject kind, std::size_t count) noexcept {
  census().created[static_cast<std::size_t>(kind)].fetch_add(count,
                                                             std::memory_order_relaxed);
}

auto deferred_gl_delete_count() noexcept -> std::size_t {
  auto& state = deferred();
  const std::lock_guard<std::mutex> lock(state.mutex);
  return state.pending.size();
}

} // namespace Render::GL
