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
  GLboolean prevEnable;
  float factor, units;
  PolygonOffsetScope(float f, float u)
      : prevEnable(glIsEnabled(GL_POLYGON_OFFSET_FILL)), factor(f), units(u) {

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(factor, units);
  }
  ~PolygonOffsetScope() {
    if (prevEnable == 0U) {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    glPolygonOffset(0.0F, 0.0F);
  }
};

struct BlendScope {
  GLboolean prevEnable;
  BlendScope(bool enable = true) : prevEnable(glIsEnabled(GL_BLEND)) {

    if (enable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
  ~BlendScope() {
    if (prevEnable != 0U) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
};

struct DepthTestScope {
  GLboolean prevEnable;
  explicit DepthTestScope(bool enable)
      : prevEnable(glIsEnabled(GL_DEPTH_TEST)) {

    if (enable) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  ~DepthTestScope() {
    if (prevEnable != 0U) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
};
} 
