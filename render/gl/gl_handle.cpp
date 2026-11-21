#include "gl_handle.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include "gl_resource_tracking.h"
#include "platform_gl.h"
#include "render/profiling/asset_counters.h"

namespace Render::GL {

namespace {

void note_profiling_counters(DeferredGlObject kind) noexcept {
  switch (kind) {
  case DeferredGlObject::Buffer:
    note_buffers_created(1);
    break;
  case DeferredGlObject::VertexArray:
    note_vertex_arrays_created(1);
    break;
  case DeferredGlObject::Texture:
    note_textures_created(1);
    break;
  case DeferredGlObject::Program:
    Profiling::count_asset(Profiling::AssetCounter::GlProgramCreated);
    break;
  default:
    break;
  }
}

} // namespace

auto generate_gl_object(DeferredGlObject kind, unsigned int shader_type) -> unsigned int {
  QOpenGLContext* context = QOpenGLContext::currentContext();
  if (context == nullptr) {
    return 0U;
  }
  QOpenGLExtraFunctions* gl = context->extraFunctions();
  GLuint name = 0U;
  switch (kind) {
  case DeferredGlObject::Buffer:
    gl->glGenBuffers(1, &name);
    break;
  case DeferredGlObject::VertexArray:
    gl->glGenVertexArrays(1, &name);
    break;
  case DeferredGlObject::Texture:
    gl->glGenTextures(1, &name);
    break;
  case DeferredGlObject::Program:
    name = gl->glCreateProgram();
    break;
  case DeferredGlObject::Shader:
    name = gl->glCreateShader(static_cast<GLenum>(shader_type));
    break;
  case DeferredGlObject::Framebuffer:
    gl->glGenFramebuffers(1, &name);
    break;
  case DeferredGlObject::Renderbuffer:
    gl->glGenRenderbuffers(1, &name);
    break;
  case DeferredGlObject::Sampler:
    gl->glGenSamplers(1, &name);
    break;
  case DeferredGlObject::Query:
    gl->glGenQueries(1, &name);
    break;
  case DeferredGlObject::Sync:
    return 0U;
  }
  if (name != 0U) {
    note_gl_object_created(kind);
    note_profiling_counters(kind);
  }
  return name;
}

auto FenceHandle::insert() -> FenceHandle {
  FenceHandle fence;
  QOpenGLContext* context = QOpenGLContext::currentContext();
  if (context == nullptr) {
    return fence;
  }
  GLsync sync = context->extraFunctions()->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync != nullptr) {
    fence.m_sync = sync;
    fence.m_group = current_gl_share_group();
    note_gl_object_created(DeferredGlObject::Sync);
  }
  return fence;
}

} // namespace Render::GL
