#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include "../draw_queue.h"
#include "camera.h"
#include "resources.h"
#include <array>
#include <memory>
#include "shader.h"

namespace Render::GL {
// Shader included above

class Backend : protected QOpenGLFunctions_3_3_Core {
public:
    Backend() = default;
    ~Backend();
    void initialize();
    void beginFrame();
    void setViewport(int w, int h);
    void setClearColor(float r, float g, float b, float a);
    void execute(const DrawQueue& queue, const Camera& cam, const ResourceManager& res);

    void enableDepthTest(bool enable) {
        if (enable) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
    void setDepthFunc(GLenum func) { glDepthFunc(func); }
    void setDepthMask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

    void enableBlend(bool enable) {
        if (enable) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }
    void setBlendFunc(GLenum src, GLenum dst) { glBlendFunc(src, dst); }

    void enablePolygonOffset(bool enable) {
        if (enable) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);
    }
    void setPolygonOffset(float factor, float units) { glPolygonOffset(factor, units); }
private:
    int m_viewportWidth{0};
    int m_viewportHeight{0};
    std::array<float,4> m_clearColor{0.2f,0.3f,0.3f,0.0f};
    std::unique_ptr<Shader> m_basicShader;
    std::unique_ptr<Shader> m_gridShader;
};

} // namespace Render::GL
