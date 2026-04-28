#pragma once
#include <QOpenGLFunctions_3_3_Core>

namespace Render::GL {

struct DepthMaskScope {
  GLboolean prev{};
  explicit DepthMaskScope(bool enableWrite) {
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev);
    glDepthMask(enableWrite ? GL_TRUE : GL_FALSE);
  }
  ~DepthMaskScope() { glDepthMask(prev); }
};

struct PolygonOffsetScope {
  GLboolean prev_enable;
  float factor, units;
  PolygonOffsetScope(float f, float u)
      : prev_enable(glIsEnabled(GL_POLYGON_OFFSET_FILL)), factor(f), units(u) {

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(factor, units);
  }
  ~PolygonOffsetScope() {
    if (prev_enable == 0U) {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glPolygonOffset(0.0F, 0.0F);
  }
};

struct BlendScope {
  GLboolean prev_enable;
  BlendScope(bool enable = true) : prev_enable(glIsEnabled(GL_BLEND)) {

    if (enable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
  ~BlendScope() {
    if (prev_enable != 0U) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
};

struct CullFaceScope {
  GLboolean prev_enable;
  explicit CullFaceScope(bool enable) : prev_enable(glIsEnabled(GL_CULL_FACE)) {
    if (enable) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }
  ~CullFaceScope() {
    if (prev_enable != 0U) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }
};

struct DepthTestScope {
  GLboolean prev_enable;
  explicit DepthTestScope(bool enable)
      : prev_enable(glIsEnabled(GL_DEPTH_TEST)) {

    if (enable) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  ~DepthTestScope() {
    if (prev_enable != 0U) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
};
} // namespace Render::GL
