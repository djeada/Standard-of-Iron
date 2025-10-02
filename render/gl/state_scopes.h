#pragma once
#include <QOpenGLFunctions_3_3_Core>
namespace Render::GL {
class GLStateScopes : protected QOpenGLFunctions_3_3_Core {
protected:
    GLStateScopes() { initializeOpenGLFunctions(); }
};

struct DepthMaskScope : GLStateScopes {
    GLboolean prev;
    explicit DepthMaskScope(bool enableWrite) {
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prev);
        glDepthMask(enableWrite ? GL_TRUE : GL_FALSE);
    }
    ~DepthMaskScope() { glDepthMask(prev); }
};

struct PolygonOffsetScope : GLStateScopes {
    GLboolean prevEnable;
    float factor, units;
    PolygonOffsetScope(float f, float u) : factor(f), units(u) {
        prevEnable = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(factor, units);
    }
    ~PolygonOffsetScope() {
        if (!prevEnable) glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f, 0.0f);
    }
};

struct BlendScope : GLStateScopes {
    GLboolean prevEnable;
    BlendScope(bool enable=true) {
        prevEnable = glIsEnabled(GL_BLEND);
        if (enable) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }
    ~BlendScope() {
        if (!prevEnable) glDisable(GL_BLEND);
    }
};
}
