#pragma once

#include "../mesh.h"
#include "../shader.h"
#include <QOpenGLFunctions_3_3_Core>
#include <cstddef>

namespace Render::GL {

class Backend;
class Shader;
struct Vertex;

namespace BackendImpl {

void initializeCylinderPipeline(Backend *backend);

void shutdownCylinderPipeline(Backend *backend);

void cacheCylinderUniforms(Backend *backend);

void uploadCylinderInstances(Backend *backend, std::size_t count);

void drawCylinders(Backend *backend, std::size_t count);

void initializeFogPipeline(Backend *backend);

void shutdownFogPipeline(Backend *backend);

void cacheFogUniforms(Backend *backend);

void uploadFogInstances(Backend *backend, std::size_t count);

void drawFog(Backend *backend, std::size_t count);

} // namespace BackendImpl
} // namespace Render::GL
