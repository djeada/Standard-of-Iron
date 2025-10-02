#include "bootstrap.h"
#include "../scene_renderer.h"
#include "camera.h"
#include "resources.h"
#include <QOpenGLContext>
#include <QDebug>

namespace Render { namespace GL {

bool RenderBootstrap::initialize(Renderer& renderer,
                                 Camera& camera,
                                 std::shared_ptr<ResourceManager>& outResources) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "RenderBootstrap: no current valid OpenGL context";
        return false;
    }
    outResources = std::make_shared<ResourceManager>();
    renderer.setResources(outResources);
    if (!renderer.initialize()) {
        qWarning() << "RenderBootstrap: renderer initialize failed";
        return false;
    }
    renderer.setCamera(&camera);
    return true;
}

} } // namespace Render::GL
